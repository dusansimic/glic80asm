#ifndef GLIC80ASM_ASM_H
#define GLIC80ASM_ASM_H

#include <stdint.h>
#include <stdio.h>

typedef enum { PASS1 = 1, PASS2 = 2 } Pass;

struct SymTab;

typedef struct {
    Pass            pass;
    uint16_t        pc;
    uint16_t        origin;
    int             origin_set;
    uint8_t         out[65536];
    int             out_lo;
    int             out_hi;
    struct SymTab  *syms;
    const char     *filename;
    int             line_no;
    int             errors;
    int             end_seen;
    char           *last_label;   /* most recent non-local label, for `.foo` scoping */
    int             ext_escapes;  /* -ee: process \n \t \r \0 \\ \" \' in literals */
} AsmCtx;

void asm_error(AsmCtx *ctx, const char *fmt, ...);
void emit_byte(AsmCtx *ctx, uint8_t b);
void emit_word(AsmCtx *ctx, uint16_t w);

#endif
