#!/bin/sh
# Mutation testing with Mull: changes argh.h in small ways (< to <=, a call
# removed, a constant replaced) one at a time and runs the tests on each. A
# mutant the tests don't notice "survived": a mistake there would go unseen.
# Prints the survivors and the share caught (mutation score), and fails below
# the minimum. Needs clang and, on first use, downloads Mull for its version
# (Ubuntu 24.04, x86-64 or ARM64) into $MULL_DIR.
# Usage: sh tests/mutation.sh [min-percent]
set -e
MIN=${1:-87}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
MULL_VERSION=0.34.1
MULL_DIR=${MULL_DIR:-$HOME/.cache/mull}
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

LLVM=$(clang --version | sed -n 's/.*clang version \([0-9]*\)\..*/\1/p' | head -n 1)
PLUGIN="$MULL_DIR/usr/lib/mull-ir-frontend-$LLVM"
RUNNER="$MULL_DIR/usr/bin/mull-runner-$LLVM"
if [ ! -x "$RUNNER" ]; then
    case $(uname -m) in
        aarch64 | arm64) arch=aarch64 ;;
        *) arch=amd64 ;;
    esac
    llvm_full=$(clang --version | sed -n 's/.*clang version \([0-9.]*\).*/\1/p' | head -n 1)
    # Mull's packages name the LLVM release they were built with
    url=$(curl -fsSL "https://api.github.com/repos/mull-project/mull/releases/tags/$MULL_VERSION" |
        grep -o "https://[^\"]*/Mull-$LLVM-$MULL_VERSION-LLVM-[0-9.]*-ubuntu-$arch-24.04.deb" | head -n 1)
    [ -n "$url" ] || { echo "no Mull $MULL_VERSION package for LLVM $LLVM ($llvm_full) on $arch" >&2; exit 1; }
    mkdir -p "$MULL_DIR"
    curl -fsSL -o "$OUT/mull.deb" "$url"
    dpkg -x "$OUT/mull.deb" "$MULL_DIR"
fi

# mull.yml in the repository root says what to mutate: argh.h only
cd "$ROOT"
clang -std=c99 -O0 -g -grecord-command-line -fpass-plugin="$PLUGIN" -o "$OUT/test_argh" tests/test_argh.c
cd "$OUT"
# The runner exits non-zero whenever a mutant survives; the score decides here
"$RUNNER" --reporters=IDE --no-output ./test_argh >"$OUT/log" 2>&1 || true
grep -q "Mutation score:" "$OUT/log" || { cat "$OUT/log"; exit 1; }

# One survivor per line: argh.h:LINE: what was changed
grep "Survived:" "$OUT/log" | sed -E 's|^.*/(argh\.h:[0-9]+):[0-9]+: warning: Survived: (.*) \[.*$|  \1: \2|' | sort -t: -k2,2n
score=$(sed -n 's/.*Mutation score: \([0-9]*\)%.*/\1/p' "$OUT/log")
survivors=$(grep -c "Survived:" "$OUT/log" || true)
echo "argh.h: $survivors mutants survived, mutation score $score%, minimum $MIN%"
[ "$score" -ge "$MIN" ]
