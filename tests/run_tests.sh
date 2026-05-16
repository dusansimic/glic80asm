#!/usr/bin/env bash
# Smoke test: assemble tests/smoke.asm and diff against the committed golden.
# Regenerate golden by running with REGEN=1 (requires z80asm in PATH).
set -e
cd "$(dirname "$0")/.."

if [ "${REGEN:-0}" = "1" ]; then
    if ! command -v z80asm >/dev/null; then
        echo "REGEN=1 but z80asm not found" >&2
        exit 2
    fi
    z80asm -o tests/expected.bin tests/smoke.asm
    echo "regenerated tests/expected.bin ($(stat -c%s tests/expected.bin) bytes)"
fi

./glic80asm -o tests/out.bin tests/smoke.asm
if cmp -s tests/expected.bin tests/out.bin; then
    echo "OK ($(stat -c%s tests/out.bin) bytes match)"
    exit 0
fi
echo "FAIL: tests/out.bin differs from tests/expected.bin" >&2
cmp -l tests/expected.bin tests/out.bin | head -20 >&2
exit 1
