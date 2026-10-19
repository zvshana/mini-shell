#include "shell/command.h"

#include <stdlib.h>
#include <string.h>

#include "utils/str_utils.h"
#include "utils/xalloc.h"

static char *join_words(char *const *words, int count)
{
    size_t len = 1;
    for (int i = 0; i < count; i++)
        len += strlen(words[i]) + 1;

    char *text = xmalloc(len);
    text[0] = '\0';
    for (int i = 0; i < count; i++) {
        if (i > 0)
            strcat(text, " ");
        strcat(text, words[i]);
    }
    return text;
}

Command *command_from_words(char *const *words, int count)
{
    if (count <= 0)
        return NULL;

    Command *cmd = xcalloc(1, sizeof *cmd);
    cmd->argc = count;
    cmd->argv = xcalloc((size_t)count + 1, sizeof *cmd->argv);
    for (int i = 0; i < count; i++)
        cmd->argv[i] = xstrdup(words[i]);
    cmd->text = join_words(words, count);
    return cmd;
}

Command *command_parse(const char *text)
{
    int count;
    char **words = str_split_words(text, &count);
    Command *cmd = command_from_words(words, count);
    str_free_words(words);
    return cmd;
}

bool command_equals(const Command *a, const Command *b)
{
    if (a->argc != b->argc)
        return false;
    for (int i = 0; i < a->argc; i++) {
        if (strcmp(a->argv[i], b->argv[i]) != 0)
            return false;
    }
    return true;
}

void command_free(Command *cmd)
{
    if (!cmd)
        return;
    str_free_words(cmd->argv);
    free(cmd->text);
    free(cmd);
}
