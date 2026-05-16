#ifndef GLIC80ASM_ENCODER_H
#define GLIC80ASM_ENCODER_H

#include "asm.h"
#include "lexer.h"

/* Encode one instruction line starting at toks[*cur], where the mnemonic
   token (uppercased ident) is at toks[*cur]. On success advances *cur past
   the instruction and emits bytes via emit_byte. Returns 0 ok, -1 on error. */
int encode_instruction(AsmCtx *ctx, Token *toks, int *cur);

/* True if name matches a Z80 mnemonic recognized by the encoder. */
int is_mnemonic(const char *upper);

#endif
