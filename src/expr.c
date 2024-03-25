#include "expr.h"
#include "exprs.h"
#include <malloc.h>
#include <string.h>

void delete_expr(expr_t expr) {
  switch (expr.type) {
  case Null:
  case Bool:
  case Chr:
  case UniChr:
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

expr_t clone_expr(expr_t expr) {
  switch (expr.type) {
  case Null:
  case Bool:
  case Chr:
  case UniChr:
  case Num:
  default:
    return expr;
  case Str:
  case Symb:
    return (expr_t){.loc = expr.loc,
                    .line = expr.line,
                    .type = expr.type,
                    .str = strdup(expr.str)};
  case List:
  case Vec:
    return (expr_t){.loc = expr.loc,
                    .line = expr.line,
                    .type = expr.type,
                    .exprs = clone_exprs(expr.exprs)};
  }
}

int check_symb_expr(expr_t expr, const char *symb) {
  switch (expr.type) {
  case Null:
  case Bool:
  case Chr:
  case UniChr:
  case Num:
  case Str:
    break;
  case Symb:
    if (!strcmp(expr.str, symb))
      return 1;
    else
      break;
  case List:
  case Vec:
    return find_symb_exprs(expr.exprs, symb) > -1;
  }
  return 0;
}

void print_expr(expr_t expr) {
  switch (expr.type) {
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
  case UniChr:
    printf("#\\%zx", expr.uch);
    break;
  case Num:
    printf("%zd", expr.num);
    break;
  case Str:
    printf("\"%s\"", expr.str);
    break;
  case Symb:
    printf("%s", expr.str);
    break;
  case List:
    printf("(");
    print_exprs(expr.exprs);
    printf(")");
    break;
  case Vec:
    printf("#(");
    print_exprs(expr.exprs);
    printf(")");
    break;
  }
}
