#ifndef BUILTINS_MY_TEE_H
#define BUILTINS_MY_TEE_H

#include "shell/command.h"

/*
 * The `my_tee` built-in: my_tee [-a] file...
 *
 * Copies everything read from `in_fd` (the read end of a pipe) to stdout
 * and to each file. Files are truncated, or appended to with -a.
 * Runs inside the shell process and uses only read()/write().
 */
void my_tee(int in_fd, const Command *cmd);

#endif /* BUILTINS_MY_TEE_H */
