#ifndef EXPR_H_
#define EXPR_H_

#include <sys/types.h>

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

typedef struct span_t {
  size_t start;
  size_t len;
} span_t;

typedef struct expr_t {
  size_t line;
  size_t loc;
  enum expr type;
  union {
    char ch;               // Char, Err (byte)
    ssize_t num;           // Num
    span_t span;           // StrSpan, SymbSpan
    char *str;             // Str, Symb
    struct exprs_t *exprs; // List
  };
} expr_t;

void delete_expr(expr_t expr);

char *span_as_str(span_t span, const char *src);
void span_to_str(expr_t *span, const char *src);

void print_expr(expr_t expr, const char *src);

#endif // EXPR_H_
