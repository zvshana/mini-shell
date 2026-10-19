#ifndef SHELL_PROCESS_H
#define SHELL_PROCESS_H

#include "builtins/rlimit.h"

/*
 * Prints a message for a child that did not finish cleanly:
 * a resource limit that was hit, or the signal that killed it.
 * `limits` may be NULL.
 */
void process_report_status(int status, const ResourceLimits *limits);

/* Collects background children that have finished, without blocking. */
void process_reap_background(void);

#endif /* SHELL_PROCESS_H */
