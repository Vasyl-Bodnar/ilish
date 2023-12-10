#ifndef EXPRS_H_
#define EXPRS_H_

#include <stddef.h>

typedef struct exprs_t {
  struct expr_t *arr;
  size_t len;
  size_t cap;
} exprs_t;

/// Mallocs an `exprs` with initial capacity of `cap`
exprs_t *create_exprs(size_t cap);

/// `exprs` should own the `expr`
void push_exprs(exprs_t *exprs, struct expr_t expr);

/// Sliced are not to be deleted or freed
exprs_t slice_start_exprs(exprs_t *exprs, size_t start);

/// Find the loc of a `symb` in `exprs` or sub `exprs` (loc in top exprs is given)
/// Return -1 if none are found
int find_symb_exprs(exprs_t *exprs, const char *symb, const char *src);

/// Deletes the inner `expr`s as well
void delete_exprs(exprs_t *exprs);

/// Activates spans into strings
void print_exprs(exprs_t *exprs, const char *src);

#endif // EXPRS_H_
