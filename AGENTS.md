# AGENTS.md

Notes for future agents (or humans) working on this codebase. Captures
non-obvious design decisions, the conventions in force, and the
compatibility lens to evaluate changes through. Read this before
touching the lexer, parser, encoder, or CLI surface — much of what
looks like a free choice is actually load-bearing for binary
compatibility with the existing test corpus.

## What this is

`glic80asm` is a Z80 assembler written in C99, targeting a custom
"GLIČ80" hobby board (a GameBoy-style device built around a Z80
processor with 32K RAM, an OLED, three buttons + a 5-way joystick, and
a single 8-LED row).

Source language: standard Zilog-style Z80 assembly, taken from
`z80_asm_list.txt` (a public reference). Output: raw flat binary,
trimmed to `[min_pc, max_pc)` so a program at `ORG $100` doesn't emit
256 bytes of padding.

Test sources for the board itself live outside the repo (the project's
`.gitignore` excludes `/tests/`); during development they were assembled
side-by-side against the original assembler's binaries to verify a
byte-for-byte match.

## Architecture in one paragraph

Two-pass, table-light. Pass 1 walks the source line-by-line: lexes the
line, defines labels (`label:` binds PC; `name EQU expr` binds a
constant), advances PC through directives and encoded instructions
without emitting bytes. Pass 2 does the same walk but with the symbol
table fully populated, this time emitting actual bytes into a 64K
`out[]` buffer while tracking `out_lo`/`out_hi` for final trimming. The
encoder is mnemonic-dispatched (`switch`-like via a small `MNEMS[]`
table); operand parsing is contextual rather than table-driven.

Files and their roles:

| File                             | Role                                                                         |
| -------------------------------- | ---------------------------------------------------------------------------- |
| `src/asm.h`, `src/asm.c`         | `AsmCtx`, `emit_byte`/`emit_word`, `asm_error`                               |
| `src/lexer.h`, `src/lexer.c`     | Single-line tokenizer; supports all numeric formats and string/char literals |
| `src/symtab.h`, `src/symtab.c`   | Case-insensitive linked-list symbol table                                    |
| `src/expr.h`, `src/expr.c`       | Recursive-descent expression parser with C precedence                        |
| `src/encoder.h`, `src/encoder.c` | Mnemonic dispatch + Z80 instruction encoding                                 |
| `src/parser.h`, `src/parser.c`   | Per-line dispatcher: label? directive? mnemonic?                             |
| `src/main.c`                     | CLI, two-pass orchestration, file I/O                                        |

`emit_byte` is pass-aware: PASS1 just bumps PC; PASS2 also writes into
`out[]` and updates the lo/hi bounds. This is what lets the same
encoder code drive both passes without bookkeeping at every call site.

## Compatibility is the whole point

The most important thing to understand: **this assembler exists to
produce the same binaries as the reference assembler the user already
uses**. There is a corpus of real programs (`systest`, `realrain`,
`minefield2`, `minefield3`, `tinyfont`, etc.) with committed reference
`.bin` files. The success criterion when adding any feature is that
those binaries still match byte-for-byte.

When in doubt about syntax or encoding, the rule is: **match the
reference**, not "do what the manual says." The reference assembler may
have quirks (e.g. literal backslash in strings, both `org` and `forg`
accepted, label-with-colon `EQU` form); we accommodate them. The plan
was always "be a drop-in replacement first, add cleanups behind opt-in
flags later."

