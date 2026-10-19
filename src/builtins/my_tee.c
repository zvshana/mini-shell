#include "builtins/my_tee.h"

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "utils/xalloc.h"

#define TEE_BUFFER_SIZE 4096

static bool write_all(int fd, const char *buf, size_t len)
{
    while (len > 0) {
        ssize_t written = write(fd, buf, len);
        if (written < 0) {
            if (errno == EINTR)
                continue;
            return false;
        }
        buf += written;
        len -= (size_t)written;
    }
    return true;
}

void my_tee(int in_fd, const Command *cmd)
{
    int first_file = 1;
    int flags = O_WRONLY | O_CREAT | O_TRUNC;

    if (cmd->argc > 1 && strcmp(cmd->argv[1], "-a") == 0) {
        first_file = 2;
        flags = O_WRONLY | O_CREAT | O_APPEND;
    }

    int file_count = cmd->argc - first_file;
    int *fds = xcalloc((size_t)file_count, sizeof *fds);

    for (int i = 0; i < file_count; i++) {
        fds[i] = open(cmd->argv[first_file + i], flags, 0644);
        if (fds[i] == -1)
            perror(cmd->argv[first_file + i]);
    }

    fflush(stdout);  /* keep ordering with anything already printed */

    char buf[TEE_BUFFER_SIZE];
    ssize_t n;
    while ((n = read(in_fd, buf, sizeof buf)) > 0) {
        if (!write_all(STDOUT_FILENO, buf, (size_t)n))
            perror("write");

        for (int i = 0; i < file_count; i++) {
            if (fds[i] != -1 && !write_all(fds[i], buf, (size_t)n)) {
                perror("write");
                close(fds[i]);
                fds[i] = -1;
            }
        }
    }
    if (n < 0)
        perror("read");

    for (int i = 0; i < file_count; i++) {
        if (fds[i] != -1 && close(fds[i]) == -1)
            perror("close");
    }
    free(fds);
}
