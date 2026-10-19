#ifndef BUILTINS_BUILTINS_H
#define BUILTINS_BUILTINS_H

#include "builtins/rlimit.h"
#include "shell/shell.h"

typedef enum {
    BUILTIN_NOT_FOUND,    /* not a built-in: run as an external command */
    BUILTIN_HANDLED,      /* fully handled inside the shell */
    BUILTIN_PASSTHROUGH   /* the line was rewritten: run the rest externally */
} BuiltinResult;

/*
 * Runs `line` if it starts with a built-in name (vmem, mcalc, rlimit).
 * `rlimit set` fills `limits` and rewrites `line` to the command to run.
 *
 * To add a built-in, write a handler and add it to the table in builtins.c.
 */
BuiltinResult builtins_dispatch(Shell *sh, char *line, ResourceLimits *limits);

#endif /* BUILTINS_BUILTINS_H */
