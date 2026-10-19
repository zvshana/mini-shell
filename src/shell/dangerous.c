#include "shell/dangerous.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "utils/str_utils.h"
#include "utils/xalloc.h"

DangerousList *dangerous_load(const char *path)
{
    FILE *fp = fopen(path, "r");
    if (!fp)
        return NULL;

    DangerousList *list = xcalloc(1, sizeof *list);
    Command **tail = &list->head;
    char line[MAX_INPUT_LEN + 2];

    while (fgets(line, sizeof line, fp)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (str_is_blank(line))
            continue;

        Command *cmd = command_parse(line);
        *tail = cmd;
        tail = &cmd->next;
        list->count++;
    }

    fclose(fp);
    return list;
}

DangerLevel dangerous_check(const DangerousList *list, const Command *cmd)
{
    const Command *similar = NULL;

    for (const Command *entry = list->head; entry; entry = entry->next) {
        if (strcmp(entry->argv[0], cmd->argv[0]) != 0)
            continue;

        if (command_equals(entry, cmd)) {
            printf("ERR: Dangerous command detected (\"%s\"). Execution prevented.\n",
                   entry->text);
            return DANGER_BLOCK;
        }
        if (!similar)
            similar = entry;
    }

    if (similar) {
        printf("WARNING: Command similar to dangerous command (\"%s\"). Proceed with caution.\n",
               similar->text);
        return DANGER_WARN;
    }
    return DANGER_NONE;
}

void dangerous_free(DangerousList *list)
{
    if (!list)
        return;

    Command *cmd = list->head;
    while (cmd) {
        Command *next = cmd->next;
        command_free(cmd);
        cmd = next;
    }
    free(list);
}
