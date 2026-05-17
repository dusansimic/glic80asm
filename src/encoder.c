#include "encoder.h"
#include "expr.h"
#include "symtab.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Operand kinds (encoder-private). */
enum {
    OT_NONE = 0,
    OT_R8,        /* 8-bit reg A/B/C/D/E/H/L; val = reg code (B=0,C=1,D=2,E=3,H=4,L=5,A=7) */
    OT_HL_IND,    /* (HL) */
    OT_BC_IND,    /* (BC) */
    OT_DE_IND,    /* (DE) */
    OT_SP_IND,    /* (SP) */
    OT_IX_IND,    /* (IX) bare, no disp */
    OT_IY_IND,    /* (IY) bare, no disp */
    OT_IX_D,      /* (IX+disp) */
    OT_IY_D,      /* (IY+disp) */
    OT_C_IND,     /* (C) */
    OT_BC, OT_DE, OT_HL, OT_SP, OT_AF, OT_AFP, OT_IX, OT_IY,
    OT_I, OT_R_REG,
    OT_IMM,
    OT_MEM
};

typedef struct {
    int     type;
    int     reg;
    int32_t val;
    int32_t disp;
    int     val_known;
    int     disp_known;
} Op;

/* --- name lookups --- */
static int reg8_code(const char *n) {
    if (!n) return -1;
    if (n[1] != 0) return -1;
    switch (n[0]) {
        case 'B': return 0;
        case 'C': return 1;
        case 'D': return 2;
        case 'E': return 3;
        case 'H': return 4;
        case 'L': return 5;
        case 'A': return 7;
    }
    return -1;
}

static int cond_code(const char *n) {
    if (!n) return -1;
    if (!strcmp(n, "NZ")) return 0;
    if (!strcmp(n, "Z"))  return 1;
    if (!strcmp(n, "NC")) return 2;
    if (!strcmp(n, "C"))  return 3;
    if (!strcmp(n, "PO")) return 4;
    if (!strcmp(n, "PE")) return 5;
    if (!strcmp(n, "P"))  return 6;
    if (!strcmp(n, "M"))  return 7;
    return -1;
}

/* JR-allowed conditions only (NZ,Z,NC,C). */
static int jr_cond_code(const char *n) {
    if (!n) return -1;
    if (!strcmp(n, "NZ")) return 0;
    if (!strcmp(n, "Z"))  return 1;
    if (!strcmp(n, "NC")) return 2;
    if (!strcmp(n, "C"))  return 3;
    return -1;
}

/* --- operand parser --- */
static int parse_op(AsmCtx *ctx, Token *toks, int *cur, Op *op) {
    memset(op, 0, sizeof *op);
    op->val_known = 1;
    op->disp_known = 1;
    Token *t = &toks[*cur];

    if (t->kind == TK_LPAREN) {
        (*cur)++;
        Token *inner = &toks[*cur];
        if (inner->kind == TK_IDENT) {
            const char *n = inner->text;
            int r;
            if (!strcmp(n, "C")  && toks[*cur + 1].kind == TK_RPAREN) {
                op->type = OT_C_IND;
                *cur += 2;
                return 0;
            }
            if (!strcmp(n, "BC") && toks[*cur + 1].kind == TK_RPAREN) {
                op->type = OT_BC_IND; *cur += 2; return 0;
            }
            if (!strcmp(n, "DE") && toks[*cur + 1].kind == TK_RPAREN) {
                op->type = OT_DE_IND; *cur += 2; return 0;
            }
            if (!strcmp(n, "HL") && toks[*cur + 1].kind == TK_RPAREN) {
                op->type = OT_HL_IND; *cur += 2; return 0;
            }
            if (!strcmp(n, "SP") && toks[*cur + 1].kind == TK_RPAREN) {
                op->type = OT_SP_IND; *cur += 2; return 0;
            }
            if (!strcmp(n, "IX") || !strcmp(n, "IY")) {
                int ix = (n[1] == 'X');
                (*cur)++; /* skip IX/IY */
                if (toks[*cur].kind == TK_RPAREN) {
                    op->type = ix ? OT_IX_IND : OT_IY_IND;
                    (*cur)++;
                    return 0;
                }
                if (toks[*cur].kind == TK_PLUS) {
                    (*cur)++;
                    int32_t v; int rsv;
                    if (expr_parse(ctx, toks, cur, &v, &rsv) < 0) return -1;
                    if (toks[*cur].kind != TK_RPAREN) { asm_error(ctx, "expected ')'"); return -1; }
                    (*cur)++;
                    op->type = ix ? OT_IX_D : OT_IY_D;
                    op->disp = v;
                    op->disp_known = rsv;
                    return 0;
                }
                if (toks[*cur].kind == TK_MINUS) {
                    /* leave the '-' for expr to consume as unary */
                    int32_t v; int rsv;
                    if (expr_parse(ctx, toks, cur, &v, &rsv) < 0) return -1;
                    if (toks[*cur].kind != TK_RPAREN) { asm_error(ctx, "expected ')'"); return -1; }
                    (*cur)++;
                    op->type = ix ? OT_IX_D : OT_IY_D;
                    op->disp = v;
                    op->disp_known = rsv;
                    return 0;
                }
                asm_error(ctx, "expected '+', '-' or ')' after I%c", ix ? 'X' : 'Y');
                return -1;
            }
            (void)r;
        }
        /* fall through: parse an arbitrary expression -> OT_MEM */
        int32_t v; int rsv;
        if (expr_parse(ctx, toks, cur, &v, &rsv) < 0) return -1;
        if (toks[*cur].kind != TK_RPAREN) { asm_error(ctx, "expected ')'"); return -1; }
        (*cur)++;
        op->type = OT_MEM;
        op->val = v;
        op->val_known = rsv;
        return 0;
    }

    if (t->kind == TK_IDENT) {
        const char *n = t->text;
        int r = reg8_code(n);
        if (r >= 0) { op->type = OT_R8; op->reg = r; (*cur)++; return 0; }
        if (!strcmp(n, "BC")) { op->type = OT_BC;  (*cur)++; return 0; }
        if (!strcmp(n, "DE")) { op->type = OT_DE;  (*cur)++; return 0; }
        if (!strcmp(n, "HL")) { op->type = OT_HL;  (*cur)++; return 0; }
        if (!strcmp(n, "SP")) { op->type = OT_SP;  (*cur)++; return 0; }
        if (!strcmp(n, "AF")) { op->type = OT_AF;  (*cur)++; return 0; }
        if (!strcmp(n, "AF'")){ op->type = OT_AFP; (*cur)++; return 0; }
        if (!strcmp(n, "IX")) { op->type = OT_IX;  (*cur)++; return 0; }
        if (!strcmp(n, "IY")) { op->type = OT_IY;  (*cur)++; return 0; }
        if (!strcmp(n, "I"))  { op->type = OT_I;   (*cur)++; return 0; }
        if (!strcmp(n, "R"))  { op->type = OT_R_REG; (*cur)++; return 0; }
        /* fall through to expression */
    }

    int32_t v; int rsv;
    if (expr_parse(ctx, toks, cur, &v, &rsv) < 0) return -1;
    op->type = OT_IMM;
    op->val = v;
    op->val_known = rsv;

    /* SDCC indexed: `disp (ix)` / `disp (iy)` is equivalent to `(ix+disp)`. */
    if (ctx->ext_sdcc &&
        toks[*cur].kind == TK_LPAREN &&
        toks[*cur + 1].kind == TK_IDENT &&
        toks[*cur + 2].kind == TK_RPAREN) {
        const char *n = toks[*cur + 1].text;
        if (!strcmp(n, "IX") || !strcmp(n, "IY")) {
            int isiy = (n[1] == 'Y');
            *cur += 3;
            op->type      = isiy ? OT_IY_D : OT_IX_D;
            op->disp      = op->val;
            op->disp_known = op->val_known;
            op->val       = 0;
            op->val_known = 1;
        }
    }
    return 0;
}