The one place we already chose to diverge is the JR/DJNZ disp encoding:
target is computed relative to `pc + 2` (the _next_ instruction's PC).
This is the canonical Z80 definition; assemblers that get it wrong
produce wrong code, so no compatibility quirk to preserve there.

## Extension flag convention

CLI flags starting with `-e` are reserved for opt-in extensions —
features that change parser/lexer behavior in non-default ways. The
prefix lets future flags coexist without taking another short letter.
Currently:

| Flag  | Effect                                                                                                                                           |
| ----- | ------------------------------------------------------------------------------------------------------------------------------------------------ |
| `-ee` | Enable C-style escapes (`\n \t \r \0 \\ \" \'`) in string and char literals. Default is **literal backslash**, matching the reference assembler. |

The parser logic for this lives in `main.c`'s argv loop. Unknown
`-e<x>` flags get an explicit "unknown extension flag" error rather
than the generic "unknown option" message — that's intentional, to
make typos easy to spot.

**If you add a new extension flag**, follow this pattern:

1. Pick `-eX` (single trailing letter, lowercase preferred).
2. Add a field to `AsmCtx` (e.g. `int ext_<feature>;`).
3. Parse the flag in `main.c` and copy into `ctx`.
4. Honor it wherever needed (lexer, parser, encoder).
5. Document it in the `Extensions` section of `README.md` and the
   table above.
6. Default stays compatible — extensions are opt-in.

## Lexer specifics worth knowing

- **Numeric literals**: `$FF`, `0xFF`, `0FFh`, `%1010`, `10101010b`,
  decimal, and `'A'` character literals are all supported. Leading
  zero is required for hex with `h` suffix (`FFh` won't lex; `0FFh`
  will) — that's deliberate, the Zilog convention, and disambiguates
  identifiers from numbers.
- **`%` as binary vs modulo**: ambiguity is resolved by the next
  character. `%0` or `%1` → binary literal. Anything else → modulo
  operator. Practical effect: `a % b` works as modulo as long as there
  are spaces.
- **`$` as hex prefix vs PC**: `$` followed by a hex digit is a hex
  literal; bare `$` (followed by non-hex or operator) is the current PC.
- **`AF'`**: the lexer special-cases a trailing apostrophe right after
  `AF` so the alternate accumulator pair can be parsed as a single
  identifier. Don't generalize this to other registers — Z80 doesn't
  use `BC'`/`DE'`/`HL'` in source (those exchanges are done via `EXX`).
- **Identifier characters**: `.` is allowed in identifiers. This is
  what makes local labels (`.loop`) lex as a normal identifier.

## Local labels

Labels starting with `.` are scoped to the most recently defined
non-local label. Internally they're qualified — `.loop` after
`generate_droplet:` becomes the symbol `GENERATE_DROPLET.LOOP`. The
qualification happens at both definition (in `parser.c`) and reference
(in `expr.c`); both sides read `ctx->last_label` and prepend it.

Two things to watch out for:

1. `ctx->last_label` is reset at the start of every pass — without
   that, pass 2 would carry pass 1's leftover scope and the qualifier
   would point at the last label of pass 1 by the time pass 2 starts.
2. `EQU` does **not** update `last_label`. Only colon-bound labels
   (`label:` form) anchor scope. This matches what most assemblers do
   and avoids constants accidentally redirecting `.foo` references.

## Bugs the corpus actually caught

- **Expression `*res` initialization** (commit 1 era): `p_primary`
  didn't set `*res = 1` when it returned a numeric literal, leaving
  the caller's "resolved" flag uninitialized. Surfaced as
  _"ORG needs resolved expression in pass 1"_ even though `ORG $100`
  is clearly resolvable.
- **Binary `Nb` suffix**: the original plan only had `%1010`. A test
  source used the Intel/Zilog `11111111b` form. Added a fallback
  branch in the lexer's digit path.
- **`forg` directive**: alias for `ORG`. Some assemblers (or their
  users) use both `forg` and `org`; we accept either to the same
  effect.
- **String escapes**: `\|` in a `tinyfont` string is meant as a literal
  backslash-pipe pair on the reference assembler. We initially decoded
  escapes by default, which dropped the backslash and shifted every
  later byte by one. Fix: move escapes behind `-ee` (see above).

The pattern in all of these: the corpus diff was localized (often a
1-byte shift cascading), and the fix was small. When the corpus
mismatches by exactly N bytes from some early offset, look for an
"emitted 1 too many" / "emitted 1 too few" condition in lexer or
directive handling first.

## Adding a new instruction or directive

For instructions: extend the `MNEMS[]` table in `encoder.c` with the
mnemonic and a handler function. Handlers parse their own operands
using `parse_op`, then emit bytes. For simple no-operand instructions,
reuse `emit_simple` (or `emit_ed_simple` for `ED`-prefixed).

For directives: add a name to `is_directive()` in `parser.c` and a
branch in `parse_line()`'s directive dispatch. Make sure both passes
do the right thing — typically pass 1 advances PC, pass 2 emits bytes.
If the directive accepts expressions that must be known in pass 1
(`ORG`, `DS`), check `rsv` and error out if unresolved.

## Testing

There is no committed test corpus in this repo. The CI workflow only
builds the binary across Linux/macOS/Windows; it doesn't run a test
suite. Local development relied on the user's external corpus in
`tests/asm_tests/` and `tests/asm_games/` (ignored by git), each
`.asm` paired with a reference `.bin`. The workflow was:

```sh
for d in tests/asm_tests tests/asm_games; do
  for a in "$d"/*.asm; do
    base=$(basename "$a" .asm)
    ./glic80asm -o "/tmp/out_$base.bin" "$a" || echo "FAIL $a"
    cmp -s "/tmp/out_$base.bin" "$d/$base.bin" || echo "DIFF $a"
  done
done
```

When sources use `\\` or `\n` expecting escape decoding, add `-ee`.

If you add a feature that could change output, run the corpus loop in
both default and `-ee` modes before claiming the change is safe. Note
that one or two corpus reference binaries may be incorrect themselves
(`rndchrs.bin` was 0 bytes when checked) — don't trust ref blindly,
inspect the diff first.

## What's deliberately out of scope (v1)

These have been declined, not forgotten:

- Macros (`MACRO/ENDM`)
- `IF`/`ELSE`/`ENDIF` conditional assembly
- `INCLUDE` directive
- Output formats other than raw binary (Intel HEX, listing files,
  symbol map files)
- Undocumented opcodes other than `SLL`
- IXH/IXL/IYH/IYL half-register forms

If a user asks for one of these, the answer is "yes, but as a separate
feature with its own design pass" — not "shoehorn into existing code."

## Things to NOT change without asking

- The flag naming convention. Extension flags must stay under `-e*`.
- The default behavior of the lexer for backslash (literal). Changing
  the default re-breaks every source that doesn't use `-ee`.
- The two-pass model. The single-buffer `out[]` and the `emit_byte`
  pass-awareness are what make the encoder code uniform.
- The 64K output buffer. The Z80 address space is 64K; if a future
  feature needs banking, design it as an extension, don't grow `out[]`.

## CI workflow

`.github/workflows/build.yml` builds for Linux (Ubuntu), macOS (arm64,
M-series runners), and Windows (via MSYS2 MinGW64). Artifacts are
uploaded per OS. Path filter limits runs to changes under `src/`,
`Makefile`, or the workflow itself — README/IO.md/AGENTS.md edits
don't burn CI minutes.

Action versions in use (verified May 2026):

- `actions/checkout@v6`
- `actions/upload-artifact@v7`
- `msys2/setup-msys2@v2`

When bumping these, check the release notes for breaking changes
(notably `upload-artifact` had a v3→v4 incompatible bump; v7 adds
non-zipped artifacts behind `archive: false`).

## Cross-platform notes

The C code is pure C99 + libc, no POSIX includes, no `unistd.h`. Should
build cleanly with gcc/clang/MSVC. The Makefile uses GNU make + the
`install` command — fine on Linux and macOS, needs MSYS2 / WSL on
Windows.

CRLF line endings in source files work: the parser splits on `\n` and
the lexer breaks on `\r` (treated like a comment terminator), so a CR
at the end of a CRLF line is harmless.
