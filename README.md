# glic80asm

A standalone Z80 assembler written in C99. Targets the raw Z80 CPU (no
specific machine/platform glue) and produces a flat binary image.

## Build

```sh
make
```

Requires a C99 compiler and GNU make. No third-party dependencies.

## Install

```sh
make install               # installs to $HOME/.local/bin
make install PREFIX=/usr/local   # system-wide (likely needs sudo)
make uninstall
```

Installs two commands into `$PREFIX/bin` (default `$HOME/.local/bin`):

- `glic80asm` — the assembler binary
- `glic80compile` — the C-to-binary wrapper (`tools/compile.sh` renamed)

`PREFIX` overrides the install root; `DESTDIR` is honoured for staged
installs (packaging).

## Usage

```
glic80asm [-o out.bin] [-l] [-e<flag>...] input.asm
  -o PATH   output file (default: a.bin)
  -l        list symbols on stderr after assembly
  -h        help
  -e<flag>  enable an extension (see "Extensions" below)
```

Output is a flat binary trimmed to the range of emitted bytes — if the
program starts at `ORG $100`, the output file's offset 0 corresponds to
address `$100`. No padding from address 0.

Exits non-zero on any error; diagnostics go to stderr as
`file:line: error: message`.

## Source language

### Lexical

| Form          | Meaning                          |
|---------------|----------------------------------|
| `$FF`, `0xFF` | Hexadecimal literal              |
| `0FFh`        | Hexadecimal literal (Intel/Zilog suffix; must start with a digit) |
| `%1010`       | Binary literal                   |
| `10101010b`   | Binary literal (suffix form)     |
| `123`         | Decimal literal                  |
| `'A'`         | Character literal (one byte)     |
| `"hello"`     | String literal (in `DB`)         |
| `$`           | Current program counter          |
| `; ...`       | Comment to end of line           |

