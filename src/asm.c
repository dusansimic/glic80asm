#include "asm.h"
#include <stdarg.h>
#include <stdio.h>

void asm_error(AsmCtx *ctx, const char *fmt, ...) {
    if (ctx->pass == PASS1) {
        /* During pass 1, only report errors that cannot be deferred (parse failures);
           expression resolution failures are silenced (out=0 returned). */
    }
    fprintf(stderr, "%s:%d: error: ", ctx->filename, ctx->line_no);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    ctx->errors++;
}

void emit_byte(AsmCtx *ctx, uint8_t b) {
    if (ctx->pass == PASS2) {
        ctx->out[ctx->pc] = b;
        if (ctx->pc < ctx->out_lo) ctx->out_lo = ctx->pc;
        if (ctx->pc + 1 > ctx->out_hi) ctx->out_hi = ctx->pc + 1;
    }
    ctx->pc++;
}

void emit_word(AsmCtx *ctx, uint16_t w) {
    emit_byte(ctx, (uint8_t)(w & 0xFF));
    emit_byte(ctx, (uint8_t)((w >> 8) & 0xFF));
}
