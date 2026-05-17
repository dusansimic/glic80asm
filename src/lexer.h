#ifndef GLIC80ASM_LEXER_H
#define GLIC80ASM_LEXER_H

#include <stdint.h>
#include <stddef.h>

typedef enum {
    TK_EOL = 0,
    TK_IDENT,    /* uppercase */
    TK_NUM,
    TK_STR,
    TK_CHAR,
    TK_COMMA,
    TK_LPAREN,
    TK_RPAREN,
    TK_COLON,
    TK_PLUS,
    TK_MINUS,
    TK_STAR,
    TK_SLASH,
    TK_PERCENT,
    TK_AMP,
    TK_PIPE,
    TK_CARET,
    TK_TILDE,
    TK_SHL,
    TK_SHR,
    TK_DOLLAR,   /* '$' as current PC */
    TK_APOS,     /* trailing apostrophe (AF') captured as part of ident, but keep token for safety */
    TK_HASH,     /* '#' SDCC immediate prefix (only emitted with sdcc=1) */
    TK_LT,       /* '<' single (low-byte op, sdcc=1 only) */
    TK_GT,       /* '>' single (high-byte op, sdcc=1 only) */
} TokKind;

typedef struct {
    TokKind  kind;
    char    *text;       /* IDENT (uppercased) or STR contents */
    int      tlen;       /* byte length for STR */
    int32_t  num;        /* NUMBER value or CHAR codepoint */
} Token;

typedef struct {
    Token *toks;
    int    n;
    int    cap;
} TokenList;

/* Tokenize a single line.
   `escapes` (-ee): decode \n \t etc. inside string/char literals.
   `sdcc`    (-ec): accept SDCC syntax — `#` immediate prefix, single `<` `>`
                    low/high ops, numeric `NNN$` labels, `$` inside idents.
   Returns 0 ok, -1 on lex error (msg printed to stderr with file:line). */
int  lex_line(const char *line, const char *filename, int line_no,
              int escapes, int sdcc, TokenList *out);
void tokens_free(TokenList *tl);

#endif
