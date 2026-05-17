#include "parser.h"
#include "lexer.h"
#include "expr.h"
#include "encoder.h"
#include "symtab.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Returns 1 if name is a directive (including SDCC ones when -ec). */
static int is_directive(const char *n) {
    return !strcmp(n, "ORG") || !strcmp(n, "FORG") ||
           !strcmp(n, "DB") || !strcmp(n, "DW") ||
           !strcmp(n, "DS")  || !strcmp(n, "EQU") || !strcmp(n, "END") ||
           !strcmp(n, "DEFB") || !strcmp(n, "DEFW") || !strcmp(n, "DEFS") ||
           !strcmp(n, ".DB") || !strcmp(n, ".DW") || !strcmp(n, ".DS") ||
           !strcmp(n, ".ASCII") || !strcmp(n, ".ASCIZ") ||
           !strcmp(n, ".MODULE") || !strcmp(n, ".OPTSDCC") ||
           !strcmp(n, ".GLOBL")  || !strcmp(n, ".AREA");
}

/* A name is "local" if it should be scoped under the previous non-local label.
   Dot-prefixed always; digit-prefixed only under -ec (SDCC numeric labels). */
static int is_local_name(AsmCtx *ctx, const char *n) {
    if (n[0] == '.') return 1;
    if (ctx->ext_sdcc && isdigit((unsigned char)n[0])) return 1;
    return 0;
}

/* Compose qualified name for a local label -> "<parent><name>".
   Returns malloc'd string the caller must free. */
static char *qualify_label(AsmCtx *ctx, const char *name) {
    if (!is_local_name(ctx, name)) {
        char *d = malloc(strlen(name) + 1);
        strcpy(d, name);
        return d;
    }
    if (!ctx->last_label || !ctx->last_label[0]) {
        asm_error(ctx, "local label '%s' has no parent", name);
        return NULL;
    }
    size_t la = strlen(ctx->last_label);
    size_t ln = strlen(name);
    char *d = malloc(la + ln + 2);
    memcpy(d, ctx->last_label, la);
    /* For digit-prefixed SDCC labels, insert a separator so e.g.
       "_MAIN" + "00104$" -> "_MAIN.00104$" stays unambiguous. */
    size_t off = la;
    if (ctx->ext_sdcc && isdigit((unsigned char)name[0])) {
        d[off++] = '.';
    }
    memcpy(d + off, name, ln + 1);
    return d;
}

/* Define a label = current pc. Updates ctx->last_label for non-local labels. */
static int define_label(AsmCtx *ctx, const char *name) {
    char *qname = qualify_label(ctx, name);
    if (!qname) return -1;
    if (!is_local_name(ctx, name)) {
        free(ctx->last_label);
        ctx->last_label = malloc(strlen(name) + 1);
        strcpy(ctx->last_label, name);
    }
    if (ctx->pass == PASS1) {
        if (symtab_define(ctx->syms, qname, ctx->pc) < 0) {
            asm_error(ctx, "duplicate label '%s'", qname);
            free(qname);
            return -1;
        }
    }
    free(qname);
    return 0;
}

static int do_org(AsmCtx *ctx, Token *toks, int *cur) {
    int32_t v; int rsv;
    if (expr_parse(ctx, toks, cur, &v, &rsv) < 0) return -1;
    if (!rsv) { asm_error(ctx, "ORG needs resolved expression in pass 1"); return -1; }
    ctx->pc = (uint16_t)v;
    if (!ctx->origin_set) { ctx->origin = ctx->pc; ctx->origin_set = 1; }
    return 0;
}

static int do_db(AsmCtx *ctx, Token *toks, int *cur) {
    for (;;) {
        if (toks[*cur].kind == TK_STR) {
            int tlen = toks[*cur].tlen;
            const char *s = toks[*cur].text;
            for (int i = 0; i < tlen; i++) emit_byte(ctx, (uint8_t)s[i]);
            (*cur)++;
        } else {
            int32_t v; int rsv;
            if (expr_parse(ctx, toks, cur, &v, &rsv) < 0) return -1;
            if (ctx->pass == PASS2) {
                if (v < -128 || v > 255) { asm_error(ctx, "DB value out of byte range: %d", v); return -1; }
            }
            emit_byte(ctx, (uint8_t)(v & 0xFF));
        }
        if (toks[*cur].kind == TK_COMMA) { (*cur)++; continue; }
        break;
    }
    if (toks[*cur].kind != TK_EOL) { asm_error(ctx, "trailing garbage after DB"); return -1; }
    return 0;
}

