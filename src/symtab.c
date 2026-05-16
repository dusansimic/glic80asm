#include "symtab.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static char *upstr(const char *s) {
    size_t n = strlen(s);
    char *d = malloc(n + 1);
    for (size_t i = 0; i < n; i++) d[i] = (char)toupper((unsigned char)s[i]);
    d[n] = 0;
    return d;
}

SymTab *symtab_new(void) {
    SymTab *t = calloc(1, sizeof *t);
    return t;
}

void symtab_free(SymTab *t) {
    if (!t) return;
    Sym *p = t->head;
    while (p) {
        Sym *n = p->next;
        free(p->name);
        free(p);
        p = n;
    }
    free(t);
}

Sym *symtab_find(SymTab *t, const char *name) {
    char *u = upstr(name);
    Sym *p = t->head;
    while (p) {
        if (strcmp(p->name, u) == 0) { free(u); return p; }
        p = p->next;
    }
    free(u);
    return NULL;
}

int symtab_define(SymTab *t, const char *name, int32_t value) {
    Sym *e = symtab_find(t, name);
    if (e) {
        if (e->defined && e->value != value) return -1;
        e->value = value;
        e->defined = 1;
        return 0;
    }
    Sym *n = calloc(1, sizeof *n);
    n->name = upstr(name);
    n->value = value;
    n->defined = 1;
    n->next = t->head;
    t->head = n;
    return 0;
}

void symtab_each(SymTab *t, void (*fn)(const Sym *, void *), void *ud) {
    for (Sym *p = t->head; p; p = p->next) fn(p, ud);
}
