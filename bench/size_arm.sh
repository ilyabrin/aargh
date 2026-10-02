#!/bin/sh
# Flash that argh adds to bare-metal ARM firmware, with size budgets.
# Builds bench/size_fw.c with and without the parser (newlib-nano, -Os,
# --gc-sections) and prints the difference. Exits 1 if a budget is exceeded.
# Usage: sh bench/size_arm.sh   (needs arm-none-eabi-gcc and newlib)
set -e
DIR=$(cd "$(dirname "$0")" && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
GCC=arm-none-eabi-gcc
FW="-DNDEBUG -DARGH_NO_STDIO"
FLAGS="-std=c99 -Os -Wall -Wextra -Werror -ffunction-sections -fdata-sections
       -Wl,--gc-sections --specs=nano.specs --specs=nosys.specs"

# Budgets in bytes of flash added, for the two firmware builds
BUDGET_FULL=12288     # ARGH_NO_FLOAT
BUDGET_REDUCED=10240  # ARGH_NO_FLOAT ARGH_NO_COMMANDS ARGH_NO_SUGGEST ARGH_HELP_WIDTH=0

flash() {
    # text + data: data is copied from flash at startup
    arm-none-eabi-size "$1" | awk 'NR==2 {print $1 + $2}'
}

# The nosys stubs warn at link time; they are the same with and without argh
build() {
    $GCC $FLAGS "$@" >"$OUT/log" 2>&1 || { cat "$OUT/log" >&2; exit 1; }
}

echo "Compiler: $($GCC --version | head -n 1)"
echo
printf '%-11s %-28s %9s %8s\n' "cpu" "build" "flash" "budget"
status=0
for cpu in cortex-m0 cortex-m4; do
    arch="-mcpu=$cpu -mthumb"
    build $arch -o "$OUT/none.elf" "$DIR/size_fw.c"
    base=$(flash "$OUT/none.elf")
    for build_name in with-double full reduced; do
        case $build_name in
            with-double) defs="$FW" budget= ;;
            full) defs="$FW -DARGH_NO_FLOAT" budget=$BUDGET_FULL ;;
            reduced) defs="$FW -DARGH_NO_FLOAT -DARGH_NO_COMMANDS -DARGH_NO_SUGGEST -DARGH_HELP_WIDTH=0" budget=$BUDGET_REDUCED ;;
        esac
        build $arch $defs -DARGH_PROBE_PARSER -o "$OUT/argh.elf" "$DIR/size_fw.c"
        added=$(( $(flash "$OUT/argh.elf") - base ))
        mark=
        if [ -n "$budget" ] && [ "$added" -gt "$budget" ]; then
            mark="  OVER BUDGET"
            status=1
        fi
        printf '%-11s %-28s %7s B %6s B%s\n' "$cpu" "$build_name" "$added" "${budget:--}" "$mark"
    done
done
echo
echo "with-double: ARGH_NO_STDIO only; strtod pulls in newlib's float parser and printf"
echo "full:        + ARGH_NO_FLOAT"
echo "reduced:     + ARGH_NO_FLOAT ARGH_NO_COMMANDS ARGH_NO_SUGGEST ARGH_HELP_WIDTH=0"
exit $status