static int expect_comma(AsmCtx *ctx, Token *toks, int *cur) {
    if (toks[*cur].kind != TK_COMMA) { asm_error(ctx, "expected ','"); return -1; }
    (*cur)++;
    return 0;
}

static int expect_eol(AsmCtx *ctx, Token *toks, int *cur) {
    if (toks[*cur].kind != TK_EOL) { asm_error(ctx, "trailing garbage"); return -1; }
    return 0;
}

/* Range checks. */
static int chk_byte(AsmCtx *ctx, int32_t v, const char *what) {
    if (ctx->pass != PASS2) return 0;
    if (v < -128 || v > 255) { asm_error(ctx, "%s out of range: %d", what, v); return -1; }
    return 0;
}
static int chk_disp(AsmCtx *ctx, int32_t v) {
    if (ctx->pass != PASS2) return 0;
    if (v < -128 || v > 127) { asm_error(ctx, "IX/IY displacement out of range: %d", v); return -1; }
    return 0;
}
static int chk_word(AsmCtx *ctx, int32_t v, const char *what) {
    if (ctx->pass != PASS2) return 0;
    if (v < -32768 || v > 65535) { asm_error(ctx, "%s out of range: %d", what, v); return -1; }
    return 0;
}
static int chk_bit(AsmCtx *ctx, int32_t v) {
    if (ctx->pass != PASS2) return 0;
    if (v < 0 || v > 7) { asm_error(ctx, "bit index out of range: %d", v); return -1; }
    return 0;
}

/* ---------------- instruction handlers ---------------- */

/* ALU group: ADD/ADC/SUB/SBC/AND/OR/XOR/CP. Each has a base for the reg form. */
static int do_alu(AsmCtx *ctx, Token *toks, int *cur,
                  const char *mnem, int base_r, int base_n, int wants_a)
{
    Op a, b;
    if (parse_op(ctx, toks, cur, &a) < 0) return -1;
    Op *src = &a;
    int has_a = 0;
    if (a.type == OT_R8 && a.reg == 7 && toks[*cur].kind == TK_COMMA) {
        /* "ADD A,..." form */
        has_a = 1;
        (*cur)++;
        if (parse_op(ctx, toks, cur, &b) < 0) return -1;
        src = &b;
    } else if (wants_a && toks[*cur].kind == TK_COMMA) {
        /* allow "ADD HL,rp" / "ADC HL,rp" / "SBC HL,rp" / "ADD IX,rp" / "ADD IY,rp" */
        (*cur)++;
        if (parse_op(ctx, toks, cur, &b) < 0) return -1;
        /* Only ADD/ADC/SBC accept 16-bit forms; AND/OR/XOR/CP do not. */
        int is_add = !strcmp(mnem, "ADD");
        int is_adc = !strcmp(mnem, "ADC");
        int is_sbc = !strcmp(mnem, "SBC");
        if (a.type == OT_HL && (is_add || is_adc || is_sbc)) {
            int rp;
            switch (b.type) {
                case OT_BC: rp = 0; break;
                case OT_DE: rp = 1; break;
                case OT_HL: rp = 2; break;
                case OT_SP: rp = 3; break;
                default: asm_error(ctx, "bad 16-bit %s operand", mnem); return -1;
            }
            if (is_add) {
                emit_byte(ctx, 0x09 + rp * 16);
            } else if (is_adc) {
                emit_byte(ctx, 0xED);
                emit_byte(ctx, 0x4A + rp * 16);
            } else { /* sbc */
                emit_byte(ctx, 0xED);
                emit_byte(ctx, 0x42 + rp * 16);
            }
            return expect_eol(ctx, toks, cur);
        }
        if ((a.type == OT_IX || a.type == OT_IY) && is_add) {
            int prefix = (a.type == OT_IX) ? 0xDD : 0xFD;
            int self   = (a.type == OT_IX) ? OT_IX : OT_IY;
            int rp;
            switch (b.type) {
                case OT_BC: rp = 0; break;
                case OT_DE: rp = 1; break;
                case OT_SP: rp = 3; break;
                default:
                    if (b.type == self) { rp = 2; break; }
                    asm_error(ctx, "bad ADD I%c operand", (a.type == OT_IX) ? 'X' : 'Y');
                    return -1;
            }
            emit_byte(ctx, prefix);
            emit_byte(ctx, 0x09 + rp * 16);
            return expect_eol(ctx, toks, cur);
        }
        asm_error(ctx, "bad %s operands", mnem);
        return -1;
    }

    /* 8-bit form */
    if (!has_a) {
        /* short form: ADD r ; SUB r ; AND r ; etc. — A implicit */
        src = &a;
    }
    switch (src->type) {
        case OT_R8:
            emit_byte(ctx, (uint8_t)(base_r + src->reg));
            return expect_eol(ctx, toks, cur);
        case OT_HL_IND:
            emit_byte(ctx, (uint8_t)(base_r + 6));
            return expect_eol(ctx, toks, cur);
        case OT_IX_D:
        case OT_IX_IND:
            if (chk_disp(ctx, src->type == OT_IX_D ? src->disp : 0) < 0) return -1;
            emit_byte(ctx, 0xDD);
            emit_byte(ctx, (uint8_t)(base_r + 6));
            emit_byte(ctx, (uint8_t)(src->type == OT_IX_D ? src->disp : 0));
            return expect_eol(ctx, toks, cur);
        case OT_IY_D:
        case OT_IY_IND:
            if (chk_disp(ctx, src->type == OT_IY_D ? src->disp : 0) < 0) return -1;
            emit_byte(ctx, 0xFD);
            emit_byte(ctx, (uint8_t)(base_r + 6));
            emit_byte(ctx, (uint8_t)(src->type == OT_IY_D ? src->disp : 0));
            return expect_eol(ctx, toks, cur);
        case OT_IMM:
            if (chk_byte(ctx, src->val, "immediate") < 0) return -1;
            emit_byte(ctx, (uint8_t)base_n);
            emit_byte(ctx, (uint8_t)(src->val & 0xFF));
            return expect_eol(ctx, toks, cur);
        default:
            asm_error(ctx, "bad %s operand", mnem);
            return -1;
    }
}

