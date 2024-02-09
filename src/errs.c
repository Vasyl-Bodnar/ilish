#include "errs.h"
#include <err.h>
#include <malloc.h>

errs_t *create_errs(size_t cap) {
  errs_t *errs = malloc(sizeof(*errs));
  errs->arr = malloc(sizeof(*errs->arr) * cap);
  errs->len = 0;
  errs->cap = cap;
  return errs;
}

void delete_errs(errs_t *errs) {
  free(errs->arr);
  free(errs);
}

void expand_errs(errs_t *errs) {
  if (errs->len >= errs->cap) {
    errs->cap <<= 1;
    errs->arr = reallocarray(errs->arr, errs->cap, sizeof(*errs->arr));
    if (!errs->arr) {
      err(1, "Failed to allocate memory for array of errs");
    }
  }
}

void push_errs(errs_t *errs, err_t expr) {
  expand_errs(errs);
  errs->arr[errs->len] = expr;
  errs->len++;
}

void print_err(err_t err) {
  switch (err.type) {
  case Eof:
    printf("Unexpected Null Character");
    break;
  case QuoteUnfinishedEof:
    printf("Special symbol ' is unfinished. Did you forget to finish the quote "
           "like '(), '(a b). Chars use #\\c instead of 'c'");
    break;
  case SpecialUnfinishedEof:
    printf("Special symbol # is unfinished. Did you mean a char #\\a, bool #t, "
           "or vector #()?");
    break;
  case CharUnfinishedEof:
    printf("Char is unfinished, check for unfinished #\\.");
    break;
  case StrUnfinishedEof:
    printf("String is unfinished, check for matching \"\".");
    break;
  case ListUnfinishedEof:
    printf("SExpr is unfinished, check for matching parens ( ).");
    break;
  case ListUnmatchedRight:
    printf(
        "Right paren ) is unmatched, either it is extra or lacks a left paren "
        "(.");
    break;
  case EmptyList:
    printf("Empty lists are not allowed.");
    break;
  case VecUnfinishedEof:
    printf("Vector is unfinished, check for matching parens #( ).");
    break;
  case VecUnmatchedRight:
    printf("Right vector paren ) is unmatched, either it is extra or lacks "
           "a left paren "
           "#(.");
    break;
  case EmptyVec:
    printf("Empty lists are not allowed.");
    break;
  case ParserFailure:
    printf("Parser has failed, but compilation was attempted.");
    break;
  case UnmatchedFun:
    printf("Undefined function.");
    break;
  case UndefinedSymb:
    printf("Undefined symbol.");
    break;
  case ExpectedFixnum:
    printf("Expected a number.");
    break;
  case ExpectedFunSymb:
    printf("Expected function name to be a symbol.");
    break;
  case ExpectedSymb:
    printf("Expected a symbol.");
    break;
  case ExpectedList:
    printf("Expected a list.");
    break;
  case ExpectedUnary:
    printf("Expected 1 argument to function.");
    break;
  case ExpectedAtLeastUnary:
    printf("Expected at least 1 argument to function.");
    break;
  case ExpectedBinary:
    printf("Expected 2 arguments to function.");
    break;
  case ExpectedAtLeastBinary:
    printf("Expected at least 2 arguments to function.");
    break;
  case ExpectedAtMostBinary:
    printf("Expected at most 2 arguments to function.");
    break;
  case ExpectedTrinary:
    printf("Expected 3 arguments to function.");
    break;
  }
}

void print_lined_err(err_t err) {
  printf("At line %lu, char %lu: ", err.line, err.loc);
  print_err(err);
}

void print_errs(errs_t *errs) {
  for (size_t i = 0; i < errs->len - 1; i++) {
    print_err(errs->arr[i]);
    puts("");
  }
  print_err(errs->arr[errs->len - 1]);
}

void print_lined_errs(errs_t *errs) {
  for (size_t i = 0; i < errs->len - 1; i++) {
    print_lined_err(errs->arr[i]);
    puts("");
  }
  print_lined_err(errs->arr[errs->len - 1]);
}