static int do_dw(AsmCtx *ctx, Token *toks, int *cur) {
    for (;;) {
        int32_t v; int rsv;
        if (expr_parse(ctx, toks, cur, &v, &rsv) < 0) return -1;
        if (ctx->pass == PASS2) {
            if (v < -32768 || v > 65535) { asm_error(ctx, "DW value out of word range: %d", v); return -1; }
        }
        emit_word(ctx, (uint16_t)(v & 0xFFFF));
        if (toks[*cur].kind == TK_COMMA) { (*cur)++; continue; }
        break;
    }
    if (toks[*cur].kind != TK_EOL) { asm_error(ctx, "trailing garbage after DW"); return -1; }
    return 0;
}

/* SDCC `.ascii "..."` (raw bytes) and `.asciz "..."` (null-terminated). */
static int do_ascii(AsmCtx *ctx, Token *toks, int *cur, int zero_term) {
    if (toks[*cur].kind != TK_STR) {
        asm_error(ctx, ".ascii expects a string literal");
        return -1;
    }
    while (toks[*cur].kind == TK_STR) {
        int n = toks[*cur].tlen;
        const char *s = toks[*cur].text;
        for (int i = 0; i < n; i++) emit_byte(ctx, (uint8_t)s[i]);
        (*cur)++;
        if (toks[*cur].kind == TK_COMMA) { (*cur)++; continue; }
        break;
    }
    if (zero_term) emit_byte(ctx, 0);
    if (toks[*cur].kind != TK_EOL) {
        asm_error(ctx, "trailing garbage after .ascii");
        return -1;
    }
    return 0;
}

static int do_ds(AsmCtx *ctx, Token *toks, int *cur) {
    int32_t v; int rsv;
    if (expr_parse(ctx, toks, cur, &v, &rsv) < 0) return -1;
    if (!rsv) { asm_error(ctx, "DS needs resolved expression in pass 1"); return -1; }
    if (v < 0) { asm_error(ctx, "DS negative count"); return -1; }
    int32_t fill = 0;
    if (toks[*cur].kind == TK_COMMA) {
        (*cur)++;
        int rsv2;
        if (expr_parse(ctx, toks, cur, &fill, &rsv2) < 0) return -1;
    }
    if (toks[*cur].kind != TK_EOL) { asm_error(ctx, "trailing garbage after DS"); return -1; }
    for (int32_t i = 0; i < v; i++) emit_byte(ctx, (uint8_t)(fill & 0xFF));
    return 0;
}

