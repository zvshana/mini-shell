#ifndef SHELL_SHELL_H
#define SHELL_SHELL_H

#include <stdbool.h>

#include "shell/dangerous.h"
#include "shell/stats.h"

/* State that lives for the whole shell session. */
typedef struct {
    ShellStats     stats;
    DangerousList *dangerous;
} Shell;

/* Loads the dangerous-command list and prepares the execution log. */
bool shell_init(Shell *sh, const char *dangerous_path, const char *log_path);

/* Read-eval loop. Returns when the user types "done" or stdin closes. */
void shell_run(Shell *sh);

void shell_destroy(Shell *sh);

#endif /* SHELL_SHELL_H */
