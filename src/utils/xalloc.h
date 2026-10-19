#ifndef UTILS_XALLOC_H
#define UTILS_XALLOC_H

#include <stddef.h>

/*
 * Allocation wrappers for the shell.
 * On allocation failure they print an error and terminate the shell,
 * so callers never have to handle a NULL result.
 */
void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
char *xstrdup(const char *s);
char *xstrndup(const char *s, size_t n);

#endif /* UTILS_XALLOC_H */
