#include "asm.h"
#include "symtab.h"
#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(const char *prog) {
    fprintf(stderr,
        "glic80asm - Z80 assembler\n"
        "usage: %s [-o out.bin] [-l] [-e<flag>...] input.asm\n"
        "  -o PATH   output file (default a.bin)\n"
        "  -l        list symbols on stderr\n"
        "  -h        help\n"
        "extension flags (prefix -e):\n"
        "  -ee       decode C-style escapes (\\n \\t \\r \\0 \\\\ \\\" \\') in literals\n",
        prog);
}

static int read_file(const char *path, char **out) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "cannot open '%s'\n", path); return -1; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc((size_t)n + 1);
    if (fread(buf, 1, (size_t)n, f) != (size_t)n) {
        fprintf(stderr, "read error '%s'\n", path);
        fclose(f); free(buf); return -1;
    }
    buf[n] = 0;
    fclose(f);
    *out = buf;
    return 0;
}

static int run_pass(AsmCtx *ctx, const char *src) {
    ctx->pc = 0;
    ctx->line_no = 0;
    ctx->end_seen = 0;
    free(ctx->last_label);
    ctx->last_label = NULL;
    const char *p = src;
    while (*p && !ctx->end_seen) {
        const char *line_start = p;
        while (*p && *p != '\n') p++;
        size_t len = (size_t)(p - line_start);
        char *line = malloc(len + 1);
        memcpy(line, line_start, len);
        line[len] = 0;
        ctx->line_no++;
        parse_line(ctx, line);
        free(line);
        if (*p == '\n') p++;
    }
    return ctx->errors ? -1 : 0;
}

static void list_sym(const Sym *s, void *ud) {
    (void)ud;
    fprintf(stderr, "  %-20s 0x%04X%s\n", s->name, (unsigned)(s->value & 0xFFFF),
            s->defined ? "" : " (undef)");
}

int main(int argc, char **argv) {
    const char *out_path = "a.bin";
    const char *in_path  = NULL;
    int list_syms = 0;
    int ext_escapes = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o")) {
            if (i + 1 >= argc) { usage(argv[0]); return 2; }
            out_path = argv[++i];
        } else if (!strcmp(argv[i], "-l")) {
            list_syms = 1;
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]); return 0;
        } else if (!strcmp(argv[i], "-ee")) {
            ext_escapes = 1;
        } else if (argv[i][0] == '-' && argv[i][1] == 'e') {
            fprintf(stderr, "unknown extension flag '%s'\n", argv[i]);
            usage(argv[0]); return 2;
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "unknown option '%s'\n", argv[i]);
            usage(argv[0]); return 2;
        } else {
            if (in_path) { fprintf(stderr, "multiple inputs not supported\n"); return 2; }
            in_path = argv[i];
        }
    }
    if (!in_path) { usage(argv[0]); return 2; }

    char *src = NULL;
    if (read_file(in_path, &src) < 0) return 1;

    AsmCtx ctx = {0};
    ctx.filename    = in_path;
    ctx.ext_escapes = ext_escapes;
    ctx.syms     = symtab_new();
    ctx.out_lo   = 65536;
    ctx.out_hi   = 0;

    ctx.pass = PASS1;
    run_pass(&ctx, src);

    if (ctx.errors) {
        fprintf(stderr, "%d error(s) in pass 1\n", ctx.errors);
        free(src); symtab_free(ctx.syms); return 1;
    }

    ctx.pass = PASS2;
    ctx.errors = 0;
    run_pass(&ctx, src);

    if (list_syms) {
        fprintf(stderr, "symbols:\n");
        symtab_each(ctx.syms, list_sym, NULL);
    }

    if (ctx.errors) {
        fprintf(stderr, "%d error(s) in pass 2\n", ctx.errors);
        free(src); symtab_free(ctx.syms); return 1;
    }

    /* Write [out_lo, out_hi). */
    if (ctx.out_hi <= ctx.out_lo) {
        /* nothing emitted; still write empty file */
        FILE *f = fopen(out_path, "wb");
        if (!f) { fprintf(stderr, "cannot write '%s'\n", out_path); return 1; }
        fclose(f);
    } else {
        FILE *f = fopen(out_path, "wb");
        if (!f) { fprintf(stderr, "cannot write '%s'\n", out_path); return 1; }
        size_t n = (size_t)(ctx.out_hi - ctx.out_lo);
        if (fwrite(&ctx.out[ctx.out_lo], 1, n, f) != n) {
            fprintf(stderr, "write error '%s'\n", out_path);
            fclose(f); return 1;
        }
        fclose(f);
    }

    free(src);
    free(ctx.last_label);
    symtab_free(ctx.syms);
    return 0;
}
