#ifndef GLIC80ASM_PARSER_H
#define GLIC80ASM_PARSER_H

#include "asm.h"

/* Process a single source line. */
int parse_line(AsmCtx *ctx, const char *line);

#endif
