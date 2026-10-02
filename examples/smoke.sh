#!/bin/sh
# Runs the examples with real arguments and checks exit codes and output.
# Usage: sh examples/smoke.sh   (from the repository root, after make examples)

EXE=
case "$(uname -s)" in MINGW* | MSYS* | CYGWIN*) EXE=.exe ;; esac

WC=examples/wc$EXE
LOGSHIP=examples/logship$EXE
PKG=examples/pkg/pkg$EXE

failures=0
checks=0

# check <expected exit code> <text the output must contain> <command...>
check()
{
    want_code=$1
    want_text=$2
    shift 2
    checks=$((checks + 1))
    out=$("$@" 2>&1)
    code=$?
    if [ "$code" != "$want_code" ]; then
        echo "FAIL (exit $code, expected $want_code): $*"
        echo "$out" | sed 's/^/    /'
        failures=$((failures + 1))
    elif ! printf '%s\n' "$out" | grep -qF -- "$want_text"; then
        echo "FAIL (missing '$want_text'): $*"
        echo "$out" | sed 's/^/    /'
        failures=$((failures + 1))
    fi
}

# --- wc ---------------------------------------------------------------------
check 0 "Usage: wc [OPTIONS] [files...]"   $WC --help
check 0 "wc 1.0.0"                         $WC --version
check 0 " LICENSE"                         $WC LICENSE
check 0 "total"                            $WC LICENSE Makefile
check 1 "cannot open"                      $WC no-such-file
check 2 "did you mean '--lines'"           $WC --line LICENSE

# --- logship ----------------------------------------------------------------
check 0 "Plan: 2 files"                    $LOGSHIP --to logs.example.com:6514 LICENSE Makefile
check 0 "skip  Makefile (excluded)"        $LOGSHIP --to x:1 -v -x Make LICENSE Makefile
check 0 "timeout 30s (default)"            $LOGSHIP --to x:1 -vv LICENSE
check 0 "Would stream standard input"      $LOGSHIP --to x:1 --stdin
check 0 "(default: 64M)"                   $LOGSHIP --help
check 2 "missing required option '--to'"   $LOGSHIP LICENSE
check 2 "port must be a number"            $LOGSHIP --to x:99999 LICENSE
check 2 "expected a duration"              $LOGSHIP --to x:1 --timeout 5x LICENSE
check 2 "'--gzip' and '--zstd' cannot"     $LOGSHIP --to x:1 --gzip --zstd LICENSE
check 2 "'--tls-key' requires '--tls-cert'" $LOGSHIP --to x:1 --tls-key k LICENSE
check 2 "must not be larger than --max-size" $LOGSHIP --to x:1 --chunk 128M LICENSE
check 2 "one of '<files>' or '--stdin'"    $LOGSHIP --to x:1
check 2 "did you mean '--format'"         $LOGSHIP --to x:1 --fromat raw LICENSE

# --- pkg --------------------------------------------------------------------
check 0 "+ left-pad ^1.3.0 (dev)"          $PKG install left-pad --version ^1.3 --dev
check 0 "Would install packages"           $PKG -C ./app --offline install --dry-run
check 0 "Added remote 'origin'"            $PKG remote add origin https://pkgs.example.com
check 0 "Would run: node --version"        $PKG exec node --version
check 0 "Would run: node --help"           $PKG -v exec node --help
check 0 "Would run: node --version"        $PKG exec -- node --version
check 0 "pkg 0.9.0"                        $PKG --version
check 0 "Usage: pkg remote add"            $PKG help remote add
check 0 "Global options:"                  $PKG install --help
check 2 "missing command"                  $PKG
check 2 "did you mean 'install'"           $PKG instal left-pad
check 2 "without leading zeros"            $PKG install x --version 1.02
check 2 "'--dev' and '--optional'"         $PKG install -D -O x
check 2 "needs the network"                $PKG --offline remote add o https://u
check 2 "'remote' needs a command"         $PKG remote
check 2 "unknown option '--trace-resolv'"  $PKG install x --trace-resolv

echo "$checks checks, $failures failed"
[ "$failures" -eq 0 ]