/* INC/DEC */
static int do_incdec(AsmCtx *ctx, Token *toks, int *cur, int is_dec) {
    Op a;
    if (parse_op(ctx, toks, cur, &a) < 0) return -1;
    int base8  = is_dec ? 0x05 : 0x04;
    int base16 = is_dec ? 0x0B : 0x03;
    int base16ix = is_dec ? 0x2B : 0x23;
    switch (a.type) {
        case OT_R8:
            emit_byte(ctx, (uint8_t)(base8 + a.reg * 8));
            return expect_eol(ctx, toks, cur);
        case OT_HL_IND:
            emit_byte(ctx, (uint8_t)(base8 + 6 * 8));
            return expect_eol(ctx, toks, cur);
        case OT_IX_D:
        case OT_IX_IND:
            if (chk_disp(ctx, a.type == OT_IX_D ? a.disp : 0) < 0) return -1;
            emit_byte(ctx, 0xDD);
            emit_byte(ctx, (uint8_t)(base8 + 6 * 8));
            emit_byte(ctx, (uint8_t)(a.type == OT_IX_D ? a.disp : 0));
            return expect_eol(ctx, toks, cur);
        case OT_IY_D:
        case OT_IY_IND:
            if (chk_disp(ctx, a.type == OT_IY_D ? a.disp : 0) < 0) return -1;
            emit_byte(ctx, 0xFD);
            emit_byte(ctx, (uint8_t)(base8 + 6 * 8));
            emit_byte(ctx, (uint8_t)(a.type == OT_IY_D ? a.disp : 0));
            return expect_eol(ctx, toks, cur);
        case OT_BC: emit_byte(ctx, (uint8_t)(base16 + 0 * 16)); return expect_eol(ctx, toks, cur);
        case OT_DE: emit_byte(ctx, (uint8_t)(base16 + 1 * 16)); return expect_eol(ctx, toks, cur);
        case OT_HL: emit_byte(ctx, (uint8_t)(base16 + 2 * 16)); return expect_eol(ctx, toks, cur);
        case OT_SP: emit_byte(ctx, (uint8_t)(base16 + 3 * 16)); return expect_eol(ctx, toks, cur);
        case OT_IX: emit_byte(ctx, 0xDD); emit_byte(ctx, (uint8_t)base16ix); return expect_eol(ctx, toks, cur);
        case OT_IY: emit_byte(ctx, 0xFD); emit_byte(ctx, (uint8_t)base16ix); return expect_eol(ctx, toks, cur);
        default:
            asm_error(ctx, "bad %s operand", is_dec ? "DEC" : "INC");
            return -1;
    }
}

/* PUSH / POP */
static int do_pushpop(AsmCtx *ctx, Token *toks, int *cur, int is_push) {
    Op a;
    if (parse_op(ctx, toks, cur, &a) < 0) return -1;
    int base = is_push ? 0xC5 : 0xC1;
    switch (a.type) {
        case OT_BC: emit_byte(ctx, (uint8_t)(base + 0 * 16)); return expect_eol(ctx, toks, cur);
        case OT_DE: emit_byte(ctx, (uint8_t)(base + 1 * 16)); return expect_eol(ctx, toks, cur);
        case OT_HL: emit_byte(ctx, (uint8_t)(base + 2 * 16)); return expect_eol(ctx, toks, cur);
        case OT_AF: emit_byte(ctx, (uint8_t)(base + 3 * 16)); return expect_eol(ctx, toks, cur);
        case OT_IX:
            emit_byte(ctx, 0xDD); emit_byte(ctx, (uint8_t)(is_push ? 0xE5 : 0xE1));
            return expect_eol(ctx, toks, cur);
        case OT_IY:
            emit_byte(ctx, 0xFD); emit_byte(ctx, (uint8_t)(is_push ? 0xE5 : 0xE1));
            return expect_eol(ctx, toks, cur);
        default:
            asm_error(ctx, "bad %s operand", is_push ? "PUSH" : "POP");
            return -1;
    }
}

/* Shift/rotate group: RLC/RRC/RL/RR/SLA/SRA/SLL/SRL.
   base in CB table: RLC=0, RRC=8, RL=10, RR=18, SLA=20, SRA=28, SLL=30, SRL=38. */
static int do_shift(AsmCtx *ctx, Token *toks, int *cur, int base) {
    Op a;
    if (parse_op(ctx, toks, cur, &a) < 0) return -1;
    switch (a.type) {
        case OT_R8:
            emit_byte(ctx, 0xCB);
            emit_byte(ctx, (uint8_t)(base + a.reg));
            return expect_eol(ctx, toks, cur);
        case OT_HL_IND:
            emit_byte(ctx, 0xCB);
            emit_byte(ctx, (uint8_t)(base + 6));
            return expect_eol(ctx, toks, cur);
        case OT_IX_D:
        case OT_IX_IND:
            if (chk_disp(ctx, a.type == OT_IX_D ? a.disp : 0) < 0) return -1;
            emit_byte(ctx, 0xDD); emit_byte(ctx, 0xCB);
            emit_byte(ctx, (uint8_t)(a.type == OT_IX_D ? a.disp : 0));
            emit_byte(ctx, (uint8_t)(base + 6));
            return expect_eol(ctx, toks, cur);
        case OT_IY_D:
        case OT_IY_IND:
            if (chk_disp(ctx, a.type == OT_IY_D ? a.disp : 0) < 0) return -1;
            emit_byte(ctx, 0xFD); emit_byte(ctx, 0xCB);
            emit_byte(ctx, (uint8_t)(a.type == OT_IY_D ? a.disp : 0));
            emit_byte(ctx, (uint8_t)(base + 6));
            return expect_eol(ctx, toks, cur);
        default:
            asm_error(ctx, "bad shift/rotate operand");
            return -1;
    }
}

