#!/bin/sh
# Builds tests/package/app.c against aargh in every supported way and runs it:
# CMake (install + find_package, FetchContent, add_subdirectory), pkg-config,
# Conan (conan create) and a Meson subproject; checks package.json for clib.
# Ways whose tool is missing are skipped, unless
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
    # The header goes to include/aargh, where no other argh.h can be
    for f in include/aargh/argh.h share/cmake/aargh/aarghConfig.cmake share/cmake/aargh/aarghConfigVersion.cmake \
        share/pkgconfig/aargh.pc share/cmake/argh/arghConfig.cmake share/cmake/argh/arghConfigVersion.cmake; do
        [ -f "$OUT/prefix/$f" ] || bad "installed $f"
    done
    if [ -f "$OUT/prefix/include/argh.h" ]; then bad "argh.h installed outside include/aargh"; fi

    # aargh, and the deprecated v1.8 name argh
    for pkg in aargh argh; do
        for via in find_package fetchcontent subdirectory; do
            b="$OUT/$pkg-$via"
            if cmake -S "$HERE" -B "$b" -DARGH_VIA=$via -DARGH_NAME=$pkg -DARGH_SOURCE="$ROOT" \
                -DCMAKE_PREFIX_PATH="$OUT/prefix" >"$OUT/log" 2>&1 &&
                cmake --build "$b" >>"$OUT/log" 2>&1; then
                run "cmake $via ($pkg::$pkg)" "$(exe "$b/app" "$b")"
            else
                cat "$OUT/log"
                bad "cmake $via ($pkg::$pkg)"
            fi
        done
    done

    # Registries install without the v1.8 name
    cmake -S "$ROOT" -B "$OUT/nolegacy-build" -DARGH_BUILD_TESTS=OFF -DARGH_INSTALL_LEGACY_NAME=OFF >"$OUT/log" 2>&1 &&
        cmake --install "$OUT/nolegacy-build" --prefix "$OUT/nolegacy" >>"$OUT/log" 2>&1 || { cat "$OUT/log"; bad "install without legacy name"; }
    if [ -d "$OUT/nolegacy/share/cmake/argh" ]; then bad "ARGH_INSTALL_LEGACY_NAME=OFF still installs share/cmake/argh"; else ok "ARGH_INSTALL_LEGACY_NAME=OFF"; fi

    # SameMajorVersion: asking for the next major version must not find 1.x
    major=$(echo "$VERSION" | cut -d. -f1)
    mkdir -p "$OUT/major-src"
    printf 'cmake_minimum_required(VERSION 3.14)\nproject(m LANGUAGES C)\nfind_package(aargh %s QUIET)\nif(aargh_FOUND)\n  message(FATAL_ERROR "found")\nendif()\n' \
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
elif pkg-config --version >/dev/null 2>&1 && [ -f "$OUT/prefix/share/pkgconfig/aargh.pc" ]; then
    flags=$(PKG_CONFIG_PATH="$OUT/prefix/share/pkgconfig" pkg-config --cflags aargh)
    pcver=$(PKG_CONFIG_PATH="$OUT/prefix/share/pkgconfig" pkg-config --modversion aargh)
    [ "$pcver" = "$VERSION" ] || bad "pkg-config version $pcver"
    if ${CC:-cc} -std=c99 $flags -o "$OUT/pc-app" "$HERE/app.c"; then run "pkg-config" "$(exe "$OUT/pc-app")"; else bad "pkg-config build"; fi
else
    skip "pkg-config" pkg-config
fi

# clib reads package.json from the repository; its version must follow argh.h
if grep -q "\"version\": \"$VERSION\"" "$ROOT/package.json"; then ok "package.json version"; else bad "package.json version is not $VERSION"; fi

CONAN=${CONAN:-conan}
if command -v "$CONAN" >/dev/null 2>&1; then
    # Builds the package from this checkout and runs test_package against it
    if (cd "$ROOT" && "$CONAN" create . --build=missing) >"$OUT/log" 2>&1 && grep -q "$EXPECT" "$OUT/log"; then
        ok "conan create"
    else
        cat "$OUT/log"
        bad "conan create"
    fi
else
    skip "conan create" conan
fi

MESON=${MESON:-meson}
if command -v "$MESON" >/dev/null 2>&1; then
    m="$OUT/meson-src"
    mkdir -p "$m/subprojects/aargh"
    cp "$HERE/app.c" "$HERE/meson.build" "$m/"
    cp "$ROOT/argh.h" "$ROOT/meson.build" "$m/subprojects/aargh/"
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
