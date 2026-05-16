#ifndef GLIC80ASM_SYMTAB_H
#define GLIC80ASM_SYMTAB_H

#include <stdint.h>

typedef struct Sym {
    char       *name;        /* uppercased */
    int32_t     value;
    int         defined;
    struct Sym *next;
} Sym;

typedef struct SymTab {
    Sym *head;
} SymTab;

SymTab *symtab_new(void);
void    symtab_free(SymTab *t);

/* Returns existing sym or NULL. */
Sym *symtab_find(SymTab *t, const char *name);

/* Define or update. Returns 0 on success, -1 on duplicate definition. */
int  symtab_define(SymTab *t, const char *name, int32_t value);

/* Iterate (for -l listing). */
void symtab_each(SymTab *t, void (*fn)(const Sym *, void *), void *ud);

#endif
