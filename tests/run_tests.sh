#!/usr/bin/env bash
#
# End-to-end tests: each case feeds commands to the shell on stdin and
# checks that the output contains an expected string.
#
# Usage: tests/run_tests.sh [path/to/mini-shell]

set -u

SHELL_BIN="$(realpath "${1:-./mini-shell}")"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

cp "$ROOT"/examples/vmem/* "$WORK"/
cd "$WORK" || exit 1

cat > dangerous.txt <<'EOF'
rm -rf /
echo forbidden words
EOF

passed=0
failed=0

# run <command>... : runs the shell with the given input lines, then "done".
# NUL bytes (zero-filled memory pages) are dropped so bash can hold the output.
run() {
    printf '%s\n' "$@" done |
        timeout 20 "$SHELL_BIN" dangerous.txt exec_times.log 2>&1 |
        tr -d '\0'
}

# check <name> <expected-substring> <command>...
check() {
    local name="$1" expected="$2"
    shift 2
    local output
    output="$(run "$@")"
    if [[ "$output" == *"$expected"* ]]; then
        passed=$((passed + 1))
        printf '  \033[32mPASS\033[0m %s\n' "$name"
    else
        failed=$((failed + 1))
        printf '  \033[31mFAIL\033[0m %s\n' "$name"
        printf '       expected to find: %s\n' "$expected"
        printf '       output was:\n%s\n' "$(sed 's/^/         | /' <<<"$output")"
    fi
}

# check_not <name> <unexpected-substring> <command>...
check_not() {
    local name="$1" unexpected="$2"
    shift 2
    local output
    output="$(run "$@")"
    if [[ "$output" == *"$unexpected"* ]]; then
        failed=$((failed + 1))
        printf '  \033[31mFAIL\033[0m %s (found: %s)\n' "$name" "$unexpected"
    else
        passed=$((passed + 1))
        printf '  \033[32mPASS\033[0m %s\n' "$name"
    fi
}

echo "Basic execution"
check "runs a command"              "hello"                   "echo hello"
check "counts successful commands"  "#cmd:1|"                 "echo hello"
check "exit prints dangerous count" ">>0"                     "pwd"
check "unknown command fails"       "execvp"                  "no_such_command_xyz"
check_not "failed command not counted" "#cmd:1|"              "no_such_command_xyz"
check "writes the execution log"    "echo logged : "          "echo logged" "cat exec_times.log"

echo "Input validation"
check "too many arguments"          "ERR_ARGS"                "echo 1 2 3 4 5 6 7"
check "six arguments are allowed"   "1 2 3 4 5 6"             "echo 1 2 3 4 5 6"
check "double space"                "ERR_SPACE"               "echo  hi"
check "pipe without spaces"         "ERR"                     "echo hi|cat"
check "done with arguments"         "ERR"                     "done now"
check "line longer than 1024 chars" "ERR"                     "echo $(printf 'a%.0s' {1..1100})"

echo "Dangerous commands"
check "exact match is blocked"      'ERR: Dangerous command detected ("echo forbidden words")' \
                                    "echo forbidden words"
check_not "blocked command not run" ">>forbidden words"       "echo forbidden words"
check "blocked counter in prompt"   "#dangerous_cmd_blocked:1|" "echo forbidden words"
check "same name warns"             "WARNING: Command similar to dangerous command" \
                                    "echo forbidden"
check "warned command still runs"   $'\nforbidden'            "echo forbidden"
check "exit counts blocked + warned" ">>2"                    "echo forbidden words" "echo forbidden"

echo "Pipes, redirection, background"
check "two-stage pipe"              "3"                       "echo a b c | wc -w"
check "three-stage pipe"            "abc"                     "echo abc | rev | rev"
check "stderr redirection"          "No such file"            "ls /no/such/path 2> err.txt" "cat err.txt"
check "redirection inside a pipe"   "No such file"            "ls /no/such/path 2> err2.txt | wc -l" "cat err2.txt"
check "background job"              "after"                   "sleep 1 &" "echo after"

echo "my_tee"
check "prints to stdout"            "tee-test"                "echo tee-test | my_tee t1.txt t2.txt"
check "writes every file"           "tee-test"                "echo tee-test | my_tee t1.txt t2.txt" "cat t2.txt"
check "append mode"                 $'first\nsecond'          "echo first | my_tee t3.txt" \
                                    "echo second | my_tee -a t3.txt" "cat t3.txt"

echo "rlimit"
check "show"                        "CPU time: soft="         "rlimit show"
check "cpu soft limit (SIGXCPU)"    "CPU time limit exceeded!" "rlimit set cpu=1:2 sha256sum /dev/zero"
check "cpu hard limit (SIGKILL)"    "CPU time limit exceeded!" "rlimit set cpu=1 sha256sum /dev/zero"
check "limits only affect the child" "CPU time: soft=unlimited" "rlimit set cpu=1:2 echo x" "rlimit show"
check "file size limit"             "File size limit exceeded!" \
                                    "rlimit set fsize=1K dd if=/dev/zero of=big.bin bs=4K count=1"
check "open files limit"            "Too many open files!"    "rlimit set nofile=3 ls /"
check "unknown resource"            "ERR"                     "rlimit set foo=1 ls"
check "limit without command"       "ERR"                     "rlimit set cpu=1"

echo "mcalc"
check "add"                         "(2,2:6,6,6,6)"           'mcalc "(2,2:1,1,1,1)" "(2,2:2,2,2,2)" "(2,2:3,3,3,3)" "ADD"'
check "sub"                         "(2,2:8,6,4,2)"           'mcalc "(2,2:9,8,7,6)" "(2,2:1,2,3,4)" "SUB"'
check "sub keeps tree order"        "(1,1:5)"                 'mcalc "(1,1:16)" "(1,1:8)" "(1,1:4)" "(1,1:2)" "(1,1:1)" "SUB"'
check "non-square matrices"         "(2,3:2,4,6,8,10,12)"     'mcalc "(2,3:1,2,3,4,5,6)" "(2,3:1,2,3,4,5,6)" "ADD"'
check "negative numbers"            "(1,2:-3,0)"              'mcalc "(1,2:-1,2)" "(1,2:2,2)" "SUB"'
check "dimension mismatch"          "ERR_MAT_INPUT"           'mcalc "(2,2:1,1,1,1)" "(1,1:1)" "ADD"'
check "wrong value count"           "ERR_MAT_INPUT"           'mcalc "(2,2:1,1,1)" "(2,2:1,1,1,1)" "ADD"'
check "unknown operation"           "ERR_MAT_INPUT"           'mcalc "(1,1:1)" "(1,1:1)" "MUL"'
check "single matrix"               "ERR_MAT_INPUT"           'mcalc "(1,1:1)" "ADD"'
check "no arguments"                "ERR_MAT_INPUT"           'mcalc'

echo "vmem"
check "usage message"               "Usage: vmem <script_file>" "vmem"
check "missing script"              "Error: Cannot open script file nope.txt" "vmem nope.txt"
check "loads the program"           'Loaded program "my_program.bin" with text=64, data=32, bss=16, heap_stack=48.' \
                                    "vmem vmem_script.txt"
check "reads text segment"          "Value at address 0 = A"  "vmem vmem_script.txt"
check "reads data segment"          "Value at address 64 = d" "vmem vmem_script.txt"
check "page fault from program"     "Page fault: Loading page 8 from program file (data)" "vmem vmem_script.txt"
check "dirty page goes to swap"     "Page replacement: Evicting page 12 to swap" "vmem vmem_script.txt"
check "page comes back from swap"   "Page fault: Loading page 12 from swap file" "vmem vmem_script.txt"
check "value survives swapping"     "Value at address 100 = A" "vmem vmem_script.txt"
check "TLB hit"                     "TLB Hit: Page 12 -> Frame" "vmem vmem_script.txt"
check "write to text is rejected"   "Error: Invalid write operation to read-only segment at address 10" \
                                    "vmem vmem_script.txt"
check "address out of range"        "Error: Invalid address 500 (out of range)" "vmem vmem_script.txt"
mkdir -p scripts && cp "$ROOT"/examples/vmem/* scripts/
check "script from another directory" "Value at address 100 = A" "vmem scripts/vmem_script.txt"

printf 'bad.bin swap.bin 1 2 3\nload 0\n' > bad_script.txt
check "invalid script format"       "Error: Invalid script format" "vmem bad_script.txt"

echo
echo "Passed: $passed  Failed: $failed"
[ "$failed" -eq 0 ]
