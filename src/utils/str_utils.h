#ifndef UTILS_STR_UTILS_H
#define UTILS_STR_UTILS_H

#include <stdbool.h>

/* True if the string is empty or contains only whitespace. */
bool str_is_blank(const char *s);

/* Removes leading and trailing whitespace in place. */
void str_trim(char *s);

/* Number of words in `s`, where words are separated by spaces. */
int str_count_words(const char *s);

/*
 * Splits `s` on spaces into a NULL-terminated array of newly allocated
 * words. The number of words is stored in `*count` (if not NULL).
 * Free the result with str_free_words().
 */
char **str_split_words(const char *s, int *count);

/* Frees an array returned by str_split_words(). Accepts NULL. */
void str_free_words(char **words);

/* "123", "-42" -> true. Empty strings and stray characters -> false. */
bool str_is_integer(const char *s);

/* Digits only, at least one. */
bool str_is_unsigned(const char *s);

bool str_ends_with(const char *s, const char *suffix);

/* True if `s` starts with `word` followed by a space or end of string. */
bool str_starts_with_word(const char *s, const char *word);

#endif /* UTILS_STR_UTILS_H */
