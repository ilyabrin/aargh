#!/bin/sh
# Line coverage of argh.h by the unit tests, debug and release builds merged.
# Fails below the threshold. Usage: sh tests/coverage.sh [min-percent]
# Needs gcc and gcov.
set -e
MIN=${1:-98}
ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

# Debug builds run the definition checks, release builds the paths those
# checks would stop first; the union is what the tests reach
for build in debug release; do
    mkdir -p "$OUT/$build"
    flags=
    [ "$build" = release ] && flags=-DNDEBUG
    # The tests run from the repository root, where their expected files are
    (cd "$OUT/$build" && gcc -std=c99 -O0 --coverage $flags -I"$ROOT" -o t "$ROOT/tests/test_argh.c" -lm)
    (cd "$ROOT" && "$OUT/$build/t" >/dev/null)
    (cd "$OUT/$build" && gcov -o . t-test_argh.gcda >/dev/null)
done

# A .gcov line is "count: line: source"; '-' is not code, '#####' never ran
awk -F: '
    FNR == 1 { file++ }
    {
        count = $1; gsub(/ /, "", count); line = $2 + 0
        if (line == 0 || count == "-") next
        code[line] = 1
        if (count != "#####" && count != "=====") ran[line] = 1
        if (!(line in src)) { s = $0; sub(/^[^:]*:[^:]*:/, "", s); src[line] = s }
    }
    END {
        for (l in code) { total++; if (l in ran) hit++; else missed[l] = 1 }
        n = 0
        for (l in missed) idx[++n] = l + 0
        for (i = 2; i <= n; i++) { v = idx[i]; for (j = i - 1; j > 0 && idx[j] > v; j--) idx[j + 1] = idx[j]; idx[j + 1] = v }
        for (i = 1; i <= n; i++) printf "  not run: argh.h:%d %s\n", idx[i], src[idx[i]]
        pct = 100 * hit / total
        printf "argh.h: %d of %d lines run by the tests (%.2f%%), minimum %s%%\n", hit, total, pct, min
        exit pct < min ? 1 : 0
    }' min="$MIN" "$OUT/debug/argh.h.gcov" "$OUT/release/argh.h.gcov"
