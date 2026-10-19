#include "shell/executor.h"

#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "builtins/my_tee.h"
#include "shell/process.h"
#include "utils/timer.h"

/* Returns false if any stage is blocked. Updates the dangerous counters. */
static bool screen_dangerous(Shell *sh, const Pipeline *p)
{
    for (int i = 0; i < p->count; i++) {
        DangerLevel level = dangerous_check(sh->dangerous, p->stages[i].cmd);

        if (level == DANGER_BLOCK) {
            sh->stats.dangerous_blocked++;
            return false;
        }
        if (level == DANGER_WARN)
            sh->stats.dangerous_warned++;
    }
    return true;
}

static void close_pipes(int pipes[][2], int count)
{
    for (int i = 0; i < count; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }
}

static bool is_my_tee(const Command *cmd)
{
    return strcmp(cmd->argv[0], "my_tee") == 0;
}

/* Child side of stage `i`: wire up stdin/stdout/stderr and exec. */
static void exec_stage(const Pipeline *p, int i, int pipes[][2],
                       const ResourceLimits *limits)
{
    int pipe_count = p->count - 1;
    const PipelineStage *stage = &p->stages[i];

    if (i > 0 && dup2(pipes[i - 1][0], STDIN_FILENO) == -1) {
        perror("dup2");
        _exit(EXIT_FAILURE);
    }
    if (i < pipe_count && dup2(pipes[i][1], STDOUT_FILENO) == -1) {
        perror("dup2");
        _exit(EXIT_FAILURE);
    }
    close_pipes(pipes, pipe_count);

    if (stage->stderr_path) {
        int fd = open(stage->stderr_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd == -1) {
            perror("open");
            _exit(EXIT_FAILURE);
        }
        dup2(fd, STDERR_FILENO);
        close(fd);
    }

    /* Applied last, so a low "nofile" limit does not affect the setup above. */
    rlimit_apply(limits);

    execvp(stage->cmd->argv[0], stage->cmd->argv);
    perror("execvp");
    _exit(EXIT_EXEC_FAILED);
}

void executor_run(Shell *sh, const Pipeline *p, const ResourceLimits *limits,
                  const char *line)
{
    if (!screen_dangerous(sh, p))
        return;

    int pipe_count = p->count - 1;
    int pipes[MAX_PIPELINE_STAGES][2];
    for (int i = 0; i < pipe_count; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("pipe");
            close_pipes(pipes, i);
            return;
        }
    }

    /* my_tee as the last stage runs in the shell, reading the last pipe. */
    bool tee_last = p->count > 1 && is_my_tee(p->stages[p->count - 1].cmd);
    int child_count = tee_last ? p->count - 1 : p->count;
    pid_t pids[MAX_PIPELINE_STAGES];

    Stopwatch sw;
    stopwatch_start(&sw);
    fflush(stdout);

    for (int i = 0; i < child_count; i++) {
        pids[i] = fork();
        if (pids[i] < 0) {
            perror("fork");
            exit(EXIT_FAILURE);
        }
        if (pids[i] == 0)
            exec_stage(p, i, pipes, limits);
    }

    if (tee_last) {
        int tee_input = pipes[pipe_count - 1][0];
        for (int i = 0; i < pipe_count; i++) {
            close(pipes[i][1]);
            if (pipes[i][0] != tee_input)
                close(pipes[i][0]);
        }
        my_tee(tee_input, p->stages[p->count - 1].cmd);
        close(tee_input);
    } else {
        close_pipes(pipes, pipe_count);
    }

    /* Background children are collected later by process_reap_background(). */
    if (p->background)
        return;

    int last_status = -1;
    for (int i = 0; i < child_count; i++) {
        int status;
        if (waitpid(pids[i], &status, 0) == -1) {
            perror("waitpid");
            continue;
        }
        process_report_status(status, limits);
        last_status = status;
    }

    /* Like bash, a pipeline succeeds when its last command succeeds. */
    if (last_status != -1 && WIFEXITED(last_status) && WEXITSTATUS(last_status) == 0)
        stats_record(&sh->stats, line, stopwatch_elapsed(&sw));
}
