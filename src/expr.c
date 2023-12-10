#include "expr.h"
#include "exprs.h"
#include <malloc.h>
#include <string.h>

// char *span_as_str(span_t span, const char *src) {
//   char *str = strndup(src + span.start, span.len);
//   str[span.len] = '\0';
//   return str;
// }
// 
// void span_to_str(expr_t *span, const char *src) {
//   switch (span->type) {
//   case Err:
//   case Char:
//   case Num:
//   case Str:
//   case Symb:
//   case List:
//     break;
//   case StrSpan:
//     span->type = Str;
//     span->str = span_as_str(span->span, src);
//     break;
//   case SymbSpan:
//     span->type = Symb;
//     span->str = span_as_str(span->span, src);
//     break;
//   }
// }

void delete_expr(expr_t expr) {
  switch (expr.type) {
  case Err:
  case Char:
  case Num:
  case StrSpan:
  case SymbSpan:
    break;
  case Str:
  case Symb:
    free(expr.str);
    break;
  case List:
    delete_exprs(expr.exprs);
    break;
  }
}

void print_expr(expr_t expr, const char *src) {
  switch (expr.type) {
  case Err:
    printf("ERR %d!", expr.ch);
    break;
  case Char:
    printf("'%c'", expr.ch);
    break;
  case Num:
    printf("%zu", expr.num);
    break;
  case StrSpan:
    printf("strspan");
    break;
  case Str: 
    printf("\"%s\"", expr.str);
    break;
  case SymbSpan:
    printf("symbspan");
    break;
  case Symb: 
    printf("`%s`", expr.str);
    break;
  case List: {
    printf("(");
    print_exprs(expr.exprs, src);
    printf(")");
    break;
  }
  }
}
