#include "expr.h"
#include "exprs.h"
#include <malloc.h>

void delete_expr(expr_t expr) {
  switch (expr.type) {
  case Err:
  case Null:
  case Bool:
  case Chr:
  case Num:
    break;
  case Str:
  case Symb:
    free(expr.str);
    break;
  case List:
  case Vec:
    delete_exprs(expr.exprs);
    break;
  }
}

void print_expr(expr_t expr, const char *src) {
  switch (expr.type) {
  case Err:
    printf("Error %d!", expr.ch);
    break;
  case Null:
    printf("()");
    break;
  case Bool:
    if (expr.ch)
      printf("#t");
    else
      printf("#f");
    break;
  case Chr:
    printf("#\\%c", expr.ch);
    break;
  case Num:
    printf("%zu", expr.num);
    break;
  case Str:
    printf("\"%s\"", expr.str);
    break;
  case Symb:
    printf("%s", expr.str);
    break;
  case List:
    printf("(");
    print_exprs(expr.exprs, src);
    printf(")");
    break;
  case Vec:
    printf("#(");
    print_exprs(expr.exprs, src);
    printf(")");
    break;
  }
}