int parse_line(AsmCtx *ctx, const char *line) {
    TokenList tl;
    if (lex_line(line, ctx->filename, ctx->line_no,
                 ctx->ext_escapes, ctx->ext_sdcc, &tl) < 0) {
        ctx->errors++;
        return -1;
    }
    Token *t = tl.toks;
    int cur = 0;

    if (t[0].kind == TK_EOL) { tokens_free(&tl); return 0; }

    /* "IDENT EQU expr"  (no colon) */
    if (t[0].kind == TK_IDENT && t[1].kind == TK_IDENT && !strcmp(t[1].text, "EQU")) {
        if (is_mnemonic(t[0].text) || is_directive(t[0].text)) {
            asm_error(ctx, "cannot use reserved name '%s' as label", t[0].text);
            tokens_free(&tl);
            return -1;
        }
        if (ctx->pass == PASS1) {
            if (symtab_find(ctx->syms, t[0].text)) {
                asm_error(ctx, "duplicate symbol '%s'", t[0].text);
                tokens_free(&tl);
                return -1;
            }
            cur = 2;
            int32_t v; int rsv;
            if (expr_parse(ctx, t, &cur, &v, &rsv) < 0) { tokens_free(&tl); return -1; }
            symtab_define(ctx->syms, t[0].text, v);
            if (!rsv) {
                Sym *p = symtab_find(ctx->syms, t[0].text);
                p->defined = 0;
            }
            if (t[cur].kind != TK_EOL) { asm_error(ctx, "trailing garbage after EQU"); tokens_free(&tl); return -1; }
        } else {
            cur = 2;
            int32_t v; int rsv;
            if (expr_parse(ctx, t, &cur, &v, &rsv) < 0) { tokens_free(&tl); return -1; }
            Sym *s = symtab_find(ctx->syms, t[0].text);
            if (s) { s->value = v; s->defined = 1; }
            if (t[cur].kind != TK_EOL) { asm_error(ctx, "trailing garbage after EQU"); tokens_free(&tl); return -1; }
        }
        tokens_free(&tl);
        return 0;
    }

    /* Label with colon. */
    if (t[0].kind == TK_IDENT && t[1].kind == TK_COLON) {
        if (is_mnemonic(t[0].text) || is_directive(t[0].text)) {
            asm_error(ctx, "cannot use reserved name '%s' as label", t[0].text);
            tokens_free(&tl);
            return -1;
        }
        /* "label: EQU expr" form — bind label = expr, not PC. */
        if (t[2].kind == TK_IDENT && !strcmp(t[2].text, "EQU")) {
            if (ctx->pass == PASS1) {
                if (symtab_find(ctx->syms, t[0].text)) {
                    asm_error(ctx, "duplicate symbol '%s'", t[0].text);
                    tokens_free(&tl);
                    return -1;
                }
                cur = 3;
                int32_t v; int rsv;
                if (expr_parse(ctx, t, &cur, &v, &rsv) < 0) { tokens_free(&tl); return -1; }
                symtab_define(ctx->syms, t[0].text, v);
                if (!rsv) {
                    Sym *p = symtab_find(ctx->syms, t[0].text);
                    p->defined = 0;
                }
            } else {
                cur = 3;
                int32_t v; int rsv;
                if (expr_parse(ctx, t, &cur, &v, &rsv) < 0) { tokens_free(&tl); return -1; }
                Sym *s = symtab_find(ctx->syms, t[0].text);
                if (s) { s->value = v; s->defined = 1; }
            }
            if (t[cur].kind != TK_EOL) { asm_error(ctx, "trailing garbage after EQU"); tokens_free(&tl); return -1; }
            tokens_free(&tl);
            return 0;
        }
        if (define_label(ctx, t[0].text) < 0) { tokens_free(&tl); return -1; }
        cur = 2;
        /* SDCC export form: `label::` — second colon is just visibility, skip it. */
        if (ctx->ext_sdcc && t[cur].kind == TK_COLON) cur++;
    }

    if (t[cur].kind == TK_EOL) { tokens_free(&tl); return 0; }

    if (t[cur].kind != TK_IDENT) {
        asm_error(ctx, "expected mnemonic or directive");
        tokens_free(&tl);
        return -1;
    }

    const char *m = t[cur].text;
    if (!strcmp(m, "ORG") || !strcmp(m, "FORG")) { cur++; int r = do_org(ctx, t, &cur); tokens_free(&tl); return r; }
    if (!strcmp(m, "DB") || !strcmp(m, "DEFB")) { cur++; int r = do_db(ctx, t, &cur); tokens_free(&tl); return r; }
    if (!strcmp(m, "DW") || !strcmp(m, "DEFW")) { cur++; int r = do_dw(ctx, t, &cur); tokens_free(&tl); return r; }
    if (!strcmp(m, "DS") || !strcmp(m, "DEFS")) { cur++; int r = do_ds(ctx, t, &cur); tokens_free(&tl); return r; }
    if (!strcmp(m, "END")) { ctx->end_seen = 1; tokens_free(&tl); return 0; }
    if (ctx->ext_sdcc) {
        /* SDCC no-op directives: linker metadata we don't model. */
        if (!strcmp(m, ".MODULE") || !strcmp(m, ".OPTSDCC") ||
            !strcmp(m, ".GLOBL")  || !strcmp(m, ".AREA")) {
            tokens_free(&tl);
            return 0;
        }
        /* SDCC data aliases. */
        if (!strcmp(m, ".DB")) { cur++; int r = do_db(ctx, t, &cur); tokens_free(&tl); return r; }
        if (!strcmp(m, ".DW")) { cur++; int r = do_dw(ctx, t, &cur); tokens_free(&tl); return r; }
        if (!strcmp(m, ".DS")) { cur++; int r = do_ds(ctx, t, &cur); tokens_free(&tl); return r; }
        if (!strcmp(m, ".ASCII")) { cur++; int r = do_ascii(ctx, t, &cur, 0); tokens_free(&tl); return r; }
        if (!strcmp(m, ".ASCIZ")) { cur++; int r = do_ascii(ctx, t, &cur, 1); tokens_free(&tl); return r; }
    }
    if (!strcmp(m, "EQU")) {
        asm_error(ctx, "EQU requires a label name");
        tokens_free(&tl);
        return -1;
    }

    int r = encode_instruction(ctx, t, &cur);
    tokens_free(&tl);
    return r;
}
