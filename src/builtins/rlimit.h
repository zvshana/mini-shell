#ifndef BUILTINS_RLIMIT_H
#define BUILTINS_RLIMIT_H

#include <stdbool.h>
#include <sys/resource.h>

/*
 * The `rlimit` built-in:
 *     rlimit show
 *     rlimit set resource=soft[:hard] [resource=soft[:hard] ...] command [args...]
 *
 * Supported resources: cpu (seconds), mem and fsize (B/K/KB/M/MB/G/GB),
 * nofile (count). Limits apply only to the child that runs the command.
 */
typedef enum {
    RES_CPU,
    RES_NOFILE,
    RES_MEM,
    RES_FSIZE,
    RES_COUNT
} ResourceId;

typedef struct {
    bool   active;            /* at least one "rlimit set" was given */
    bool   set[RES_COUNT];
    rlim_t soft[RES_COUNT];
    rlim_t hard[RES_COUNT];
} ResourceLimits;

void rlimit_reset(ResourceLimits *limits);

/*
 * Parses "set cpu=2:3 mem=50M command args..." into `limits`.
 * On success `spec` is rewritten in place to hold only "command args...".
 */
bool rlimit_parse_set(char *spec, ResourceLimits *limits);

/* Prints the current limits of the shell process. */
void rlimit_show(void);

/* Applies the limits with setrlimit(). Must be called in the child process. */
void rlimit_apply(const ResourceLimits *limits);

#endif /* BUILTINS_RLIMIT_H */
