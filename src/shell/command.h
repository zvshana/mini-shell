#ifndef SHELL_COMMAND_H
#define SHELL_COMMAND_H

#include <stdbool.h>

/* A single command: program name plus arguments. */
typedef struct Command {
    char **argv;          /* NULL-terminated, argv[0] is the program name */
    int    argc;
    char  *text;          /* the command as text, words joined by one space */
    struct Command *next; /* link for command lists */
} Command;

/* Builds a command from a line of text. Returns NULL for an empty line. */
Command *command_parse(const char *text);

/* Builds a command from the first `count` words. Returns NULL if count <= 0. */
Command *command_from_words(char *const *words, int count);

/* True if both commands have the same name and the same arguments. */
bool command_equals(const Command *a, const Command *b);

void command_free(Command *cmd);

#endif /* SHELL_COMMAND_H */