/* BIT / RES / SET: base in CB table = BIT 0x40, RES 0x80, SET 0xC0. */
static int do_bitop(AsmCtx *ctx, Token *toks, int *cur, int base, const char *mnem) {
    Op b, t;
    if (parse_op(ctx, toks, cur, &b) < 0) return -1;
    if (b.type != OT_IMM) { asm_error(ctx, "%s expects bit number", mnem); return -1; }
    if (chk_bit(ctx, b.val) < 0) return -1;
    if (expect_comma(ctx, toks, cur) < 0) return -1;
    if (parse_op(ctx, toks, cur, &t) < 0) return -1;
    int bb = (int)(b.val & 7);
    switch (t.type) {
        case OT_R8:
            emit_byte(ctx, 0xCB);
            emit_byte(ctx, (uint8_t)(base + bb * 8 + t.reg));
            return expect_eol(ctx, toks, cur);
        case OT_HL_IND:
            emit_byte(ctx, 0xCB);
            emit_byte(ctx, (uint8_t)(base + bb * 8 + 6));
            return expect_eol(ctx, toks, cur);
        case OT_IX_D:
        case OT_IX_IND:
            if (chk_disp(ctx, t.type == OT_IX_D ? t.disp : 0) < 0) return -1;
            emit_byte(ctx, 0xDD); emit_byte(ctx, 0xCB);
            emit_byte(ctx, (uint8_t)(t.type == OT_IX_D ? t.disp : 0));
            emit_byte(ctx, (uint8_t)(base + bb * 8 + 6));
            return expect_eol(ctx, toks, cur);
        case OT_IY_D:
        case OT_IY_IND:
            if (chk_disp(ctx, t.type == OT_IY_D ? t.disp : 0) < 0) return -1;
            emit_byte(ctx, 0xFD); emit_byte(ctx, 0xCB);
            emit_byte(ctx, (uint8_t)(t.type == OT_IY_D ? t.disp : 0));
            emit_byte(ctx, (uint8_t)(base + bb * 8 + 6));
            return expect_eol(ctx, toks, cur);
        default:
            asm_error(ctx, "bad %s target", mnem);
            return -1;
    }
}

