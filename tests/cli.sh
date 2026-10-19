#!/bin/sh
# end-to-end checks of the fq binary: what main() prints and the exit codes
# scripts rely on. Usage: sh tests/cli.sh build/fq

fq=$1
if [ ! -x "$fq" ]; then
    echo "usage: $0 <path to fq>"
    exit 2
fi
root=$(dirname "$0")/..

HOME=$(mktemp -d "${TMPDIR:-/tmp}/fileq-cli.XXXXXX") || exit 2
export HOME

failed=0

# expect_exit <code> <command...>
expect_exit() {
    want=$1
    shift
    "$@" > /dev/null 2>&1
    got=$?
    if [ "$got" -ne "$want" ]; then
        echo "FAIL: \`$*\` exited $got, expected $want"
        failed=1
    fi
}

# expect_output <text> <command...>
expect_output() {
    want=$1
    shift
    got=$("$@" 2>&1)
    if [ "$got" != "$want" ]; then
        echo "FAIL: \`$*\` printed '$got', expected '$want'"
        failed=1
    fi
}

# the version shown by help must match the README badge
version=$(sed -n 's/.*badge\/version-\([0-9.]*\)-.*/\1/p' "$root/README.md")
help_line=$("$fq" help | head -n 1)
[ "$help_line" = "fileq v$version" ] || { echo "FAIL: help says '$help_line', README badge says $version"; failed=1; }
expect_exit 0 "$fq" help
no_args_line=$("$fq" | head -n 1)
[ "$no_args_line" = "$help_line" ] || { echo "FAIL: no arguments printed '$no_args_line', not help"; failed=1; }

expect_exit 1 "$fq" pul -q t
expect_exit 1 "$fq" push -q ../x m
[ ! -e "$HOME/.fileq/x" ] || { echo "FAIL: push -q ../x created a file outside qs/"; failed=1; }

expect_exit 0 "$fq" push -q t hi there
expect_output "hi there" "$fq" pull -q t
expect_exit 1 "$fq" pull -q t

expect_exit 1 "$fq" clear -q missing
expect_exit 0 "$fq" del -q t
expect_exit 1 "$fq" del -q t

# exit code 1, not a crash (>= 128)
expect_exit 1 env -u HOME "$fq" list

if [ "$failed" -eq 0 ]; then
    echo "cli.sh: all checks passed"
fi
exit "$failed"
