#ifndef SHELL_PARSER_H
#define SHELL_PARSER_H

#include <stdbool.h>

/*
 * Syntax information collected from a command line.
 *
 * For the operator fields: 0 means "not present", -1 means "present but
 * malformed", and a positive value is the operator's index in the line.
 */
typedef struct {
    int  word_count;
    int  pipe_index;        /* '|'  - must have exactly one space on each side */
    int  background_index;  /* '&'  - must be the last character */
    int  redirect_index;    /* "2>" - must have exactly one space on each side */
    bool too_many_args;     /* a command has more than MAX_ARGS arguments */
    bool extra_spaces;      /* more than one space between words */
} InputInfo;

/* Scans `line` (already trimmed) and fills `info`. */
void parse_input(const char *line, InputInfo *info);

/* Prints ERR_ARGS / ERR_SPACE / ERR for every problem found.
 * Returns true if the line can be executed. */
bool validate_input(const InputInfo *info);

#endif /* SHELL_PARSER_H */