/* LD encoder. */
static int do_ld(AsmCtx *ctx, Token *toks, int *cur) {
    Op d, s;
    if (parse_op(ctx, toks, cur, &d) < 0) return -1;
    if (expect_comma(ctx, toks, cur) < 0) return -1;
    if (parse_op(ctx, toks, cur, &s) < 0) return -1;

    /* LD r,r' (also LD A,r / LD r,A) */
    if (d.type == OT_R8 && s.type == OT_R8) {
        emit_byte(ctx, (uint8_t)(0x40 + d.reg * 8 + s.reg));
        return expect_eol(ctx, toks, cur);
    }
    /* LD r,(HL) */
    if (d.type == OT_R8 && s.type == OT_HL_IND) {
        emit_byte(ctx, (uint8_t)(0x46 + d.reg * 8));
        return expect_eol(ctx, toks, cur);
    }
    /* LD (HL),r */
    if (d.type == OT_HL_IND && s.type == OT_R8) {
        emit_byte(ctx, (uint8_t)(0x70 + s.reg));
        return expect_eol(ctx, toks, cur);
    }
    /* LD (HL),n */
    if (d.type == OT_HL_IND && s.type == OT_IMM) {
        if (chk_byte(ctx, s.val, "immediate") < 0) return -1;
        emit_byte(ctx, 0x36);
        emit_byte(ctx, (uint8_t)(s.val & 0xFF));
        return expect_eol(ctx, toks, cur);
    }
    /* LD r,n */
    if (d.type == OT_R8 && s.type == OT_IMM) {
        if (chk_byte(ctx, s.val, "immediate") < 0) return -1;
        emit_byte(ctx, (uint8_t)(0x06 + d.reg * 8));
        emit_byte(ctx, (uint8_t)(s.val & 0xFF));
        return expect_eol(ctx, toks, cur);
    }
    /* LD r,(IX+d) / LD r,(IY+d) */
    if (d.type == OT_R8 && (s.type == OT_IX_D || s.type == OT_IX_IND ||
                            s.type == OT_IY_D || s.type == OT_IY_IND)) {
        int isiy = (s.type == OT_IY_D || s.type == OT_IY_IND);
        int32_t dd = (s.type == OT_IX_D || s.type == OT_IY_D) ? s.disp : 0;
        if (chk_disp(ctx, dd) < 0) return -1;
        emit_byte(ctx, isiy ? 0xFD : 0xDD);
        emit_byte(ctx, (uint8_t)(0x46 + d.reg * 8));
        emit_byte(ctx, (uint8_t)dd);
        return expect_eol(ctx, toks, cur);
    }
    /* LD (IX+d),r / LD (IY+d),r */
    if ((d.type == OT_IX_D || d.type == OT_IX_IND ||
         d.type == OT_IY_D || d.type == OT_IY_IND) && s.type == OT_R8) {
        int isiy = (d.type == OT_IY_D || d.type == OT_IY_IND);
        int32_t dd = (d.type == OT_IX_D || d.type == OT_IY_D) ? d.disp : 0;
        if (chk_disp(ctx, dd) < 0) return -1;
        emit_byte(ctx, isiy ? 0xFD : 0xDD);
        emit_byte(ctx, (uint8_t)(0x70 + s.reg));
        emit_byte(ctx, (uint8_t)dd);
        return expect_eol(ctx, toks, cur);
    }
    /* LD (IX+d),n / LD (IY+d),n */
    if ((d.type == OT_IX_D || d.type == OT_IX_IND ||
         d.type == OT_IY_D || d.type == OT_IY_IND) && s.type == OT_IMM) {
        int isiy = (d.type == OT_IY_D || d.type == OT_IY_IND);
        int32_t dd = (d.type == OT_IX_D || d.type == OT_IY_D) ? d.disp : 0;
        if (chk_disp(ctx, dd) < 0) return -1;
        if (chk_byte(ctx, s.val, "immediate") < 0) return -1;
        emit_byte(ctx, isiy ? 0xFD : 0xDD);
        emit_byte(ctx, 0x36);
        emit_byte(ctx, (uint8_t)dd);
        emit_byte(ctx, (uint8_t)(s.val & 0xFF));
        return expect_eol(ctx, toks, cur);
    }
    /* LD A,(BC) / LD A,(DE) / LD A,(nn) */
    if (d.type == OT_R8 && d.reg == 7 && s.type == OT_BC_IND) {
        emit_byte(ctx, 0x0A); return expect_eol(ctx, toks, cur);
    }
    if (d.type == OT_R8 && d.reg == 7 && s.type == OT_DE_IND) {
        emit_byte(ctx, 0x1A); return expect_eol(ctx, toks, cur);
    }
    if (d.type == OT_R8 && d.reg == 7 && s.type == OT_MEM) {
        if (chk_word(ctx, s.val, "address") < 0) return -1;
        emit_byte(ctx, 0x3A);
        emit_word(ctx, (uint16_t)s.val);
        return expect_eol(ctx, toks, cur);
    }
    /* LD (BC),A / LD (DE),A */
    if (d.type == OT_BC_IND && s.type == OT_R8 && s.reg == 7) {
        emit_byte(ctx, 0x02); return expect_eol(ctx, toks, cur);
    }
    if (d.type == OT_DE_IND && s.type == OT_R8 && s.reg == 7) {
        emit_byte(ctx, 0x12); return expect_eol(ctx, toks, cur);
    }
    /* LD (nn),A */
    if (d.type == OT_MEM && s.type == OT_R8 && s.reg == 7) {
        if (chk_word(ctx, d.val, "address") < 0) return -1;
        emit_byte(ctx, 0x32);
        emit_word(ctx, (uint16_t)d.val);
        return expect_eol(ctx, toks, cur);
    }
    /* LD A,I / LD A,R / LD I,A / LD R,A */
    if (d.type == OT_R8 && d.reg == 7 && s.type == OT_I) {
        emit_byte(ctx, 0xED); emit_byte(ctx, 0x57); return expect_eol(ctx, toks, cur);
    }
    if (d.type == OT_R8 && d.reg == 7 && s.type == OT_R_REG) {
        emit_byte(ctx, 0xED); emit_byte(ctx, 0x5F); return expect_eol(ctx, toks, cur);
    }
    if (d.type == OT_I && s.type == OT_R8 && s.reg == 7) {
        emit_byte(ctx, 0xED); emit_byte(ctx, 0x47); return expect_eol(ctx, toks, cur);
    }
    if (d.type == OT_R_REG && s.type == OT_R8 && s.reg == 7) {
        emit_byte(ctx, 0xED); emit_byte(ctx, 0x4F); return expect_eol(ctx, toks, cur);
    }
    /* 16-bit loads */
    /* LD rp,nn */
    if ((d.type == OT_BC || d.type == OT_DE || d.type == OT_HL || d.type == OT_SP) && s.type == OT_IMM) {
        if (chk_word(ctx, s.val, "immediate") < 0) return -1;
        int rp = (d.type == OT_BC) ? 0 : (d.type == OT_DE) ? 1 : (d.type == OT_HL) ? 2 : 3;
        emit_byte(ctx, (uint8_t)(0x01 + rp * 16));
        emit_word(ctx, (uint16_t)s.val);
        return expect_eol(ctx, toks, cur);
    }
    /* LD IX,nn / LD IY,nn */
    if ((d.type == OT_IX || d.type == OT_IY) && s.type == OT_IMM) {
        if (chk_word(ctx, s.val, "immediate") < 0) return -1;
        emit_byte(ctx, d.type == OT_IX ? 0xDD : 0xFD);
        emit_byte(ctx, 0x21);
        emit_word(ctx, (uint16_t)s.val);
        return expect_eol(ctx, toks, cur);
    }
    /* LD HL,(nn) */
    if (d.type == OT_HL && s.type == OT_MEM) {
        if (chk_word(ctx, s.val, "address") < 0) return -1;
        emit_byte(ctx, 0x2A);
        emit_word(ctx, (uint16_t)s.val);
        return expect_eol(ctx, toks, cur);
    }
    /* LD rp,(nn) for BC/DE/SP via ED prefix */
    if ((d.type == OT_BC || d.type == OT_DE || d.type == OT_SP) && s.type == OT_MEM) {
        if (chk_word(ctx, s.val, "address") < 0) return -1;
        int rp = (d.type == OT_BC) ? 0 : (d.type == OT_DE) ? 1 : 3;
        emit_byte(ctx, 0xED);
        emit_byte(ctx, (uint8_t)(0x4B + rp * 16));
        emit_word(ctx, (uint16_t)s.val);
        return expect_eol(ctx, toks, cur);
    }
    /* LD IX,(nn) / LD IY,(nn) */
    if ((d.type == OT_IX || d.type == OT_IY) && s.type == OT_MEM) {
        if (chk_word(ctx, s.val, "address") < 0) return -1;
        emit_byte(ctx, d.type == OT_IX ? 0xDD : 0xFD);
        emit_byte(ctx, 0x2A);
        emit_word(ctx, (uint16_t)s.val);
        return expect_eol(ctx, toks, cur);
    }
    /* LD (nn),HL */
    if (d.type == OT_MEM && s.type == OT_HL) {
        if (chk_word(ctx, d.val, "address") < 0) return -1;
        emit_byte(ctx, 0x22);
        emit_word(ctx, (uint16_t)d.val);
        return expect_eol(ctx, toks, cur);
    }
    /* LD (nn),rp for BC/DE/SP via ED prefix */
    if (d.type == OT_MEM && (s.type == OT_BC || s.type == OT_DE || s.type == OT_SP)) {
        if (chk_word(ctx, d.val, "address") < 0) return -1;
        int rp = (s.type == OT_BC) ? 0 : (s.type == OT_DE) ? 1 : 3;
        emit_byte(ctx, 0xED);
        emit_byte(ctx, (uint8_t)(0x43 + rp * 16));
        emit_word(ctx, (uint16_t)d.val);
        return expect_eol(ctx, toks, cur);
    }
    /* LD (nn),IX / LD (nn),IY */
    if (d.type == OT_MEM && (s.type == OT_IX || s.type == OT_IY)) {
        if (chk_word(ctx, d.val, "address") < 0) return -1;
        emit_byte(ctx, s.type == OT_IX ? 0xDD : 0xFD);
        emit_byte(ctx, 0x22);
        emit_word(ctx, (uint16_t)d.val);
        return expect_eol(ctx, toks, cur);
    }
    /* LD SP,HL / LD SP,IX / LD SP,IY */
    if (d.type == OT_SP && s.type == OT_HL) { emit_byte(ctx, 0xF9); return expect_eol(ctx, toks, cur); }
    if (d.type == OT_SP && s.type == OT_IX) { emit_byte(ctx, 0xDD); emit_byte(ctx, 0xF9); return expect_eol(ctx, toks, cur); }
    if (d.type == OT_SP && s.type == OT_IY) { emit_byte(ctx, 0xFD); emit_byte(ctx, 0xF9); return expect_eol(ctx, toks, cur); }

    asm_error(ctx, "invalid LD operands");
    return -1;
}

