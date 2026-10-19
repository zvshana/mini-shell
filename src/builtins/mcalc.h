#ifndef BUILTINS_MCALC_H
#define BUILTINS_MCALC_H

#include <stdbool.h>

/*
 * The `mcalc` built-in: parallel matrix addition / subtraction.
 *
 *     mcalc "(2,2:1,2,3,4)" "(2,2:5,6,7,8)" ... "ADD"|"SUB"
 *
 * Matrices are combined pairwise as a reduction tree, one thread per pair,
 * level by level, keeping the input order:
 *     M1 M2 M3 M4  ->  (M1 op M2) op (M3 op M4)
 * With an odd count, the last matrix moves up to the next level unchanged.
 */
typedef enum { MAT_SUB, MAT_ADD } MatrixOp;

typedef struct {
    int      rows;
    int      cols;
    int      count;   /* number of matrices in `items` */
    MatrixOp op;
    int    **items;   /* each item holds rows * cols values, row by row */
} MatrixSet;

/* Parses the arguments (the text after "mcalc"). Returns false on any
 * format error. */
bool mcalc_parse(const char *args, MatrixSet *set);

/* Runs the reduction. Returns the result matrix (owned by `set`), or
 * NULL if a thread failed. */
const int *mcalc_reduce(MatrixSet *set);

/* Prints a matrix as "(rows,cols:v1,v2,...)". */
void mcalc_print(const MatrixSet *set, const int *matrix);

void mcalc_free(MatrixSet *set);

#endif /* BUILTINS_MCALC_H */
