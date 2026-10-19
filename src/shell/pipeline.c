#include "shell/pipeline.h"

#include <stdlib.h>
#include <string.h>

#include "utils/str_utils.h"
#include "utils/xalloc.h"

/* Adds the command made of `words[0..count)` as the next stage. */
static bool add_stage(Pipeline *p, char *const *words, int count)
{
    if (count == 0 || p->count == MAX_PIPELINE_STAGES)
        return false;

    int argc = count;
    const char *stderr_path = NULL;

    for (int i = 0; i < count; i++) {
        if (strcmp(words[i], "2>") == 0) {
            argc = i;
            stderr_path = (i + 1 < count) ? words[i + 1] : NULL;
            break;
        }
    }
    if (argc == 0 || (argc != count && stderr_path == NULL))
        return false;

    PipelineStage *stage = &p->stages[p->count++];
    stage->cmd = command_from_words(words, argc);
    stage->stderr_path = stderr_path ? xstrdup(stderr_path) : NULL;
    return true;
}

bool pipeline_build(const char *line, const InputInfo *info, Pipeline *p)
{
    memset(p, 0, sizeof *p);

    char *copy = xstrdup(line);
    if (info->background_index > 0) {
        copy[info->background_index] = '\0';
        p->background = true;
    }

    int count;
    char **words = str_split_words(copy, &count);
    free(copy);

    bool ok = true;
    int start = 0;
    for (int i = 0; i <= count && ok; i++) {
        if (i < count && strcmp(words[i], "|") != 0)
            continue;
        ok = add_stage(p, words + start, i - start);
        start = i + 1;
    }

    str_free_words(words);
    if (!ok)
        pipeline_free(p);
    return ok;
}

void pipeline_free(Pipeline *p)
{
    for (int i = 0; i < p->count; i++) {
        command_free(p->stages[i].cmd);
        free(p->stages[i].stderr_path);
    }
    p->count = 0;
}
