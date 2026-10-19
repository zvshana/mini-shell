# mini-shell

A small POSIX shell for Linux, written in C. It runs external programs with pipes and stderr redirection, and it adds a few systems features of its own: a dangerous-command filter, per-command resource limits, a matrix calculator that uses threads, and a virtual-memory (paging) simulator.

I built it in four stages for an Operating Systems course, then restructured it into a modular codebase with a test suite and CI.

```text
#cmd:2|#dangerous_cmd_blocked:0|last_cmd_time:0.001931|avg_time:0.001520|min_time:0.001109|max_time:0.001931>>ls -l | my_tee listing.txt
```

## Features

| Area | What it does |
| --- | --- |
| **Process execution** | Runs commands with `fork` + `execvp` and reports how each child exited (exit code or signal). |
| **Pipelines** | `cmd1 \| cmd2 \| ...`: one child per stage, connected with `pipe` + `dup2`. |
| **Redirection** | `cmd 2> file` sends stderr to a file. It works inside pipelines too. |
| **Background jobs** | `cmd &` returns to the prompt at once. Finished jobs are collected before each prompt. |
| **Timing & stats** | The prompt shows the command count, blocked commands, and last/avg/min/max run time. Each successful command is appended to a log file. |
| **Dangerous-command filter** | An exact match from the list file is blocked. A command with the same name but different arguments gets a warning. |
| **`my_tee`** | A built-in `tee` (`-a` for append) that uses only `read`/`write`. |
| **`rlimit`** | `rlimit show` prints the current limits. `rlimit set cpu=2:3 mem=50M cmd...` runs one command under `setrlimit` limits (`cpu`, `mem`, `fsize`, `nofile`). |
| **`mcalc`** | Adds or subtracts matrices as a parallel reduction tree, with one `pthread` per pair at each level. |
| **`vmem`** | A demand-paging simulator: page table, LRU page replacement, first-fit swap file, and a 4-entry TLB with LRU replacement. |

## Build & run

Requirements: Linux (or WSL), `gcc`, and `make`. `valgrind` is optional and only used for the memory check.

```bash
make
```

```bash
./mini-shell examples/dangerous_commands.txt exec_times.log
```

The first argument is the dangerous-commands file. The second is the execution-time log, which is recreated on every run. Type `done` to exit. On exit the shell prints the number of dangerous commands it saw (blocked plus warned).

## Usage examples

```text
>> ls -l | grep .c
>> ls /missing 2> errors.txt
>> sleep 5 &
>> echo "Hello" | my_tee out1.txt out2.txt
>> echo "more" | my_tee -a out1.txt

>> rlimit show
CPU time: soft=unlimited, hard=unlimited
Memory: soft=unlimited, hard=unlimited
File size: soft=unlimited, hard=unlimited
Open files: soft=1024, hard=1048576
>> rlimit set cpu=1:2 sha256sum /dev/zero
CPU time limit exceeded!
>> rlimit set fsize=1K dd if=/dev/zero of=big.bin bs=4K count=1
File size limit exceeded!

>> mcalc "(2,2:9,8,7,6)" "(2,2:1,2,3,4)" "SUB"
(2,2:8,6,4,2)

>> rm -rf /
ERR: Dangerous command detected ("rm -rf /"). Execution prevented.
```

### Virtual memory simulation

```bash
./mini-shell examples/dangerous_commands.txt exec_times.log
```

```text
>> vmem examples/vmem/vmem_script.txt
Loaded program "my_program.bin" with text=64, data=32, bss=16, heap_stack=48.
TLB Miss: Page 0
Page fault: Loading page 0 from program file (text)
TLB Updated: Page 0 -> Frame 0
Value at address 0 = A
...
Page replacement: Evicting page 12 to swap
...
Page fault: Loading page 12 from swap file
Value at address 100 = A
```

A script starts with a configuration line, followed by one command per line:

```text
my_program.bin my_swap.bin 64 32 16 48 8 20 40 80
load 0
store 100 A
print table          # also: ram, swap, tlb
```

The configuration fields, in order: program file, swap file, text / data / bss / heap+stack sizes, page size, number of pages, RAM size, and swap size, all in bytes. Relative file names are resolved against the script's directory.

## Project structure

```text
src/
├── main.c                 entry point: argument checks, start the shell
├── config.h               limits (line length, max args, pipeline stages)
├── shell/                 the shell itself
│   ├── shell.c/.h         read-eval loop and session state
│   ├── parser.c/.h        syntax checks (ERR_ARGS, ERR_SPACE, |, &, 2>)
│   ├── pipeline.c/.h      turns a command line into pipeline stages
│   ├── command.c/.h       Command type (argv + text)
│   ├── executor.c/.h      fork/exec, pipes, redirection, waiting
│   ├── process.c/.h       exit-status reporting, background reaping
│   ├── dangerous.c/.h     dangerous-command filter
│   └── stats.c/.h         prompt statistics and the execution log
├── builtins/              commands that run inside the shell process
│   ├── builtins.c/.h      dispatch table (add a built-in = add one row)
│   ├── rlimit.c/.h        rlimit show / set
│   ├── my_tee.c/.h        tee built on read/write
│   ├── mcalc.c/.h         parallel matrix reduction (pthreads)
│   └── vmem.c/.h          vmem script validation and execution
├── memsim/                virtual memory simulator (independent of the shell)
│   ├── mem_sim.c/.h       init, load/store, page faults, LRU, swap
│   ├── tlb.c/.h           TLB with LRU replacement
│   └── mem_print.c        RAM / swap / page table / TLB dumps
└── utils/                 small helpers shared by all modules
    ├── str_utils.c/.h
    ├── timer.c/.h
    └── xalloc.c/.h
tests/
├── run_tests.sh           end-to-end tests (58 cases)
└── run_valgrind.sh        full session under valgrind
examples/                  dangerous-commands list and a vmem demo
```

**How a line flows through the shell:**

```text
read line ─► built-in? ──yes─► run in the shell process (vmem, mcalc, rlimit show)
               │  └─ rlimit set: save limits, strip prefix ─┐
               no                                           │
               ▼                                            ▼
          parse & validate ─► build pipeline ─► dangerous check ─► fork/exec stages ─► wait, report, log
```

## Design notes

- **Pipelines.** A pipeline has N stages, each with its own optional `2>` target. One code path handles simple commands, pipes, redirection and their combinations. When `my_tee` is the last stage, it runs inside the shell and reads the last pipe.
- **Resource limits** are applied in the child right before `execvp`, so they never affect the shell. They are applied after the redirections are set up, so a small `nofile` limit doesn't break that setup.
- **Background jobs** are collected with `waitpid(-1, …, WNOHANG)` before each prompt. Foreground commands wait for their own PIDs, so a background job can never be mistaken for the foreground one.
- **mcalc** keeps the input order. For subtraction the tree is `((M1−M2)−(M3−M4))−M5`, never a left fold. Each level starts its threads, joins them, then frees the inputs from that level.
- **vmem**: text pages are read-only and never written to swap. Data pages come from the program file until they are modified. bss and heap/stack pages start zero-filled. Dirty pages are written to the first free swap slot when evicted. Each eviction also removes the page from the TLB.

## Testing

```bash
make test
```

```bash
make valgrind
```

`make test` runs 58 end-to-end cases covering all the features above. `make valgrind` runs one session that uses every feature and fails on any leak or memory error. GitHub Actions runs both on every push.

## Author

Sara Shana Zivi
