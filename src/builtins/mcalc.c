#include "builtins/mcalc.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "utils/str_utils.h"
#include "utils/xalloc.h"

/* ------------------------------------------------------------------ */
/*  Parsing                                                           */
/* ------------------------------------------------------------------ */

static bool expect(const char **p, char c)
{
    if (**p != c)
        return false;
    (*p)++;
    return true;
}

static bool parse_int(const char **p, int *out)
{
    const char *s = *p;
    if (!(isdigit((unsigned char)s[0]) || (s[0] == '-' && isdigit((unsigned char)s[1]))))
        return false;

    char *end;
    errno = 0;
    long value = strtol(s, &end, 10);
    if (errno != 0 || value < INT_MIN || value > INT_MAX)
        return false;

    *out = (int)value;
    *p = end;
    return true;
}

/* Parses "\"ADD\"" or "\"SUB\"". */
static bool parse_operation(const char *token, MatrixOp *op)
{
    if (strcmp(token, "\"ADD\"") == 0)
        *op = MAT_ADD;
    else if (strcmp(token, "\"SUB\"") == 0)
        *op = MAT_SUB;
    else
        return false;
    return true;
}

/* Parses "\"(rows,cols:v1,v2,...)\"" into set->items[set->count]. All
 * matrices must have the same dimensions as the first one. */
static bool parse_matrix(const char *token, MatrixSet *set)
{
    size_t len = strlen(token);
    if (len < 2 || token[0] != '"' || token[len - 1] != '"')
        return false;

    const char *p = token + 1;
    const char *end = token + len - 1;   /* the closing quote */
    int rows, cols;

    if (!expect(&p, '(') || !parse_int(&p, &rows) || !expect(&p, ',') ||
        !parse_int(&p, &cols) || !expect(&p, ':'))
        return false;
    /* A line of MAX_INPUT_LEN characters cannot hold more values than that. */
    if (rows <= 0 || cols <= 0 || rows > MAX_INPUT_LEN / cols)
        return false;

    if (set->count == 0) {
        set->rows = rows;
        set->cols = cols;
    } else if (rows != set->rows || cols != set->cols) {
        return false;
    }

    int size = rows * cols;
    int *values = xcalloc((size_t)size, sizeof *values);
    set->items[set->count++] = values;

    for (int i = 0; i < size; i++) {
        if (i > 0 && !expect(&p, ','))
            return false;
        if (!parse_int(&p, &values[i]))
            return false;
    }
    return expect(&p, ')') && p == end;
}

bool mcalc_parse(const char *args, MatrixSet *set)
{
    memset(set, 0, sizeof *set);

    /* Arguments are separated by exactly one space. */
    if (args[0] != ' ')
        return false;
    for (const char *c = args; *c; c++) {
        if (isspace((unsigned char)c[0]) && isspace((unsigned char)c[1]))
            return false;
    }

    int count;
    char **tokens = str_split_words(args, &count);

    /* At least two matrices and the operation. */
    bool ok = count >= 3 && parse_operation(tokens[count - 1], &set->op);
    if (ok) {
        set->items = xcalloc((size_t)count - 1, sizeof *set->items);
        for (int i = 0; ok && i < count - 1; i++)
            ok = parse_matrix(tokens[i], set);
    }

    str_free_words(tokens);
    if (!ok)
        mcalc_free(set);
    return ok;
}

/* ------------------------------------------------------------------ */
/*  Parallel reduction                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    const int *left;
    const int *right;
    int        size;
    MatrixOp   op;
} PairTask;

/* Thread body: returns a newly allocated left (op) right, or NULL. */
static void *combine_pair(void *arg)
{
    const PairTask *task = arg;
    int *result = malloc((size_t)task->size * sizeof *result);
    if (!result) {
        perror("malloc");
        return NULL;
    }

    for (int i = 0; i < task->size; i++) {
        result[i] = task->op == MAT_ADD ? task->left[i] + task->right[i]
                                        : task->left[i] - task->right[i];
    }
    return result;
}

/* Combines one level of the tree. Returns false if a thread failed. */
static bool reduce_level(MatrixSet *set)
{
    int pairs = set->count / 2;
    bool has_carry = set->count % 2 == 1;
    int *carry = has_carry ? set->items[set->count - 1] : NULL;

    pthread_t *threads = xcalloc((size_t)pairs, sizeof *threads);
    PairTask  *tasks   = xcalloc((size_t)pairs, sizeof *tasks);
    int      **results = xcalloc((size_t)pairs, sizeof *results);
    int created = 0;
    bool ok = true;

    for (int i = 0; i < pairs; i++) {
        tasks[i] = (PairTask){
            .left  = set->items[2 * i],
            .right = set->items[2 * i + 1],
            .size  = set->rows * set->cols,
            .op    = set->op,
        };
        if (pthread_create(&threads[i], NULL, combine_pair, &tasks[i]) != 0) {
            perror("pthread_create");
            ok = false;
            break;
        }
        created++;
    }

    for (int i = 0; i < created; i++) {
        void *result = NULL;
        pthread_join(threads[i], &result);
        results[i] = result;
        if (!result)
            ok = false;
    }

    if (ok) {
        for (int i = 0; i < 2 * pairs; i++)
            free(set->items[i]);
        for (int i = 0; i < pairs; i++)
            set->items[i] = results[i];
        if (has_carry)
            set->items[pairs] = carry;
        set->count = pairs + (has_carry ? 1 : 0);
    } else {
        for (int i = 0; i < created; i++)
            free(results[i]);
    }

    free(threads);
    free(tasks);
    free(results);
    return ok;
}

const int *mcalc_reduce(MatrixSet *set)
{
    while (set->count > 1) {
        if (!reduce_level(set))
            return NULL;
    }
    return set->count == 1 ? set->items[0] : NULL;
}

void mcalc_print(const MatrixSet *set, const int *matrix)
{
    int size = set->rows * set->cols;

    printf("(%d,%d:", set->rows, set->cols);
    for (int i = 0; i < size; i++)
        printf(i < size - 1 ? "%d," : "%d", matrix[i]);
    printf(")\n");
}

void mcalc_free(MatrixSet *set)
{
    if (set->items) {
        for (int i = 0; i < set->count; i++)
            free(set->items[i]);
        free(set->items);
    }
    memset(set, 0, sizeof *set);
}
