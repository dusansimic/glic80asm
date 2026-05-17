#!/usr/bin/env bash
# Pipeline: sdcc -> preprocess (prepend startup stub) -> glic80asm -ec
#
# Usage:
#   tools/compile.sh program.c [-o program.bin] [--stack 0x7700] [--asm-out program.asm]
#
# Env overrides:
#   SDCC=...        path to sdcc binary (default: sdcc-sdcc or sdcc on PATH)
#   GLIC80ASM=...   path to glic80asm binary (default: ./glic80asm next to this script)
set -euo pipefail

src=""
out=""
stack="0x7700"
asm_out=""

while (( $# )); do
    case "$1" in
        -o)         out="$2"; shift 2 ;;
        --stack)    stack="$2"; shift 2 ;;
        --asm-out)  asm_out="$2"; shift 2 ;;
        -h|--help)
            sed -n '2,9p' "$0"; exit 0 ;;
        -*)
            echo "unknown option: $1" >&2
            exit 2 ;;
        *)
            if [ -n "$src" ]; then
                echo "multiple inputs not supported" >&2
                exit 2
            fi
            src="$1"; shift ;;
    esac
done

if [ -z "$src" ]; then
    echo "usage: $(basename "$0") program.c [-o program.bin] [--stack 0x7700] [--asm-out program.asm]" >&2
    exit 2
fi
[ -n "$out" ] || out="${src%.c}.bin"

sdcc="${SDCC:-}"
if [ -z "$sdcc" ]; then
    sdcc=$(command -v sdcc-sdcc || command -v sdcc || true)
fi
if [ -z "$sdcc" ]; then
    echo "sdcc not found; install sdcc or set SDCC=/path/to/sdcc" >&2
    exit 2
fi

# Default glic80asm is the binary next to this script's repo root.
script_dir="$(cd "$(dirname "$0")" && pwd)"
glic80asm="${GLIC80ASM:-$script_dir/../glic80asm}"
if [ ! -x "$glic80asm" ]; then
    # fall back to PATH
    glic80asm=$(command -v glic80asm || true)
fi
if [ -z "$glic80asm" ] || [ ! -x "$glic80asm" ]; then
    echo "glic80asm not found; build it (make) or set GLIC80ASM=/path/to/glic80asm" >&2
    exit 2
fi

tmp=$(mktemp -d)
trap "rm -rf '$tmp'" EXIT

# 1) C -> Z80 assembly via SDCC.
"$sdcc" -mz80 -S -o "$tmp/prog.asm" "$src"

# 2) Prepend a startup stub so the CPU jumps into _main on reset.
cat > "$tmp/full.asm" <<EOF
    org \$0000
    ld  sp, $stack
    call _main
__glic80_halt:
    jr __glic80_halt
EOF
cat "$tmp/prog.asm" >> "$tmp/full.asm"

if [ -n "$asm_out" ]; then
    cp "$tmp/full.asm" "$asm_out"
fi

# 3) Assemble with SDCC compatibility enabled.
"$glic80asm" -ec -o "$out" "$tmp/full.asm"
