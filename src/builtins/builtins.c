#include "builtins/builtins.h"

#include <stdio.h>
#include <string.h>

#include "builtins/mcalc.h"
#include "builtins/vmem.h"
#include "utils/str_utils.h"
#include "utils/timer.h"

/* `line` is the whole command line, `args` points just past the built-in
 * name (either "" or " arg1 arg2 ..."). */
typedef BuiltinResult (*BuiltinHandler)(Shell *sh, char *line, char *args,
                                        ResourceLimits *limits);

/* vmem <script_file.txt> */
static BuiltinResult run_vmem(Shell *sh, char *line, char *args, ResourceLimits *limits)
{
    (void)sh;
    (void)line;
    (void)limits;

    const char *path = args + 1;
    if (args[0] != ' ' || *path == '\0' || strchr(path, ' ') || !str_ends_with(path, ".txt")) {
        printf("Usage: vmem <script_file>\n");
        return BUILTIN_HANDLED;
    }

    vmem_run(path);
    return BUILTIN_HANDLED;
}

/* mcalc "matrix1" "matrix2" ... "ADD"|"SUB" */
static BuiltinResult run_mcalc(Shell *sh, char *line, char *args, ResourceLimits *limits)
{
    (void)limits;

    MatrixSet set;
    if (!mcalc_parse(args, &set)) {
        printf("ERR_MAT_INPUT\n");
        return BUILTIN_HANDLED;
    }

    Stopwatch sw;
    stopwatch_start(&sw);

    const int *result = mcalc_reduce(&set);
    if (result) {
        mcalc_print(&set, result);
        stats_record(&sh->stats, line, stopwatch_elapsed(&sw));
    } else {
        printf("ERR\n");
    }

    mcalc_free(&set);
    return BUILTIN_HANDLED;
}

/* rlimit show | rlimit set <limits> <command> */
static BuiltinResult run_rlimit(Shell *sh, char *line, char *args, ResourceLimits *limits)
{
    if (args[0] == ' ') {
        char *spec = args + 1;

        if (strcmp(spec, "show") == 0) {
            Stopwatch sw;
            stopwatch_start(&sw);
            rlimit_show();
            stats_record(&sh->stats, "rlimit show", stopwatch_elapsed(&sw));
            return BUILTIN_HANDLED;
        }

        if (rlimit_parse_set(spec, limits)) {
            memmove(line, spec, strlen(spec) + 1);
            return BUILTIN_PASSTHROUGH;
        }
    }

    printf("ERR\n");
    return BUILTIN_HANDLED;
}

static const struct {
    const char    *name;
    BuiltinHandler handler;
} BUILTINS[] = {
    { "vmem",   run_vmem   },
    { "mcalc",  run_mcalc  },
    { "rlimit", run_rlimit },
};

BuiltinResult builtins_dispatch(Shell *sh, char *line, ResourceLimits *limits)
{
    for (size_t i = 0; i < sizeof BUILTINS / sizeof BUILTINS[0]; i++) {
        if (str_starts_with_word(line, BUILTINS[i].name)) {
            char *args = line + strlen(BUILTINS[i].name);
            return BUILTINS[i].handler(sh, line, args, limits);
        }
    }
    return BUILTIN_NOT_FOUND;
}