/* JP */
static int do_jp(AsmCtx *ctx, Token *toks, int *cur) {
    /* Conditional? First token is IDENT matching a cond name AND next is comma. */
    if (toks[*cur].kind == TK_IDENT && cond_code(toks[*cur].text) >= 0 &&
        toks[*cur + 1].kind == TK_COMMA) {
        int cc = cond_code(toks[*cur].text);
        (*cur)++;
        (*cur)++; /* comma */
        Op a;
        if (parse_op(ctx, toks, cur, &a) < 0) return -1;
        if (a.type != OT_IMM) { asm_error(ctx, "JP cc expects address"); return -1; }
        if (chk_word(ctx, a.val, "address") < 0) return -1;
        emit_byte(ctx, (uint8_t)(0xC2 + cc * 8));
        emit_word(ctx, (uint16_t)a.val);
        return expect_eol(ctx, toks, cur);
    }
    Op a;
    if (parse_op(ctx, toks, cur, &a) < 0) return -1;
    if (a.type == OT_HL_IND || a.type == OT_HL) {
        /* "JP (HL)" — accept "JP HL" too. */
        emit_byte(ctx, 0xE9);
        return expect_eol(ctx, toks, cur);
    }
    if (a.type == OT_IX_IND || a.type == OT_IX) {
        emit_byte(ctx, 0xDD); emit_byte(ctx, 0xE9);
        return expect_eol(ctx, toks, cur);
    }
    if (a.type == OT_IY_IND || a.type == OT_IY) {
        emit_byte(ctx, 0xFD); emit_byte(ctx, 0xE9);
        return expect_eol(ctx, toks, cur);
    }
    if (a.type == OT_IMM) {
        if (chk_word(ctx, a.val, "address") < 0) return -1;
        emit_byte(ctx, 0xC3);
        emit_word(ctx, (uint16_t)a.val);
        return expect_eol(ctx, toks, cur);
    }
    asm_error(ctx, "bad JP operand");
    return -1;
}

/* JR */
static int do_jr(AsmCtx *ctx, Token *toks, int *cur) {
    int cc = -1;
    if (toks[*cur].kind == TK_IDENT && jr_cond_code(toks[*cur].text) >= 0 &&
        toks[*cur + 1].kind == TK_COMMA) {
        cc = jr_cond_code(toks[*cur].text);
        (*cur)++;
        (*cur)++;
    }
    int32_t v; int rsv;
    if (expr_parse(ctx, toks, cur, &v, &rsv) < 0) return -1;
    /* Encode as: opcode + signed8 relative to PC+2 (after this 2-byte insn). */
    int32_t base_pc = ctx->pc + 2;
    int32_t disp = v - base_pc;
    if (ctx->pass == PASS2 && (disp < -128 || disp > 127)) {
        asm_error(ctx, "JR target out of range (disp=%d)", disp);
        return -1;
    }
    if (cc < 0) {
        emit_byte(ctx, 0x18);
    } else {
        static const uint8_t base[4] = { 0x20, 0x28, 0x30, 0x38 }; /* NZ,Z,NC,C */
        emit_byte(ctx, base[cc]);
    }
    emit_byte(ctx, (uint8_t)(disp & 0xFF));
    return expect_eol(ctx, toks, cur);
}

/* DJNZ */
static int do_djnz(AsmCtx *ctx, Token *toks, int *cur) {
    int32_t v; int rsv;
    if (expr_parse(ctx, toks, cur, &v, &rsv) < 0) return -1;
    int32_t base_pc = ctx->pc + 2;
    int32_t disp = v - base_pc;
    if (ctx->pass == PASS2 && (disp < -128 || disp > 127)) {
        asm_error(ctx, "DJNZ target out of range (disp=%d)", disp);
        return -1;
    }
    emit_byte(ctx, 0x10);
    emit_byte(ctx, (uint8_t)(disp & 0xFF));
    return expect_eol(ctx, toks, cur);
}

/* CALL */
static int do_call(AsmCtx *ctx, Token *toks, int *cur) {
    if (toks[*cur].kind == TK_IDENT && cond_code(toks[*cur].text) >= 0 &&
        toks[*cur + 1].kind == TK_COMMA) {
        int cc = cond_code(toks[*cur].text);
        (*cur)++; (*cur)++;
        int32_t v; int rsv;
        if (expr_parse(ctx, toks, cur, &v, &rsv) < 0) return -1;
        if (chk_word(ctx, v, "address") < 0) return -1;
        emit_byte(ctx, (uint8_t)(0xC4 + cc * 8));
        emit_word(ctx, (uint16_t)v);
        return expect_eol(ctx, toks, cur);
    }
    int32_t v; int rsv;
    if (expr_parse(ctx, toks, cur, &v, &rsv) < 0) return -1;
    if (chk_word(ctx, v, "address") < 0) return -1;
    emit_byte(ctx, 0xCD);
    emit_word(ctx, (uint16_t)v);
    return expect_eol(ctx, toks, cur);
}

/* RET */
static int do_ret(AsmCtx *ctx, Token *toks, int *cur) {
    if (toks[*cur].kind == TK_EOL) {
        emit_byte(ctx, 0xC9);
        return 0;
    }
    if (toks[*cur].kind == TK_IDENT && cond_code(toks[*cur].text) >= 0) {
        int cc = cond_code(toks[*cur].text);
        (*cur)++;
        emit_byte(ctx, (uint8_t)(0xC0 + cc * 8));
        return expect_eol(ctx, toks, cur);
    }
    asm_error(ctx, "bad RET operand");
    return -1;
}

/* RST */
static int do_rst(AsmCtx *ctx, Token *toks, int *cur) {
    int32_t v; int rsv;
    if (expr_parse(ctx, toks, cur, &v, &rsv) < 0) return -1;
    if (ctx->pass == PASS2) {
        if (v < 0 || v > 0x38 || (v & 7) != 0) {
            asm_error(ctx, "bad RST target 0x%02X", v);
            return -1;
        }
    }
    emit_byte(ctx, (uint8_t)(0xC7 + (v & 0x38)));
    return expect_eol(ctx, toks, cur);
}

