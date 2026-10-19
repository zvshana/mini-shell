#ifndef SHELL_STATS_H
#define SHELL_STATS_H

#include <stdbool.h>

/* Execution statistics shown in the prompt and written to the log file. */
typedef struct {
    int    commands;            /* commands that finished successfully */
    int    dangerous_blocked;
    int    dangerous_warned;
    double last_time;
    double avg_time;
    double min_time;
    double max_time;
    const char *log_path;
} ShellStats;

/* Initializes the counters and deletes any previous log file. */
bool stats_init(ShellStats *st, const char *log_path);

/* Records a successful command: appends "<command> : <seconds> sec" to the
 * log and updates the counters. */
void stats_record(ShellStats *st, const char *command, double seconds);

void stats_print_prompt(const ShellStats *st);

#endif /* SHELL_STATS_H */
