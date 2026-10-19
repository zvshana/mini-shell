/*
 * mini-shell - a small POSIX shell written in C.
 *
 * Usage: mini-shell <dangerous_commands_file> <exec_times_log>
 */

#include <stdio.h>
#include <stdlib.h>

#include "shell/shell.h"

int main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <dangerous_commands_file> <exec_times_log>\n", argv[0]);
        printf("ERR\n");
        return EXIT_FAILURE;
    }

    Shell shell;
    if (!shell_init(&shell, argv[1], argv[2])) {
        printf("ERR\n");
        return EXIT_FAILURE;
    }

    shell_run(&shell);
    shell_destroy(&shell);
    return EXIT_SUCCESS;
}