/* IM */
static int do_im(AsmCtx *ctx, Token *toks, int *cur) {
    int32_t v; int rsv;
    if (expr_parse(ctx, toks, cur, &v, &rsv) < 0) return -1;
    if (ctx->pass == PASS2 && (v < 0 || v > 2)) {
        asm_error(ctx, "bad IM mode %d", v);
        return -1;
    }
    emit_byte(ctx, 0xED);
    static const uint8_t op[3] = { 0x46, 0x56, 0x5E };
    emit_byte(ctx, op[v & 3]);
    return expect_eol(ctx, toks, cur);
}

/* EX */
static int do_ex(AsmCtx *ctx, Token *toks, int *cur) {
    Op a, b;
    if (parse_op(ctx, toks, cur, &a) < 0) return -1;
    if (expect_comma(ctx, toks, cur) < 0) return -1;
    if (parse_op(ctx, toks, cur, &b) < 0) return -1;
    /* EX DE,HL */
    if (a.type == OT_DE && b.type == OT_HL) { emit_byte(ctx, 0xEB); return expect_eol(ctx, toks, cur); }
    /* EX AF,AF' */
    if (a.type == OT_AF && b.type == OT_AFP) { emit_byte(ctx, 0x08); return expect_eol(ctx, toks, cur); }
    /* EX (SP),HL */
    if (a.type == OT_SP_IND && b.type == OT_HL) { emit_byte(ctx, 0xE3); return expect_eol(ctx, toks, cur); }
    /* EX (SP),IX */
    if (a.type == OT_SP_IND && b.type == OT_IX) { emit_byte(ctx, 0xDD); emit_byte(ctx, 0xE3); return expect_eol(ctx, toks, cur); }
    /* EX (SP),IY */
    if (a.type == OT_SP_IND && b.type == OT_IY) { emit_byte(ctx, 0xFD); emit_byte(ctx, 0xE3); return expect_eol(ctx, toks, cur); }
    asm_error(ctx, "bad EX operands");
    return -1;
}

/* IN: IN A,(n) ; IN r,(C) */
static int do_in(AsmCtx *ctx, Token *toks, int *cur) {
    Op a, b;
    if (parse_op(ctx, toks, cur, &a) < 0) return -1;
    if (expect_comma(ctx, toks, cur) < 0) return -1;
    if (parse_op(ctx, toks, cur, &b) < 0) return -1;
    if (a.type == OT_R8 && a.reg == 7 && b.type == OT_MEM) {
        if (chk_byte(ctx, b.val, "port") < 0) return -1;
        emit_byte(ctx, 0xDB);
        emit_byte(ctx, (uint8_t)(b.val & 0xFF));
        return expect_eol(ctx, toks, cur);
    }
    if (a.type == OT_R8 && b.type == OT_C_IND) {
        emit_byte(ctx, 0xED);
        emit_byte(ctx, (uint8_t)(0x40 + a.reg * 8));
        return expect_eol(ctx, toks, cur);
    }
    asm_error(ctx, "bad IN operands");
    return -1;
}

/* OUT: OUT (n),A ; OUT (C),r */
static int do_out(AsmCtx *ctx, Token *toks, int *cur) {
    Op a, b;
    if (parse_op(ctx, toks, cur, &a) < 0) return -1;
    if (expect_comma(ctx, toks, cur) < 0) return -1;
    if (parse_op(ctx, toks, cur, &b) < 0) return -1;
    if (a.type == OT_MEM && b.type == OT_R8 && b.reg == 7) {
        if (chk_byte(ctx, a.val, "port") < 0) return -1;
        emit_byte(ctx, 0xD3);
        emit_byte(ctx, (uint8_t)(a.val & 0xFF));
        return expect_eol(ctx, toks, cur);
    }
    if (a.type == OT_C_IND && b.type == OT_R8) {
        emit_byte(ctx, 0xED);
        emit_byte(ctx, (uint8_t)(0x41 + b.reg * 8));
        return expect_eol(ctx, toks, cur);
    }
    asm_error(ctx, "bad OUT operands");
    return -1;
}

/* ---------------- mnemonic dispatch ---------------- */

typedef struct {
    const char *m;
    int       (*h)(AsmCtx *, Token *, int *);
} OneByte;

static int emit_simple(AsmCtx *ctx, Token *toks, int *cur, uint8_t b) {
    (void)toks; (void)cur;
    emit_byte(ctx, b);
    return expect_eol(ctx, toks, cur);
}
static int emit_ed_simple(AsmCtx *ctx, Token *toks, int *cur, uint8_t b) {
    (void)toks; (void)cur;
    emit_byte(ctx, 0xED);
    emit_byte(ctx, b);
    return expect_eol(ctx, toks, cur);
}

static int h_nop (AsmCtx *c, Token *t, int *u){ return emit_simple(c,t,u,0x00); }
static int h_halt(AsmCtx *c, Token *t, int *u){ return emit_simple(c,t,u,0x76); }
static int h_di  (AsmCtx *c, Token *t, int *u){ return emit_simple(c,t,u,0xF3); }
static int h_ei  (AsmCtx *c, Token *t, int *u){ return emit_simple(c,t,u,0xFB); }
static int h_exx (AsmCtx *c, Token *t, int *u){ return emit_simple(c,t,u,0xD9); }
static int h_ccf (AsmCtx *c, Token *t, int *u){ return emit_simple(c,t,u,0x3F); }
static int h_scf (AsmCtx *c, Token *t, int *u){ return emit_simple(c,t,u,0x37); }
static int h_cpl (AsmCtx *c, Token *t, int *u){ return emit_simple(c,t,u,0x2F); }
static int h_daa (AsmCtx *c, Token *t, int *u){ return emit_simple(c,t,u,0x27); }
static int h_rlca(AsmCtx *c, Token *t, int *u){ return emit_simple(c,t,u,0x07); }
static int h_rrca(AsmCtx *c, Token *t, int *u){ return emit_simple(c,t,u,0x0F); }
static int h_rla (AsmCtx *c, Token *t, int *u){ return emit_simple(c,t,u,0x17); }
static int h_rra (AsmCtx *c, Token *t, int *u){ return emit_simple(c,t,u,0x1F); }
static int h_neg (AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0x44); }
static int h_reti(AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0x4D); }
static int h_retn(AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0x45); }
static int h_rld (AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0x6F); }
static int h_rrd (AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0x67); }
static int h_cpd (AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xA9); }
static int h_cpdr(AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xB9); }
static int h_cpi (AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xA1); }
static int h_cpir(AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xB1); }
static int h_ldd (AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xA8); }
static int h_lddr(AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xB8); }
static int h_ldi (AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xA0); }
static int h_ldir(AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xB0); }
static int h_ind (AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xAA); }
static int h_indr(AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xBA); }
static int h_ini (AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xA2); }
static int h_inir(AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xB2); }
static int h_outd(AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xAB); }
static int h_outi(AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xA3); }
static int h_otdr(AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xBB); }
static int h_otir(AsmCtx *c, Token *t, int *u){ return emit_ed_simple(c,t,u,0xB3); }

