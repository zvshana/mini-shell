#include "shell/shell.h"

#include <stdio.h>
#include <string.h>
#include <strings.h>

#include "builtins/builtins.h"
#include "config.h"
#include "shell/executor.h"
#include "shell/parser.h"
#include "shell/pipeline.h"
#include "shell/process.h"
#include "utils/str_utils.h"

typedef enum { READ_OK, READ_EOF, READ_TOO_LONG } ReadStatus;

bool shell_init(Shell *sh, const char *dangerous_path, const char *log_path)
{
    memset(sh, 0, sizeof *sh);

    sh->dangerous = dangerous_load(dangerous_path);
    if (!sh->dangerous) {
        perror(dangerous_path);
        return false;
    }
    if (!stats_init(&sh->stats, log_path)) {
        dangerous_free(sh->dangerous);
        sh->dangerous = NULL;
        return false;
    }
    return true;
}

void shell_destroy(Shell *sh)
{
    dangerous_free(sh->dangerous);
    sh->dangerous = NULL;
}

/* Reads one line into `buf` without the trailing newline. */
static ReadStatus read_line(char *buf, size_t size)
{
    if (!fgets(buf, (int)size, stdin))
        return READ_EOF;

    size_t len = strcspn(buf, "\n");
    if (buf[len] == '\n' || feof(stdin)) {
        buf[len] = '\0';
        return READ_OK;
    }

    /* Line longer than the buffer: discard the rest of it. */
    int c;
    while ((c = getchar()) != '\n' && c != EOF)
        ;
    return READ_TOO_LONG;
}

static bool is_done_command(const char *line)
{
    return strncasecmp(line, "done", 4) == 0 && (line[4] == ' ' || line[4] == '\0');
}

/* Executes one trimmed, non-empty line. Returns false when the shell
 * should exit. */
static bool execute_line(Shell *sh, char *line)
{
    ResourceLimits limits;
    rlimit_reset(&limits);

    /* Built-ins run inside the shell. "rlimit set" only strips its prefix
     * and lets the remaining command continue as an external command. */
    if (builtins_dispatch(sh, line, &limits) == BUILTIN_HANDLED)
        return true;

    InputInfo info;
    parse_input(line, &info);
    if (!validate_input(&info))
        return true;

    if (is_done_command(line)) {
        if (info.word_count == 1)
            return false;
        printf("ERR\n");
        return true;
    }

    Pipeline pipeline;
    if (!pipeline_build(line, &info, &pipeline)) {
        printf("ERR\n");
        return true;
    }
    executor_run(sh, &pipeline, &limits, line);
    pipeline_free(&pipeline);
    return true;
}

void shell_run(Shell *sh)
{
    char line[MAX_INPUT_LEN + 2];   /* room for '\n' and '\0' */

    for (;;) {
        process_reap_background();
        stats_print_prompt(&sh->stats);

        ReadStatus status = read_line(line, sizeof line);
        if (status == READ_EOF) {
            printf("\n");
            break;
        }
        if (status == READ_TOO_LONG) {
            printf("ERR\n");
            continue;
        }
        if (str_is_blank(line))
            continue;

        str_trim(line);
        if (!execute_line(sh, line))
            break;
    }

    /* On exit: total number of dangerous commands (blocked + warned). */
    printf("%d\n", sh->stats.dangerous_blocked + sh->stats.dangerous_warned);
}
