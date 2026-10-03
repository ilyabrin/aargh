#!/bin/sh
# Loads the completion scripts of examples/pkg into bash, zsh and fish and
# checks what Tab offers in each. Shells you don't have are skipped, unless
# ARGH_COMPLETION_STRICT=1 (CI), where a missing shell is a failure.
# Usage: sh tests/completion/check.sh
set -e
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
failed=0

${CC:-cc} -std=c99 -o "$OUT/pkg" "$ROOT/examples/pkg/main.c" "$ROOT/examples/pkg/install.c" \
    "$ROOT/examples/pkg/remote.c" "$ROOT/examples/pkg/exec.c"
PATH="$OUT:$PATH"
export PATH

# Each case: the words on the line (the last one is being completed), then
# what Tab should offer, sorted
CASES='pkg ""|exec install remote remove
pkg re|remote remove
pkg remote ""|add list remove
pkg -C dir remote a|add
pkg --color ""|always auto never
pkg --completions ""|bash fish zsh
pkg install --d|--dev --dry-run
pkg install --v|--verbose --version
pkg --v|--verbose --version
pkg remote list ""|'

check() {
    shell=$1
    shift
    if ! command -v "$shell" >/dev/null 2>&1; then
        if [ "${ARGH_COMPLETION_STRICT:-0}" = 1 ]; then
            echo "FAIL  $shell not found"
            failed=1
        else
            echo "skip  $shell (not found)"
        fi
        return
    fi
    echo "$CASES" | while IFS='|' read -r words want; do
        got=$("$@" "$words" | tr -s ' ' | sed 's/ $//')
        if [ "$got" = "$want" ]; then
            echo "ok    $shell: $words"
        else
            echo "FAIL  $shell: $words: got '$got', want '$want'"
            echo 1 >"$OUT/failed"
        fi
    done
}

# bash: call the completion function the way bash does
cat >"$OUT/bash.sh" <<'EOF'
source <(pkg --completions bash)
eval "COMP_WORDS=($1)"
COMP_CWORD=$((${#COMP_WORDS[@]} - 1))
_pkg
printf '%s\n' "${COMPREPLY[@]}" | LC_ALL=C sort | tr '\n' ' '
EOF
check bash bash "$OUT/bash.sh"

# zsh: through bashcompinit, which runs the function under `emulate sh`
cat >"$OUT/zsh.sh" <<'EOF'
autoload -U +X bashcompinit && bashcompinit
source <(pkg --completions zsh)
complete_words() {
    emulate -L sh
    eval "COMP_WORDS=($1)"
    COMP_CWORD=$((${#COMP_WORDS[@]} - 1))
    _pkg
    printf '%s\n' "${COMPREPLY[@]}" | LC_ALL=C sort | tr '\n' ' '
}
complete_words "$1"
EOF
check zsh zsh "$OUT/zsh.sh"

# fish: `complete -C` asks fish itself what it would offer
cat >"$OUT/fish.fish" <<'EOF'
pkg --completions fish | source
set -l line (string replace -a '""' '' -- $argv[1])
complete -C "$line" | string replace -r '\t.*' '' | sort | string join ' '
EOF
check fish fish "$OUT/fish.fish"

[ -f "$OUT/failed" ] && failed=1
exit $failed
