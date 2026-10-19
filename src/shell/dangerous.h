#ifndef SHELL_DANGEROUS_H
#define SHELL_DANGEROUS_H

#include "shell/command.h"

/*
 * Dangerous-command filter.
 *
 * The list is loaded from a text file with one command per line, e.g.:
 *     rm -rf /
 *     shutdown -h now
 *
 * A user command that matches an entry exactly is blocked. A command with
 * the same name but different arguments only produces a warning.
 */
typedef struct {
    Command *head;
    int      count;
} DangerousList;

typedef enum {
    DANGER_NONE,   /* command is not related to any dangerous command */
    DANGER_WARN,   /* same program as a dangerous command, other arguments */
    DANGER_BLOCK   /* exact match: execution must be prevented */
} DangerLevel;

/* Loads the list from `path`. Returns NULL if the file cannot be opened. */
DangerousList *dangerous_load(const char *path);

/* Checks `cmd` against the list and prints the matching warning/error. */
DangerLevel dangerous_check(const DangerousList *list, const Command *cmd);

void dangerous_free(DangerousList *list);

#endif /* SHELL_DANGEROUS_H */
