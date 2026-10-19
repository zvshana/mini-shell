#!/usr/bin/env bash
#
# Runs a session that touches every feature under valgrind and fails on
# memory leaks or memory errors in the shell process.
#
# Usage: tests/run_valgrind.sh [path/to/mini-shell]

set -u

SHELL_BIN="$(realpath "${1:-./mini-shell}")"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

cp "$ROOT"/examples/vmem/* "$ROOT"/examples/dangerous_commands.txt "$WORK"/
cd "$WORK" || exit 1

printf '%s\n' \
    "echo hello" \
    "echo  bad" \
    "rm -rf /" \
    "rm -rf ./nothing" \
    "echo a b c | wc -w" \
    "ls /no/such/path 2> err.txt" \
    "echo tee | my_tee out.txt" \
    "sleep 0 &" \
    "rlimit show" \
    "rlimit set cpu=5 echo limited" \
    'mcalc "(1,1:16)" "(1,1:8)" "(1,1:4)" "(1,1:2)" "(1,1:1)" "SUB"' \
    'mcalc "(2,2:1)" "ADD"' \
    "vmem vmem_script.txt" \
    "done" |
valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all \
         --error-exitcode=1 --quiet \
         "$SHELL_BIN" dangerous_commands.txt exec_times.log > /dev/null

status=$?
if [ "$status" -eq 0 ]; then
    echo "valgrind: no leaks or memory errors"
fi
exit "$status"
