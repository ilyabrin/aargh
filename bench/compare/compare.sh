#!/bin/sh
# Compares argh with getopt_long, cargs and argparse: speed, behavior on
# tricky input, and code size. Fetches cargs and argparse at the versions
# below (needs git and network). Results feed COMPARISON.md.
# Usage: sh bench/compare/compare.sh [compiler]   (default: cc, Linux)
set -e
CC=${1:-cc}
CARGS_TAG=v1.2.0
ARGPARSE_TAG=v1.1.0

DIR=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$DIR/../.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT

git clone -q --depth 1 --branch "$CARGS_TAG" https://github.com/likle/cargs "$OUT/cargs" 2>/dev/null
git clone -q --depth 1 --branch "$ARGPARSE_TAG" https://github.com/cofyc/argparse "$OUT/argparse" 2>/dev/null
INC="-I$OUT/cargs/include -I$OUT/argparse"
LIBS="$OUT/cargs/src/cargs.c $OUT/argparse/argparse.c"

echo "Compiler: $($CC --version | head -n 1)"
echo "cargs $CARGS_TAG, argparse $ARGPARSE_TAG"
echo

echo "== Speed (30 options, 17 arguments; -O2 -DNDEBUG)"
$CC -std=c11 -O2 -DNDEBUG $INC -o "$OUT/speed" "$DIR/speed.c" $LIBS -lm
"$OUT/speed"
echo

echo "== Behavior (one integer option -j/--jobs)"
$CC -std=c11 -O2 $INC -o "$OUT/behavior" "$DIR/behavior.c" $LIBS
for args in "--jobs 8" "--jobs 010" "--jobs 99999999999" "--jobs 5x" "--jobs=" \
            "--jbos 3" "-j=3" "--jobs"; do
    echo "\$ tool $args"
    for lib in argh cargs argparse; do
        # shellcheck disable=SC2086
        set +e
        text=$("$OUT/behavior" $lib $args 2>&1 | head -n 1)
        code=$("$OUT/behavior" $lib $args >/dev/null 2>&1; echo $?)
        set -e
        printf '  %-9s exit %-3s %s\n' "$lib" "$code" "$text"
    done
done
echo "(cargs returns strings and leaves checking them to the program; behavior.c uses atoi)"
echo

echo "== Code size (.text added to a 3-option program; -Os, gc-sections)"
FLAGS="-std=c99 -Os -ffunction-sections -fdata-sections -Wl,--gc-sections"
text() { size "$1" | awk 'NR==2 {print $1}'; }
$CC $FLAGS -o "$OUT/none" "$ROOT/bench/size_none.c"
base=$(text "$OUT/none")
$CC $FLAGS -o "$OUT/argh" "$ROOT/bench/size_argh.c"
$CC $FLAGS -DARGH_NO_COMMANDS -DARGH_NO_SUGGEST -o "$OUT/argh_reduced" "$ROOT/bench/size_argh.c"
$CC $FLAGS $INC -o "$OUT/cargs_size" "$DIR/size_cargs.c" "$OUT/cargs/src/cargs.c"
$CC $FLAGS $INC -o "$OUT/argparse_size" "$DIR/size_argparse.c" "$OUT/argparse/argparse.c" -lm
printf '  %-14s %7s B\n' "argh" $(( $(text "$OUT/argh") - base ))
printf '  %-14s %7s B\n' "argh reduced" $(( $(text "$OUT/argh_reduced") - base ))
printf '  %-14s %7s B\n' "cargs" $(( $(text "$OUT/cargs_size") - base ))
printf '  %-14s %7s B\n' "argparse" $(( $(text "$OUT/argparse_size") - base ))

if command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    echo
    echo "== Firmware (Cortex-M0, newlib-nano; flash added to the same program)"
    AFLAGS="-mcpu=cortex-m0 -mthumb -std=c99 -Os -DNDEBUG -ffunction-sections -fdata-sections
            -Wl,--gc-sections --specs=nano.specs --specs=nosys.specs"
    flash() { arm-none-eabi-size "$1" | awk 'NR==2 {print $1 + $2}'; }
    arm() { arm-none-eabi-gcc $AFLAGS "$@" >/dev/null 2>&1; }
    arm -o "$OUT/n.elf" "$ROOT/bench/size_none.c"
    base=$(flash "$OUT/n.elf")
    arm -o "$OUT/a.elf" "$ROOT/bench/size_argh.c"
    arm -DARGH_NO_FLOAT -o "$OUT/af.elf" "$ROOT/bench/size_argh.c"
    arm $INC -o "$OUT/c.elf" "$DIR/size_cargs.c" "$OUT/cargs/src/cargs.c"
    arm $INC -o "$OUT/p.elf" "$DIR/size_argparse.c" "$OUT/argparse/argparse.c" -lm
    printf '  %-20s %7s B\n' "argh" $(( $(flash "$OUT/a.elf") - base ))
    printf '  %-20s %7s B\n' "argh ARGH_NO_FLOAT" $(( $(flash "$OUT/af.elf") - base ))
    printf '  %-20s %7s B\n' "cargs" $(( $(flash "$OUT/c.elf") - base ))
    printf '  %-20s %7s B\n' "argparse" $(( $(flash "$OUT/p.elf") - base ))
fi
