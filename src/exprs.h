#ifndef EXPRS_H_
#define EXPRS_H_

#include <stddef.h>

/// @file exprs.h
/// @brief SExpr as vector of `expr`.

/// @brief SExpr object.
typedef struct exprs_t {
  struct expr_t *arr;
  size_t len;
  size_t cap;
} exprs_t;

/// @brief Create the `exprs` object with initial capacity.
/// @param cap Initial capacity.
/// @returns `exprs` object.
exprs_t *create_exprs(size_t cap);

/// @brief Frees the `exprs`, `arr`, and all the inner `expr`.
void delete_exprs(exprs_t *exprs);

/// @brief Push an owned `expr` to `exprs`.
///
/// Expands `exprs` cap by factor of 2 if reached.
/// `exprs` assumes ownership of `expr` if it uses dynamically allocated data
/// (e.g. Num `expr` copied, Str/List owned).
/// @param expr The owned `expr` to be pushed.
void push_exprs(exprs_t *exprs, struct expr_t expr);

/// @brief Static slice referencing the original `arr` from
/// the `start` to `len`.
/// @param start Starting location.
/// @return Static slice from `start` to `len`.
exprs_t slice_start_exprs(exprs_t *exprs, size_t start);

/// @brief Find a symbol in `exprs`.
/// @param symb Non-null char* to find.
/// @param src Original string to allocate spans.
/// @return Index if found, -1 otherwise.
int find_symb_exprs(exprs_t *exprs, const char *symb, const char *src);

/// @brief Calls newline separated `print_expr` on each element.
/// @param src Original string to allocate spans.
void print_exprs(exprs_t *exprs, const char *src);

#endif // EXPRS_H_
