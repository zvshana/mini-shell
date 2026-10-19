#ifndef SHELL_PIPELINE_H
#define SHELL_PIPELINE_H

#include <stdbool.h>

#include "config.h"
#include "shell/command.h"
#include "shell/parser.h"

/* One command of a pipeline, with its optional stderr redirection. */
typedef struct {
    Command *cmd;
    char    *stderr_path;   /* target of "2>", or NULL */
} PipelineStage;

/* cmd1 [2> file] | cmd2 [2> file] | ... [&] */
typedef struct {
    PipelineStage stages[MAX_PIPELINE_STAGES];
    int  count;
    bool background;
} Pipeline;

/* Builds a pipeline from a validated line. Returns false if malformed. */
bool pipeline_build(const char *line, const InputInfo *info, Pipeline *out);

void pipeline_free(Pipeline *p);

#endif /* SHELL_PIPELINE_H */
