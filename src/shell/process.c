#include "shell/process.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>

void process_report_status(int status, const ResourceLimits *limits)
{
    if (WIFEXITED(status)) {
        if (WEXITSTATUS(status) != 0 && limits && limits->set[RES_NOFILE])
            printf("Too many open files!\n");
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);

        /* Reaching the hard CPU limit sends SIGKILL instead of SIGXCPU. */
        if (sig == SIGXCPU || (sig == SIGKILL && limits && limits->set[RES_CPU]))
            printf("CPU time limit exceeded!\n");
        else if (sig == SIGXFSZ)
            printf("File size limit exceeded!\n");
        else if (sig == SIGSEGV && limits && limits->set[RES_MEM])
            printf("Memory allocation failed!\n");
        else
            printf("Terminated by signal: %s\n", strsignal(sig));
    }
    fflush(stdout);
}

void process_reap_background(void)
{
    int status;
    while (waitpid(-1, &status, WNOHANG) > 0)
        process_report_status(status, NULL);
}
