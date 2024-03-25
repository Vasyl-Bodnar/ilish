#ifndef EXPR_H_
#define EXPR_H_

#include <sys/types.h>

/// @file expr.h
/// @brief Representation of Atom and SExpr.

/// @brief Expr type.
enum expr {
  Null,
  Bool,
  Chr,
  UniChr,
  Num,
  Str,
  Symb,
  List,
  Vec,
};

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
    char ch;               /// Chr, Err (byte)
    size_t uch;            /// UniChr
    ssize_t num;           /// Num
    char *str;             /// Str, Symb
    struct exprs_t *exprs; /// List
  };
} expr_t;

/// @brief Frees `str` and `exprs`
void delete_expr(expr_t expr);

/// @brief Clone `str` and `exprs`, otherwise copy
expr_t clone_expr(expr_t expr);

int check_symb_expr(expr_t expr, const char *symb);

/// @brief Print an expr and convert span to str as needed.
void print_expr(expr_t expr);

#endif // EXPR_H_
