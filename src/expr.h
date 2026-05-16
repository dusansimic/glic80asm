#ifndef GLIC80ASM_EXPR_H
#define GLIC80ASM_EXPR_H

#include <stdint.h>
#include "lexer.h"
#include "asm.h"

/* Parse expression starting at toks[*cur]. Advances *cur.
   On success returns 0 and stores value in *out_val and resolved flag in *out_resolved.
   On parse error, returns -1 and reports via asm_error. */
int expr_parse(AsmCtx *ctx, Token *toks, int *cur, int32_t *out_val, int *out_resolved);

#endif