static int h_add(AsmCtx *c, Token *t, int *u){ return do_alu(c,t,u,"ADD",0x80,0xC6,1); }
static int h_adc(AsmCtx *c, Token *t, int *u){ return do_alu(c,t,u,"ADC",0x88,0xCE,1); }
static int h_sub(AsmCtx *c, Token *t, int *u){ return do_alu(c,t,u,"SUB",0x90,0xD6,0); }
static int h_sbc(AsmCtx *c, Token *t, int *u){ return do_alu(c,t,u,"SBC",0x98,0xDE,1); }
static int h_and(AsmCtx *c, Token *t, int *u){ return do_alu(c,t,u,"AND",0xA0,0xE6,0); }
static int h_xor(AsmCtx *c, Token *t, int *u){ return do_alu(c,t,u,"XOR",0xA8,0xEE,0); }
static int h_or (AsmCtx *c, Token *t, int *u){ return do_alu(c,t,u,"OR", 0xB0,0xF6,0); }
static int h_cp (AsmCtx *c, Token *t, int *u){ return do_alu(c,t,u,"CP", 0xB8,0xFE,0); }

static int h_inc(AsmCtx *c, Token *t, int *u){ return do_incdec(c,t,u,0); }
static int h_dec(AsmCtx *c, Token *t, int *u){ return do_incdec(c,t,u,1); }

static int h_push(AsmCtx *c, Token *t, int *u){ return do_pushpop(c,t,u,1); }
static int h_pop (AsmCtx *c, Token *t, int *u){ return do_pushpop(c,t,u,0); }

static int h_rlc(AsmCtx *c, Token *t, int *u){ return do_shift(c,t,u,0x00); }
static int h_rrc(AsmCtx *c, Token *t, int *u){ return do_shift(c,t,u,0x08); }
static int h_rl (AsmCtx *c, Token *t, int *u){ return do_shift(c,t,u,0x10); }
static int h_rr (AsmCtx *c, Token *t, int *u){ return do_shift(c,t,u,0x18); }
static int h_sla(AsmCtx *c, Token *t, int *u){ return do_shift(c,t,u,0x20); }
static int h_sra(AsmCtx *c, Token *t, int *u){ return do_shift(c,t,u,0x28); }
static int h_sll(AsmCtx *c, Token *t, int *u){ return do_shift(c,t,u,0x30); }
static int h_srl(AsmCtx *c, Token *t, int *u){ return do_shift(c,t,u,0x38); }

static int h_bit(AsmCtx *c, Token *t, int *u){ return do_bitop(c,t,u,0x40,"BIT"); }
static int h_res(AsmCtx *c, Token *t, int *u){ return do_bitop(c,t,u,0x80,"RES"); }
static int h_set(AsmCtx *c, Token *t, int *u){ return do_bitop(c,t,u,0xC0,"SET"); }

static const OneByte MNEMS[] = {
    {"NOP",  h_nop}, {"HALT", h_halt}, {"DI",   h_di},   {"EI",   h_ei},
    {"EXX",  h_exx}, {"CCF",  h_ccf},  {"SCF",  h_scf},  {"CPL",  h_cpl},
    {"DAA",  h_daa}, {"RLCA", h_rlca}, {"RRCA", h_rrca}, {"RLA",  h_rla},
    {"RRA",  h_rra}, {"NEG",  h_neg},  {"RETI", h_reti}, {"RETN", h_retn},
    {"RLD",  h_rld}, {"RRD",  h_rrd},
    {"CPD",  h_cpd}, {"CPDR", h_cpdr}, {"CPI",  h_cpi},  {"CPIR", h_cpir},
    {"LDD",  h_ldd}, {"LDDR", h_lddr}, {"LDI",  h_ldi},  {"LDIR", h_ldir},
    {"IND",  h_ind}, {"INDR", h_indr}, {"INI",  h_ini},  {"INIR", h_inir},
    {"OUTD", h_outd},{"OUTI", h_outi}, {"OTDR", h_otdr}, {"OTIR", h_otir},
    {"ADD",  h_add}, {"ADC",  h_adc},  {"SUB",  h_sub},  {"SBC",  h_sbc},
    {"AND",  h_and}, {"XOR",  h_xor},  {"OR",   h_or},   {"CP",   h_cp},
    {"INC",  h_inc}, {"DEC",  h_dec},
    {"PUSH", h_push},{"POP",  h_pop},
    {"RLC",  h_rlc}, {"RRC",  h_rrc},  {"RL",   h_rl},   {"RR",   h_rr},
    {"SLA",  h_sla}, {"SRA",  h_sra},  {"SLL",  h_sll},  {"SRL",  h_srl},
    {"BIT",  h_bit}, {"RES",  h_res},  {"SET",  h_set},
    {"LD",   do_ld},
    {"JP",   do_jp}, {"JR",   do_jr},  {"DJNZ", do_djnz},
    {"CALL", do_call},{"RET", do_ret}, {"RST",  do_rst},
    {"IM",   do_im}, {"EX",   do_ex},
    {"IN",   do_in}, {"OUT",  do_out},
    {NULL, NULL}
};

int is_mnemonic(const char *upper) {
    if (!upper) return 0;
    for (int i = 0; MNEMS[i].m; i++) if (!strcmp(upper, MNEMS[i].m)) return 1;
    return 0;
}

int encode_instruction(AsmCtx *ctx, Token *toks, int *cur) {
    if (toks[*cur].kind != TK_IDENT) {
        asm_error(ctx, "expected mnemonic");
        return -1;
    }
    const char *m = toks[*cur].text;
    for (int i = 0; MNEMS[i].m; i++) {
        if (!strcmp(MNEMS[i].m, m)) {
            (*cur)++;
            return MNEMS[i].h(ctx, toks, cur);
        }
    }
    asm_error(ctx, "unknown mnemonic '%s'", m);
    return -1;
}
