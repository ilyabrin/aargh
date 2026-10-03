#!/bin/sh
# Builds tests/package/app.c against argh in every supported way and runs it:
# CMake (install + find_package, FetchContent, add_subdirectory), pkg-config
# and a Meson subproject. Ways whose tool is missing are skipped, unless
# ARGH_PACKAGE_STRICT=1 (CI), where a missing tool is a failure.
# Usage: sh tests/package/check.sh
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
HERE="$ROOT/tests/package"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
# tr: a Windows checkout may have CRLF line endings
VERSION=$(tr -d '\r' <"$ROOT/argh.h" | sed -n 's/^#define ARGH_VERSION "\(.*\)"/\1/p')
EXPECT="argh $VERSION jobs=3"
failed=0

ok() { echo "ok    $1"; }
bad() { echo "FAIL  $1"; failed=1; }
skip() {
    if [ "${ARGH_PACKAGE_STRICT:-0}" = 1 ]; then bad "$1 ($2 not found)"; else echo "skip  $1 ($2 not found)"; fi
}
# The built program must parse and report the version of this argh.h
run() {
    name=$1
    shift
    if [ "$("$@" -j 3 2>&1)" = "$EXPECT" ]; then ok "$name"; else bad "$name: $("$@" -j 3 2>&1)"; fi
}
exe() { for f in "$1" "$1.exe" "$2/Debug/app.exe"; do [ -f "$f" ] && { echo "$f"; return; }; done; echo "$1"; }

if command -v cmake >/dev/null 2>&1; then
    # Install, then find_package from the prefix; tests stay out of it
    cmake -S "$ROOT" -B "$OUT/argh-build" -DARGH_BUILD_TESTS=OFF >"$OUT/log" 2>&1 &&
        cmake --install "$OUT/argh-build" --prefix "$OUT/prefix" >>"$OUT/log" 2>&1 ||
        { cat "$OUT/log"; bad "cmake --install"; }
    for f in include/argh.h share/cmake/argh/arghConfig.cmake share/cmake/argh/arghConfigVersion.cmake share/pkgconfig/argh.pc; do
        [ -f "$OUT/prefix/$f" ] || bad "installed $f"
    done

    for via in find_package fetchcontent subdirectory; do
        b="$OUT/$via"
        if cmake -S "$HERE" -B "$b" -DARGH_VIA=$via -DARGH_SOURCE="$ROOT" -DCMAKE_PREFIX_PATH="$OUT/prefix" >"$OUT/log" 2>&1 &&
            cmake --build "$b" >>"$OUT/log" 2>&1; then
            run "cmake $via" "$(exe "$b/app" "$b")"
        else
            cat "$OUT/log"
            bad "cmake $via"
        fi
    done

    # SameMajorVersion: asking for the next major version must not find 1.x
    major=$(echo "$VERSION" | cut -d. -f1)
    mkdir -p "$OUT/major-src"
    printf 'cmake_minimum_required(VERSION 3.14)\nproject(m LANGUAGES C)\nfind_package(argh %s QUIET)\nif(argh_FOUND)\n  message(FATAL_ERROR "found")\nendif()\n' \
        "$((major + 1))" >"$OUT/major-src/CMakeLists.txt"
    if cmake -S "$OUT/major-src" -B "$OUT/major" -DCMAKE_PREFIX_PATH="$OUT/prefix" >"$OUT/log" 2>&1; then
        ok "find_package($((major + 1))) does not take $VERSION"
    else
        cat "$OUT/log"
        bad "find_package($((major + 1))) took $VERSION"
    fi
else
    skip "cmake" cmake
fi

if ! command -v "${CC:-cc}" >/dev/null 2>&1; then
    skip "pkg-config" "${CC:-cc}"
# --version: some installs are present but broken (Strawberry Perl's under Git Bash)
elif pkg-config --version >/dev/null 2>&1 && [ -f "$OUT/prefix/share/pkgconfig/argh.pc" ]; then
    flags=$(PKG_CONFIG_PATH="$OUT/prefix/share/pkgconfig" pkg-config --cflags argh)
    pcver=$(PKG_CONFIG_PATH="$OUT/prefix/share/pkgconfig" pkg-config --modversion argh)
    [ "$pcver" = "$VERSION" ] || bad "pkg-config version $pcver"
    if ${CC:-cc} -std=c99 $flags -o "$OUT/pc-app" "$HERE/app.c"; then run "pkg-config" "$(exe "$OUT/pc-app")"; else bad "pkg-config build"; fi
else
    skip "pkg-config" pkg-config
fi

MESON=${MESON:-meson}
if command -v "$MESON" >/dev/null 2>&1; then
    m="$OUT/meson-src"
    mkdir -p "$m/subprojects/argh"
    cp "$HERE/app.c" "$HERE/meson.build" "$m/"
    cp "$ROOT/argh.h" "$ROOT/meson.build" "$m/subprojects/argh/"
    if "$MESON" setup "$OUT/meson-build" "$m" >"$OUT/log" 2>&1 && "$MESON" compile -C "$OUT/meson-build" >>"$OUT/log" 2>&1; then
        run "meson subproject" "$(exe "$OUT/meson-build/app")"
    else
        cat "$OUT/log"
        bad "meson subproject"
    fi
else
    skip "meson subproject" meson
fi

exit $failed
