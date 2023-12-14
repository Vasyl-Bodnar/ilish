#ifndef EXPR_H_
#define EXPR_H_

#include <sys/types.h>

/// @file expr.h
/// @brief Representation of Atom and SExpr.

/// @brief Expr type.
enum expr {
  Err,
  Char,
  Num,
  StrSpan,
  SymbSpan,
  Str,
  Symb,
  List,
};

/// @brief String span in input string.
typedef struct span_t {
  size_t start;
  size_t len;
} span_t;

/// @brief Expr object with location, type, and data.
///
/// Note that `str` and `exprs` are dynamically allocated.
/// They must be freed by `delete_expr`.
/// @sa delete_expr
typedef struct expr_t {
  size_t line;
  size_t loc;
  enum expr type;
  union {
    char ch;               /// Char, Err (byte)
    ssize_t num;           /// Num
    span_t span;           /// StrSpan, SymbSpan
    char *str;             /// Str, Symb
    struct exprs_t *exprs; /// List
  };
} expr_t;

/// @brief Frees `str` and `exprs`
void delete_expr(expr_t expr);

/// @brief Represent span as a string.
/// @param span Span struct to use.
/// @param src Original input string.
/// @return Dynamically allocated char*.
char *span_as_str(span_t span, const char *src);

/// @brief Convert a span `expr` to a string.
///
/// Call `span_as_str` and reassign the type and value of the `expr`.
/// Non-span `expr` will be untouched.
/// @param span `expr` to convert.
/// @param src Original input string.
/// @sa span_as_str
void span_to_str(expr_t *span, const char *src);

/// @brief Print an expr and convert span to str as needed.
void print_expr(expr_t expr, const char *src);

#endif // EXPR_H_
