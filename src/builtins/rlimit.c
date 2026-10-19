#include "builtins/rlimit.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define MAX_LIMIT_TOKEN 64

static const struct {
    const char *name;    /* name used in "rlimit set" */
    const char *label;   /* name used in "rlimit show" */
    int resource;        /* RLIMIT_* constant */
} RESOURCES[RES_COUNT] = {
    [RES_CPU]    = { "cpu",    "CPU time",   RLIMIT_CPU    },
    [RES_NOFILE] = { "nofile", "Open files", RLIMIT_NOFILE },
    [RES_MEM]    = { "mem",    "Memory",     RLIMIT_AS     },
    [RES_FSIZE]  = { "fsize",  "File size",  RLIMIT_FSIZE  },
};

/* Order of the lines printed by "rlimit show". */
static const ResourceId SHOW_ORDER[] = { RES_CPU, RES_MEM, RES_FSIZE, RES_NOFILE };

void rlimit_reset(ResourceLimits *limits)
{
    memset(limits, 0, sizeof *limits);
}

static int find_resource(const char *name)
{
    for (int i = 0; i < RES_COUNT; i++) {
        if (strcmp(name, RESOURCES[i].name) == 0)
            return i;
    }
    return -1;
}

/* Parses "5", "10M", "unlimited", ... Size suffixes are only valid for
 * mem and fsize. */
static bool parse_value(const char *s, ResourceId id, rlim_t *out)
{
    if (strcmp(s, "unlimited") == 0) {
        *out = RLIM_INFINITY;
        return true;
    }
    if (!isdigit((unsigned char)*s))
        return false;

    char *end;
    errno = 0;
    unsigned long long value = strtoull(s, &end, 10);
    if (errno != 0)
        return false;

    unsigned long long multiplier = 1;
    if (*end != '\0') {
        if (id == RES_CPU || id == RES_NOFILE)
            return false;
        if (strcasecmp(end, "B") == 0)
            multiplier = 1ULL;
        else if (strcasecmp(end, "K") == 0 || strcasecmp(end, "KB") == 0)
            multiplier = 1ULL << 10;
        else if (strcasecmp(end, "M") == 0 || strcasecmp(end, "MB") == 0)
            multiplier = 1ULL << 20;
        else if (strcasecmp(end, "G") == 0 || strcasecmp(end, "GB") == 0)
            multiplier = 1ULL << 30;
        else
            return false;
    }

    *out = (rlim_t)(value * multiplier);
    return true;
}

/* Parses one "resource=soft[:hard]" token. */
static bool parse_assignment(char *token, ResourceLimits *limits)
{
    char *eq = strchr(token, '=');
    *eq = '\0';

    int id = find_resource(token);
    if (id < 0)
        return false;

    char *soft_str = eq + 1;
    char *hard_str = strchr(soft_str, ':');
    if (hard_str)
        *hard_str++ = '\0';

    rlim_t soft, hard;
    if (!parse_value(soft_str, id, &soft))
        return false;
    if (hard_str) {
        if (!parse_value(hard_str, id, &hard))
            return false;
    } else {
        hard = soft;  /* no hard value: hard limit equals the soft limit */
    }
    if (soft > hard)
        return false;

    limits->set[id]  = true;
    limits->soft[id] = soft;
    limits->hard[id] = hard;
    return true;
}

bool rlimit_parse_set(char *spec, ResourceLimits *limits)
{
    rlimit_reset(limits);

    if (strncmp(spec, "set ", 4) != 0)
        return false;

    char *p = spec + 4;
    for (;;) {
        while (*p == ' ')
            p++;
        if (*p == '\0')
            return false;  /* limits without a command to run */

        size_t len = strcspn(p, " ");
        if (!memchr(p, '=', len))
            break;         /* first token that is not a limit: the command */

        char token[MAX_LIMIT_TOKEN];
        if (len >= sizeof token)
            return false;
        memcpy(token, p, len);
        token[len] = '\0';

        if (!parse_assignment(token, limits))
            return false;
        p += len;
    }

    memmove(spec, p, strlen(p) + 1);
    limits->active = true;
    return true;
}

static void print_value(const char *kind, rlim_t value, const char *end)
{
    if (value == RLIM_INFINITY)
        printf("%s=unlimited%s", kind, end);
    else
        printf("%s=%llu%s", kind, (unsigned long long)value, end);
}

void rlimit_show(void)
{
    for (size_t i = 0; i < sizeof SHOW_ORDER / sizeof SHOW_ORDER[0]; i++) {
        ResourceId id = SHOW_ORDER[i];
        struct rlimit rl;

        if (getrlimit(RESOURCES[id].resource, &rl) != 0) {
            perror("getrlimit");
            return;
        }
        printf("%s: ", RESOURCES[id].label);
        print_value("soft", rl.rlim_cur, ", ");
        print_value("hard", rl.rlim_max, "\n");
    }
}

void rlimit_apply(const ResourceLimits *limits)
{
    if (!limits || !limits->active)
        return;

    for (int i = 0; i < RES_COUNT; i++) {
        if (!limits->set[i])
            continue;

        struct rlimit rl = { .rlim_cur = limits->soft[i], .rlim_max = limits->hard[i] };
        if (setrlimit(RESOURCES[i].resource, &rl) != 0) {
            perror("setrlimit");
            _exit(EXIT_FAILURE);
        }
    }
}
