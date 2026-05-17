#include "expr.h"
#include "symtab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* Forward decls. */
static int p_or  (AsmCtx *, Token *, int *, int32_t *, int *);
static int p_xor (AsmCtx *, Token *, int *, int32_t *, int *);
static int p_and (AsmCtx *, Token *, int *, int32_t *, int *);
static int p_shift(AsmCtx *, Token *, int *, int32_t *, int *);
static int p_add (AsmCtx *, Token *, int *, int32_t *, int *);
static int p_mul (AsmCtx *, Token *, int *, int32_t *, int *);
static int p_unary(AsmCtx *, Token *, int *, int32_t *, int *);
static int p_primary(AsmCtx *, Token *, int *, int32_t *, int *);

int expr_parse(AsmCtx *ctx, Token *toks, int *cur, int32_t *out, int *res) {
    *res = 1;
    return p_or(ctx, toks, cur, out, res);
}

#define BIN(LOWER, HIGHER, MATCH_BLOCK)                                   \
    do {                                                                  \
        int32_t l, r;                                                     \
        int lr, rr;                                                       \
        if (HIGHER(ctx, toks, cur, &l, &lr) < 0) return -1;               \
        for (;;) {                                                        \
            MATCH_BLOCK                                                   \
            else break;                                                   \
        }                                                                 \
        *out = l;                                                         \
        *res = lr;                                                        \
        return 0;                                                         \
    } while (0)

static int p_or(AsmCtx *ctx, Token *toks, int *cur, int32_t *out, int *res) {
    int32_t l, r; int lr, rr;
    if (p_xor(ctx, toks, cur, &l, &lr) < 0) return -1;
    while (toks[*cur].kind == TK_PIPE) {
        (*cur)++;
        if (p_xor(ctx, toks, cur, &r, &rr) < 0) return -1;
        l |= r; lr &= rr;
    }
    *out = l; *res = lr;
    return 0;
}

static int p_xor(AsmCtx *ctx, Token *toks, int *cur, int32_t *out, int *res) {
    int32_t l, r; int lr, rr;
    if (p_and(ctx, toks, cur, &l, &lr) < 0) return -1;
    while (toks[*cur].kind == TK_CARET) {
        (*cur)++;
        if (p_and(ctx, toks, cur, &r, &rr) < 0) return -1;
        l ^= r; lr &= rr;
    }
    *out = l; *res = lr;
    return 0;
}

static int p_and(AsmCtx *ctx, Token *toks, int *cur, int32_t *out, int *res) {
    int32_t l, r; int lr, rr;
    if (p_shift(ctx, toks, cur, &l, &lr) < 0) return -1;
    while (toks[*cur].kind == TK_AMP) {
        (*cur)++;
        if (p_shift(ctx, toks, cur, &r, &rr) < 0) return -1;
        l &= r; lr &= rr;
    }
    *out = l; *res = lr;
    return 0;
}

static int p_shift(AsmCtx *ctx, Token *toks, int *cur, int32_t *out, int *res) {
    int32_t l, r; int lr, rr;
    if (p_add(ctx, toks, cur, &l, &lr) < 0) return -1;
    while (toks[*cur].kind == TK_SHL || toks[*cur].kind == TK_SHR) {
        TokKind k = toks[(*cur)++].kind;
        if (p_add(ctx, toks, cur, &r, &rr) < 0) return -1;
        if (k == TK_SHL) l <<= r; else l = (int32_t)((uint32_t)l >> r);
        lr &= rr;
    }
    *out = l; *res = lr;
    return 0;
}

static int p_add(AsmCtx *ctx, Token *toks, int *cur, int32_t *out, int *res) {
    int32_t l, r; int lr, rr;
    if (p_mul(ctx, toks, cur, &l, &lr) < 0) return -1;
    while (toks[*cur].kind == TK_PLUS || toks[*cur].kind == TK_MINUS) {
        TokKind k = toks[(*cur)++].kind;
        if (p_mul(ctx, toks, cur, &r, &rr) < 0) return -1;
        if (k == TK_PLUS) l += r; else l -= r;
        lr &= rr;
    }
    *out = l; *res = lr;
    return 0;
}

static int p_mul(AsmCtx *ctx, Token *toks, int *cur, int32_t *out, int *res) {
    int32_t l, r; int lr, rr;
    if (p_unary(ctx, toks, cur, &l, &lr) < 0) return -1;
    while (toks[*cur].kind == TK_STAR || toks[*cur].kind == TK_SLASH || toks[*cur].kind == TK_PERCENT) {
        TokKind k = toks[(*cur)++].kind;
        if (p_unary(ctx, toks, cur, &r, &rr) < 0) return -1;
        if (k == TK_STAR) l *= r;
        else if (k == TK_SLASH) { if (r == 0) { asm_error(ctx, "div by zero"); return -1; } l /= r; }
        else { if (r == 0) { asm_error(ctx, "mod by zero"); return -1; } l %= r; }
        lr &= rr;
    }
    *out = l; *res = lr;
    return 0;
}

