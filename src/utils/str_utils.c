#include "utils/str_utils.h"

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "utils/xalloc.h"

bool str_is_blank(const char *s)
{
    for (; *s; s++) {
        if (!isspace((unsigned char)*s))
            return false;
    }
    return true;
}

void str_trim(char *s)
{
    size_t len = strlen(s);
    size_t start = 0;
    size_t end = len;

    while (start < len && isspace((unsigned char)s[start]))
        start++;
    while (end > start && isspace((unsigned char)s[end - 1]))
        end--;

    memmove(s, s + start, end - start);
    s[end - start] = '\0';
}

int str_count_words(const char *s)
{
    int count = 0;
    bool in_word = false;

    for (; *s; s++) {
        if (*s == ' ') {
            in_word = false;
        } else if (!in_word) {
            count++;
            in_word = true;
        }
    }
    return count;
}

char **str_split_words(const char *s, int *count)
{
    int n = str_count_words(s);
    char **words = xcalloc((size_t)n + 1, sizeof *words);

    for (int i = 0; i < n; i++) {
        while (*s == ' ')
            s++;
        const char *start = s;
        while (*s && *s != ' ')
            s++;
        words[i] = xstrndup(start, (size_t)(s - start));
    }

    if (count)
        *count = n;
    return words;
}

void str_free_words(char **words)
{
    if (!words)
        return;
    for (char **w = words; *w; w++)
        free(*w);
    free(words);
}

bool str_is_integer(const char *s)
{
    if (*s == '-')
        s++;
    return str_is_unsigned(s);
}

bool str_is_unsigned(const char *s)
{
    if (*s == '\0')
        return false;
    for (; *s; s++) {
        if (!isdigit((unsigned char)*s))
            return false;
    }
    return true;
}

bool str_ends_with(const char *s, const char *suffix)
{
    size_t len = strlen(s);
    size_t suffix_len = strlen(suffix);
    return len >= suffix_len && strcmp(s + len - suffix_len, suffix) == 0;
}

bool str_starts_with_word(const char *s, const char *word)
{
    size_t len = strlen(word);
    return strncmp(s, word, len) == 0 && (s[len] == ' ' || s[len] == '\0');
}
