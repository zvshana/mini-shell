#ifndef CONFIG_H
#define CONFIG_H

/* Maximum length of a command line, including all arguments. */
#define MAX_INPUT_LEN 1024

/* Maximum number of arguments a single command may receive
 * (not counting the command name itself). */
#define MAX_ARGS 6

/* Maximum number of commands that can be chained with '|'. */
#define MAX_PIPELINE_STAGES 16

/* Exit status of a child whose execvp() failed (same convention as bash). */
#define EXIT_EXEC_FAILED 127

#endif /* CONFIG_H */
