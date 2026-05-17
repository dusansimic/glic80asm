#!/usr/bin/env bash
# Assemble every .asm under tests/asm_tests and tests/asm_games and compare
# the result against either the committed .bin or a sha256 checksum file.
# Reports PASS / DIFF / FAIL per file; exits non-zero on any mismatch.
#
# Usage:
#   tests/run_corpus.sh                       # default: bin mode
#   tests/run_corpus.sh --mode sha256         # check against tests/checksums.sha256
#   tests/run_corpus.sh --regen-sums          # rebuild checksums file from .bin files
#   tests/run_corpus.sh PATTERN               # only files matching PATTERN
#
# Env:
#   GLIC80ASM=/path/to/glic80asm
#   FLAGS="-ee"                               # extra flags for every assemble
#   SUMS=/path/to/checksums.sha256            # override default location
set -u

script_dir="$(cd "$(dirname "$0")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
glic80asm="${GLIC80ASM:-$repo_root/glic80asm}"
flags="${FLAGS:-}"
sums="${SUMS:-$script_dir/checksums.sha256}"

mode="bin"
regen=0
pattern=""

while (( $# )); do
    case "$1" in
        --mode)        mode="$2"; shift 2 ;;
        --mode=*)      mode="${1#*=}"; shift ;;
        --regen-sums)  regen=1; shift ;;
        -h|--help)
            sed -n '2,16p' "$0"; exit 0 ;;
        -*)
            echo "unknown option: $1" >&2; exit 2 ;;
        *)
            if [ -n "$pattern" ]; then
                echo "multiple patterns not supported" >&2; exit 2
            fi
            pattern="$1"; shift ;;
    esac
done

case "$mode" in
    bin|sha256) ;;
    *) echo "unknown --mode '$mode' (want bin or sha256)" >&2; exit 2 ;;
esac

if ! command -v sha256sum >/dev/null && [ "$mode" = "sha256" -o "$regen" = "1" ]; then
    echo "sha256sum not in PATH" >&2; exit 2
fi

# --- regen mode: walk every .bin and write a sha256sum-format file ---
if [ "$regen" = "1" ]; then
    : > "$sums"
    for dir in "$script_dir/asm_tests" "$script_dir/asm_games"; do
        [ -d "$dir" ] || continue
        for ref in "$dir"/*.bin; do
            [ -f "$ref" ] || continue
            rel="${ref#"$repo_root"/}"
            sum="$(sha256sum "$ref" | awk '{print $1}')"
            printf '%s  %s\n' "$sum" "$rel" >> "$sums"
        done
    done
    sort -k2 -o "$sums" "$sums"
    n="$(wc -l < "$sums")"
    echo "wrote $n hashes to $sums"
    exit 0
fi

if [ ! -x "$glic80asm" ]; then
    echo "glic80asm not found at $glic80asm (run: make)" >&2; exit 2
fi

# --- sha256 mode: pre-load expected hashes into associative-array form ---
expected_for_path() {
    local key="$1"
    awk -v k="$key" '$2 == k { print $1; exit }' "$sums"
}

if [ "$mode" = "sha256" ] && [ ! -f "$sums" ]; then
    echo "checksum file not found: $sums (try --regen-sums)" >&2; exit 2
fi

tmp="$(mktemp -d)"
trap "rm -rf '$tmp'" EXIT

ok=0; diff=0; fail=0; missing=0
declare -a diffs=()

for dir in "$script_dir/asm_tests" "$script_dir/asm_games"; do
    [ -d "$dir" ] || continue
    for src in "$dir"/*.asm; do
        [ -f "$src" ] || continue
        base="$(basename "$src" .asm)"
        rel="${src#"$repo_root"/}"

        if [ -n "$pattern" ] && [[ "$rel" != *"$pattern"* ]]; then
            continue
        fi

        ref="$dir/$base.bin"
        ref_rel="${ref#"$repo_root"/}"
        out="$tmp/$base.bin"

        if ! "$glic80asm" $flags -o "$out" "$src" 2>"$tmp/$base.err"; then
            printf 'FAIL  %s\n' "$rel"
            sed 's/^/      /' "$tmp/$base.err"
            fail=$((fail + 1))
            continue
        fi

        case "$mode" in
            bin)
                if [ ! -f "$ref" ]; then
                    printf 'NOREF %s\n' "$rel"
                    missing=$((missing + 1))
                    continue
                fi
                if cmp -s "$out" "$ref"; then
                    printf 'PASS  %s\n' "$rel"
                    ok=$((ok + 1))
                else
                    mine_sz="$(wc -c < "$out")"
                    ref_sz="$(wc -c < "$ref")"
                    printf 'DIFF  %s (mine=%s ref=%s)\n' "$rel" "$mine_sz" "$ref_sz"
                    diff=$((diff + 1))
                fi
                ;;
            sha256)
                expected="$(expected_for_path "$ref_rel")"
                if [ -z "$expected" ]; then
                    printf 'NOREF %s (no entry in %s)\n' "$rel" "${sums#"$repo_root"/}"
                    missing=$((missing + 1))
                    continue
                fi
                got="$(sha256sum "$out" | awk '{print $1}')"
                if [ "$got" = "$expected" ]; then
                    printf 'PASS  %s\n' "$rel"
                    ok=$((ok + 1))
                else
                    printf 'DIFF  %s\n' "$rel"
                    printf '      mine: %s\n' "$got"
                    printf '      ref:  %s\n' "$expected"
                    diff=$((diff + 1))
                fi
                ;;
        esac
    done
done

total=$((ok + diff + fail + missing))
echo
printf 'summary [%s]: %d/%d match' "$mode" "$ok" "$total"
[ "$diff"    -gt 0 ] && printf ', %d diff'   "$diff"
[ "$fail"    -gt 0 ] && printf ', %d failed' "$fail"
[ "$missing" -gt 0 ] && printf ', %d no ref' "$missing"
echo

if [ "$diff" -gt 0 ] || [ "$fail" -gt 0 ]; then
    exit 1
fi
exit 0
