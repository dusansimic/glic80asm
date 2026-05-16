#include "lexer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void push(TokenList *tl, Token t) {
    if (tl->n == tl->cap) {
        tl->cap = tl->cap ? tl->cap * 2 : 16;
        tl->toks = realloc(tl->toks, tl->cap * sizeof(Token));
    }
    tl->toks[tl->n++] = t;
}

void tokens_free(TokenList *tl) {
    for (int i = 0; i < tl->n; i++) free(tl->toks[i].text);
    free(tl->toks);
    tl->toks = NULL;
    tl->n = tl->cap = 0;
}

static int is_id_start(int c) { return isalpha(c) || c == '_' || c == '.'; }
static int is_id_cont(int c)  { return isalnum(c) || c == '_' || c == '.'; }

static int parse_int(const char *s, int len, int base, int32_t *out) {
    int32_t v = 0;
    for (int i = 0; i < len; i++) {
        int d;
        char c = s[i];
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else return -1;
        if (d >= base) return -1;
        v = v * base + d;
    }
    *out = v;
    return 0;
}

int lex_line(const char *line, const char *filename, int line_no, TokenList *out) {
    out->toks = NULL;
    out->n = out->cap = 0;
    const char *p = line;
    while (*p) {
        if (*p == ';' || *p == '\n' || *p == '\r') break;
        if (isspace((unsigned char)*p)) { p++; continue; }
        Token t = {0};
        if (is_id_start((unsigned char)*p)) {
            const char *s = p;
            while (is_id_cont((unsigned char)*p)) p++;
            int len = (int)(p - s);
            /* allow trailing apostrophe for AF' */
            int has_apos = 0;
            if (*p == '\'' && len == 2 && (s[0] == 'A' || s[0] == 'a') && (s[1] == 'F' || s[1] == 'f')) {
                has_apos = 1;
                p++;
            }
            int total = len + (has_apos ? 1 : 0);
            char *buf = malloc(total + 1);
            for (int i = 0; i < len; i++) buf[i] = (char)toupper((unsigned char)s[i]);
            if (has_apos) buf[len] = '\'';
            buf[total] = 0;
            /* Hex-suffix numbers like 0FFh / 1234h are NOT identifiers; check */
            if (isdigit((unsigned char)s[0]) && total > 1 &&
                (buf[total-1] == 'H')) {
                /* try parse hex */
                int32_t v;
                if (parse_int(buf, total - 1, 16, &v) == 0) {
                    t.kind = TK_NUM;
                    t.num = v;
                    free(buf);
                    push(out, t);
                    continue;
                }
            }
            /* Plain decimal that happened to lex as ident (only digits)? */
            if (isdigit((unsigned char)s[0])) {
                int alldec = 1;
                for (int i = 0; i < total; i++) if (!isdigit((unsigned char)buf[i])) { alldec = 0; break; }
                if (alldec) {
                    int32_t v;
                    if (parse_int(buf, total, 10, &v) == 0) {
                        t.kind = TK_NUM;
                        t.num = v;
                        free(buf);
                        push(out, t);
                        continue;
                    }
                }
            }
            t.kind = TK_IDENT;
            t.text = buf;
            push(out, t);
            continue;
        }
        if (isdigit((unsigned char)*p)) {
            /* number: 0x..., 0FFh, or decimal */
            const char *s = p;
            if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
                p += 2;
                const char *h = p;
                while (isxdigit((unsigned char)*p)) p++;
                int32_t v;
                if (parse_int(h, (int)(p - h), 16, &v) < 0) {
                    fprintf(stderr, "%s:%d: bad hex literal\n", filename, line_no);
                    return -1;
                }
                t.kind = TK_NUM; t.num = v;
                push(out, t);
                continue;
            }
            while (isalnum((unsigned char)*p)) p++;
            int len = (int)(p - s);
            int32_t v;
            if (len > 1 && (s[len-1] == 'h' || s[len-1] == 'H')) {
                if (parse_int(s, len - 1, 16, &v) < 0) {
                    fprintf(stderr, "%s:%d: bad hex literal\n", filename, line_no);
                    return -1;
                }
            } else if (len > 1 && (s[len-1] == 'b' || s[len-1] == 'B')) {
                int ok = 1;
                for (int i = 0; i < len - 1; i++) if (s[i] != '0' && s[i] != '1') { ok = 0; break; }
                if (!ok || parse_int(s, len - 1, 2, &v) < 0) {
                    fprintf(stderr, "%s:%d: bad binary literal\n", filename, line_no);
                    return -1;
                }
            } else {
                if (parse_int(s, len, 10, &v) < 0) {
                    fprintf(stderr, "%s:%d: bad decimal literal\n", filename, line_no);
                    return -1;
                }
            }
            t.kind = TK_NUM; t.num = v;
            push(out, t);
            continue;
        }
        if (*p == '$') {
            /* could be hex $FF or PC marker $ */
            if (isxdigit((unsigned char)p[1])) {
                p++;
                const char *s = p;
                while (isxdigit((unsigned char)*p)) p++;
                int32_t v;
                if (parse_int(s, (int)(p - s), 16, &v) < 0) {
                    fprintf(stderr, "%s:%d: bad hex literal\n", filename, line_no);
                    return -1;
                }
                t.kind = TK_NUM; t.num = v;
                push(out, t);
                continue;
            }
            t.kind = TK_DOLLAR;
            push(out, t);
            p++;
            continue;
        }
        if (*p == '%') {
            if (p[1] == '0' || p[1] == '1') {
                p++;
                const char *s = p;
                while (*p == '0' || *p == '1') p++;
                int32_t v;
                if (parse_int(s, (int)(p - s), 2, &v) < 0) {
                    fprintf(stderr, "%s:%d: bad binary literal\n", filename, line_no);
                    return -1;
                }
                t.kind = TK_NUM; t.num = v;
                push(out, t);
                continue;
            }
            t.kind = TK_PERCENT;
            push(out, t);
            p++;
            continue;
        }
        if (*p == '"') {
            p++;
            const char *s = p;
            /* compute length and copy with escapes */
            char *buf = malloc(strlen(p) + 1);
            int bi = 0;
            while (*p && *p != '"') {
                char c = *p++;
                if (c == '\\' && *p) {
                    char e = *p++;
                    switch (e) {
                        case 'n': c = '\n'; break;
                        case 't': c = '\t'; break;
                        case 'r': c = '\r'; break;
                        case '0': c = '\0'; break;
                        case '\\': c = '\\'; break;
                        case '"': c = '"'; break;
                        case '\'': c = '\''; break;
                        default: c = e; break;
                    }
                }
                buf[bi++] = c;
            }
            if (*p != '"') {
                fprintf(stderr, "%s:%d: unterminated string\n", filename, line_no);
                free(buf);
                return -1;
            }
            p++;
            buf[bi] = 0;
            (void)s;
            t.kind = TK_STR;
            t.text = buf;
            t.tlen = bi;
            push(out, t);
            continue;
        }
        if (*p == '\'') {
            p++;
            if (*p == 0 || *p == '\'') {
                fprintf(stderr, "%s:%d: bad char literal\n", filename, line_no);
                return -1;
            }
            int32_t v;
            char c = *p++;
            if (c == '\\' && *p) {
                char e = *p++;
                switch (e) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'r': c = '\r'; break;
                    case '0': c = 0; break;
                    case '\\': c = '\\'; break;
                    case '\'': c = '\''; break;
                    case '"': c = '"'; break;
                    default: c = e; break;
                }
            }
            v = (unsigned char)c;
            if (*p != '\'') {
                fprintf(stderr, "%s:%d: unterminated char literal\n", filename, line_no);
                return -1;
            }
            p++;
            t.kind = TK_NUM;
            t.num = v;
            push(out, t);
            continue;
        }
        switch (*p) {
            case ',': t.kind = TK_COMMA;  p++; push(out, t); continue;
            case '(': t.kind = TK_LPAREN; p++; push(out, t); continue;
            case ')': t.kind = TK_RPAREN; p++; push(out, t); continue;
            case ':': t.kind = TK_COLON;  p++; push(out, t); continue;
            case '+': t.kind = TK_PLUS;   p++; push(out, t); continue;
            case '-': t.kind = TK_MINUS;  p++; push(out, t); continue;
            case '*': t.kind = TK_STAR;   p++; push(out, t); continue;
            case '/': t.kind = TK_SLASH;  p++; push(out, t); continue;
            case '&': t.kind = TK_AMP;    p++; push(out, t); continue;
            case '|': t.kind = TK_PIPE;   p++; push(out, t); continue;
            case '^': t.kind = TK_CARET;  p++; push(out, t); continue;
            case '~': t.kind = TK_TILDE;  p++; push(out, t); continue;
            case '<':
                if (p[1] == '<') { t.kind = TK_SHL; p += 2; push(out, t); continue; }
                fprintf(stderr, "%s:%d: unexpected '<'\n", filename, line_no);
                return -1;
            case '>':
                if (p[1] == '>') { t.kind = TK_SHR; p += 2; push(out, t); continue; }
                fprintf(stderr, "%s:%d: unexpected '>'\n", filename, line_no);
                return -1;
        }
        fprintf(stderr, "%s:%d: stray character '%c'\n", filename, line_no, *p);
        return -1;
    }
    Token eol = { .kind = TK_EOL };
    push(out, eol);
    return 0;
}
