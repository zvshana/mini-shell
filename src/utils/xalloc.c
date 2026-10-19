#include "utils/xalloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *check(void *ptr)
{
    if (ptr == NULL) {
        perror("malloc");
        printf("ERR\n");
        exit(EXIT_FAILURE);
    }
    return ptr;
}

void *xmalloc(size_t size)
{
    return check(malloc(size ? size : 1));
}

void *xcalloc(size_t count, size_t size)
{
    return check(calloc(count ? count : 1, size ? size : 1));
}

char *xstrdup(const char *s)
{
    return check(strdup(s));
}

char *xstrndup(const char *s, size_t n)
{
    return check(strndup(s, n));
}