By default, a backslash inside a string or character literal is a normal
byte — `"a\b"` is three bytes. Pass `-ee` to enable C-style escapes (see
[Extensions](#extensions)).

Identifiers may contain letters, digits, `_`, and `.`. Identifiers,
mnemonics, register names, and directives are case-insensitive.

A local label starts with `.` and is automatically scoped to the most
recently defined non-local label. Multiple non-local labels can each have
their own `.loop`, `.exit`, etc., without colliding.

### Expressions

Full C-style expression grammar with precedence (low → high):

```
|   ^   &   << >>   + -   * / %   unary + - ~   primary
```

Primaries: numeric literals, character literals, identifiers, `$` (current PC),
parenthesised sub-expressions.

### Directives

| Directive            | Meaning                                                            |
|----------------------|--------------------------------------------------------------------|
| `ORG expr`           | Set the program counter. The first `ORG` also sets the origin.     |
| `FORG expr`          | Alias for `ORG`.                                                   |
| `DB`, `DEFB`         | Emit bytes (expressions or string literals, comma-separated).      |
| `DW`, `DEFW`         | Emit little-endian 16-bit words.                                   |
| `DS`, `DEFS`         | Reserve N bytes (zero-filled by default; second arg = fill byte).  |
| `name EQU expr`      | Bind a constant. Also accepted as `name: EQU expr`.                |
| `END`                | Stop processing the file at this point.                            |

### Labels

```
label:        ld a, b
.loop:        djnz .loop          ; scoped to previous non-local label
const EQU 42
```

Register names (`A B C D E H L I R BC DE HL SP AF AF' IX IY`) and condition
codes (`NZ Z NC C PO PE P M`) are reserved and cannot be used as labels.

### Instruction set

The full documented Z80 instruction set is supported, including:

- 8- and 16-bit `LD` in all addressing modes, including `LD r,(IX+d)` and friends
- All ALU ops (`ADD ADC SUB SBC AND OR XOR CP`) with register, immediate, `(HL)`, `(IX+d)`, `(IY+d)` operands
- 16-bit arithmetic: `ADD HL,rp`, `ADC HL,rp`, `SBC HL,rp`, `ADD IX,rp`, `ADD IY,rp`
- `INC`/`DEC` for 8-bit registers, memory, and 16-bit register pairs
- `PUSH`/`POP` for `BC DE HL AF IX IY`
- All rotates/shifts (`RLC RRC RL RR SLA SRA SLL SRL`) on registers, `(HL)`, indexed
- Bit ops: `BIT`, `RES`, `SET` (registers and indexed)
- All jumps, calls, returns including conditional forms
- `RST` `0/8H/10H/18H/20H/28H/30H/38H`
- Block ops: `LDI LDD LDIR LDDR CPI CPD CPIR CPDR`
- I/O: `IN A,(n)`, `IN r,(C)`, `OUT (n),A`, `OUT (C),r`, `INI INIR IND INDR OUTI OTIR OUTD OTDR`
- `EX DE,HL`, `EX AF,AF'`, `EX (SP),HL/IX/IY`, `EXX`
- Misc: `NOP HALT DI EI IM 0/1/2 NEG CPL CCF SCF DAA RLD RRD RETI RETN`

Undocumented `SLL` is recognised. Other undocumented opcodes and the
IXH/IXL/IYH/IYL half-register forms are not supported.

## Extensions

CLI flags whose name starts with `-e` opt into non-default lexer or
assembler features. The default behaviour is the most compatible
interpretation; extensions trade portability for ergonomics.

| Flag  | Effect                                                                 |
|-------|------------------------------------------------------------------------|
| `-ee` | In string and character literals, decode `\n \t \r \0 \\ \" \'`. Any other `\X` collapses to `X`. Without this flag, backslash is a literal byte. |
| `-ec` | SDCC / ASxxxx compatibility: accept `.module / .optsdcc / .globl / .area` as no-op directives, `.db / .dw / .ds` as aliases, `.ascii "..."` (raw bytes) and `.asciz "..."` (null-terminated), `label::` double-colon exports, SDCC numeric labels (`00104$:`) scoped under the previous non-local label, `#expr` immediate prefix, single `<expr` (low byte) and `>expr` (high byte) unary ops, `disp (ix)` / `disp (iy)` indexed addressing, and `$` inside identifiers. Lets glic80asm consume `sdcc-sdcc -mz80 -S` output. |

Unknown `-e<x>` flags are an error.

## Compiling C with SDCC

If [SDCC](http://sdcc.sourceforge.net/) is installed, the wrappers under
`tools/` turn a single C file into a flat binary:

```sh
tools/compile.sh program.c                # -> program.bin
tools/compile.sh program.c -o build/p.bin
tools/compile.sh program.c --stack 0x7700 --asm-out program.asm
```

The Windows equivalent is `tools\compile.bat`. Both run
`sdcc-sdcc -mz80 -S → prepend a startup stub → glic80asm -ec`. The
startup stub does:

```asm
    ld   sp, <stack>     ; default 0x7700
    call _main
__glic80_halt:
    jr __glic80_halt
```

so a freestanding `void main(void)` is enough to produce a runnable
image. Multi-file C programs need to be concatenated before invoking
the wrapper — glic80asm does not link object files.

A C runtime library matching the GLIČ80 hardware (`glic80.h` with the
button-read and screen-draw helpers documented in [IO.md](IO.md)) is
not shipped here yet; programs that depend on one will fail at the
SDCC stage with a missing-header error.

## Project layout

```
src/
  asm.h, asm.c          Shared types (AsmCtx), emit_byte/word helpers
  lexer.h, lexer.c      Line tokenizer
  symtab.h, symtab.c    Case-insensitive symbol table
  expr.h, expr.c        Recursive-descent expression parser
  encoder.h, encoder.c  Mnemonic dispatch + instruction encoder
  parser.h, parser.c    Line dispatcher (labels, directives, instructions)
  main.c                CLI driver, two-pass orchestration
Makefile
```

## Limitations

- No macros, `IF/ELSE/ENDIF`, or `INCLUDE` directive in v1
- Output formats other than raw binary (Intel HEX, listing files, symbol files) are not produced
- No undocumented IXH/IXL/IYH/IYL half-register forms
- Expressions used as `DS` counts or `ORG` addresses must resolve in pass 1 (cannot depend on labels defined later)

## Acknowledgements

- The Z80 programs under [tests/asm_tests/](tests/asm_tests/) and
  [tests/asm_games/](tests/asm_games/) — the regression corpus this
  assembler is validated against — were written by **Gliša**.
- The C games under [tests/c_games/](tests/c_games/) were written by
  **Korizma**, who also helped adapt the assembler to consume SDCC
  output (the `-ec` extension).
