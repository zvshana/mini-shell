#include "shell/stats.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

bool stats_init(ShellStats *st, const char *log_path)
{
    memset(st, 0, sizeof *st);
    st->log_path = log_path;

    /* Start every session with a fresh log. */
    if (unlink(log_path) == -1 && errno != ENOENT) {
        perror(log_path);
        return false;
    }
    return true;
}

void stats_record(ShellStats *st, const char *command, double seconds)
{
    FILE *log = fopen(st->log_path, "a");
    if (log) {
        fprintf(log, "%s : %f sec\n", command, seconds);
        fclose(log);
    } else {
        perror("fopen");
    }

    st->commands++;
    st->last_time = seconds;
    if (st->commands == 1 || seconds < st->min_time)
        st->min_time = seconds;
    if (seconds > st->max_time)
        st->max_time = seconds;
    st->avg_time += (seconds - st->avg_time) / st->commands;
}

void stats_print_prompt(const ShellStats *st)
{
    printf("#cmd:%d|#dangerous_cmd_blocked:%d|last_cmd_time:%f|avg_time:%f|"
           "min_time:%f|max_time:%f>>",
           st->commands, st->dangerous_blocked, st->last_time,
           st->avg_time, st->min_time, st->max_time);
    fflush(stdout);
}
