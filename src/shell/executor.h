#ifndef SHELL_EXECUTOR_H
#define SHELL_EXECUTOR_H

#include "builtins/rlimit.h"
#include "shell/pipeline.h"
#include "shell/shell.h"

/*
 * Runs a pipeline of external commands:
 *   - checks every stage against the dangerous-command list,
 *   - forks one child per stage, connected with pipes,
 *   - applies "2>" redirections and resource limits in the children,
 *   - runs `my_tee` inside the shell when it is the last stage,
 *   - waits (unless in background), reports failures and records timing.
 *
 * `line` is the original command line, used for the execution log.
 */
void executor_run(Shell *sh, const Pipeline *p, const ResourceLimits *limits,
                  const char *line);

#endif /* SHELL_EXECUTOR_H */