static int p_unary(AsmCtx *ctx, Token *toks, int *cur, int32_t *out, int *res) {
    /* SDCC '#' immediate prefix — no-op, just skip. */
    if (ctx->ext_sdcc && toks[*cur].kind == TK_HASH) {
        (*cur)++;
        return p_unary(ctx, toks, cur, out, res);
    }
    if (toks[*cur].kind == TK_PLUS) { (*cur)++; return p_unary(ctx, toks, cur, out, res); }
    if (toks[*cur].kind == TK_MINUS) {
        (*cur)++;
        int32_t v; int r;
        if (p_unary(ctx, toks, cur, &v, &r) < 0) return -1;
        *out = -v; *res = r;
        return 0;
    }
    if (toks[*cur].kind == TK_TILDE) {
        (*cur)++;
        int32_t v; int r;
        if (p_unary(ctx, toks, cur, &v, &r) < 0) return -1;
        *out = ~v; *res = r;
        return 0;
    }
    if (ctx->ext_sdcc && toks[*cur].kind == TK_LT) {
        (*cur)++;
        int32_t v; int r;
        if (p_unary(ctx, toks, cur, &v, &r) < 0) return -1;
        *out = v & 0xFF; *res = r;
        return 0;
    }
    if (ctx->ext_sdcc && toks[*cur].kind == TK_GT) {
        (*cur)++;
        int32_t v; int r;
        if (p_unary(ctx, toks, cur, &v, &r) < 0) return -1;
        *out = (v >> 8) & 0xFF; *res = r;
        return 0;
    }
    return p_primary(ctx, toks, cur, out, res);
}

/* Register names reserved — cannot be used as labels in expressions. */
static int is_reserved_name(const char *s) {
    static const char *R[] = {
        "A","B","C","D","E","H","L","I","R",
        "AF","AF'","BC","DE","HL","SP","IX","IY",
        "NZ","Z","NC","PO","PE","P","M",
        NULL
    };
    for (int i = 0; R[i]; i++) if (strcmp(R[i], s) == 0) return 1;
    return 0;
}

static int p_primary(AsmCtx *ctx, Token *toks, int *cur, int32_t *out, int *res) {
    Token *t = &toks[*cur];
    *res = 1;
    if (t->kind == TK_NUM) {
        (*cur)++;
        *out = t->num;
        return 0;
    }
    if (t->kind == TK_DOLLAR) {
        (*cur)++;
        *out = ctx->pc;
        return 0;
    }
    if (t->kind == TK_LPAREN) {
        (*cur)++;
        if (p_or(ctx, toks, cur, out, res) < 0) return -1;
        if (toks[*cur].kind != TK_RPAREN) { asm_error(ctx, "expected ')'"); return -1; }
        (*cur)++;
        return 0;
    }
    if (t->kind == TK_IDENT) {
        if (is_reserved_name(t->text)) {
            asm_error(ctx, "reserved name '%s' in expression", t->text);
            return -1;
        }
        (*cur)++;
        /* Local label reference: prepend parent label.
           - Dot-prefixed names: always local.
           - Digit-prefixed names (SDCC numeric labels): local only under -ec. */
        const char *name = t->text;
        char *qbuf = NULL;
        int is_dot   = (name[0] == '.');
        int is_digit = (ctx->ext_sdcc && isdigit((unsigned char)name[0]));
        if (is_dot || is_digit) {
            if (!ctx->last_label || !ctx->last_label[0]) {
                asm_error(ctx, "local label '%s' has no parent", name);
                return -1;
            }
            size_t la = strlen(ctx->last_label);
            size_t ln = strlen(name);
            qbuf = malloc(la + ln + 2);
            memcpy(qbuf, ctx->last_label, la);
            size_t off = la;
            if (is_digit) qbuf[off++] = '.';   /* match parser's separator */
            memcpy(qbuf + off, name, ln + 1);
            name = qbuf;
        }
        Sym *s = symtab_find(ctx->syms, name);
        free(qbuf);
        if (!s || !s->defined) {
            *out = 0;
            *res = 0;
            if (ctx->pass == PASS2) {
                asm_error(ctx, "undefined symbol '%s'", t->text);
                return -1;
            }
            return 0;
        }
        *out = s->value;
        return 0;
    }
    asm_error(ctx, "expected expression");
    return -1;
}
