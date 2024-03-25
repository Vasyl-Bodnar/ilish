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
  case UnquoteOutsideQuote:
    printf(", or unquote was not used inside a quote.");
    break;
  case SpliceOutsideQuote:
    printf(",@ or unquote-splicing was not used inside a quote.");
    break;
  case ImproperSpecial:
    printf("Your use of # symbol is courageous, but this is not allowed.");
    break;
  case ImproperFalse:
    printf("You misspelled #false.");
    break;
  case ImproperTrue:
    printf("You misspelled #true.");
    break;
  case ImproperCharName:
    printf("You have misspelled a character by its name, make sure it is one "
           "of the allowed names like #space and #tab.");
    break;
  case ImproperAlarmName:
    printf("You have misspelled the #\\alarm character.");
    break;
  case ImproperBackspaceName:
    printf("You have misspelled the #\\backspace character.");
    break;
  case ImproperDeleteName:
    printf("You have misspelled the #\\delete character.");
    break;
  case ImproperEscapeName:
    printf("You have misspelled the #\\escape character.");
    break;
  case ImproperNewlineNullName:
    printf("You have misspelled a character name, are you trying to use "
           "#\\newline or #\\null?");
    break;
  case ImproperReturnName:
    printf("You have misspelled the #\\return character.");
    break;
  case ImproperSpaceName:
    printf("You have misspelled the #\\space character.");
    break;
  case ImproperTabName:
    printf("You have misspelled the #\\tab character.");
    break;
  case StrUnfinishedEof:
    printf("The string was not ended by \" before reaching end of file. Check "
           "for matching double-quotes.");
    break;
  case SymbUnfinishedEof:
    printf("The symb was not ended by | before reaching end of file. Check "
           "for matching pipes.");
    break;
  case VecUnfinishedEof:
    printf("The vector was not ended by ) before reaching end of file. Check "
           "for matching parens.");
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
  case ExpectedNonUniChar:
    printf("Expected a non unicode character.");
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
  case ExpectedAtLeastUnary:
    printf("Expected at least 1 argument to function.");
    break;
  case ExpectedAtLeastBinary:
    printf("Expected at least 2 arguments to function.");
    break;
  case ExpectedAtMostBinary:
    printf("Expected at most 2 arguments to function.");
    break;
  case ExpectedNoArg:
    printf("Expected 0 argument to function.");
    break;
  case ExpectedUnary:
    printf("Expected 1 argument to function.");
    break;
  case ExpectedBinary:
    printf("Expected 2 arguments to function.");
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

void print_charred_err(err_t err) {
  printf("At char %lu: ", err.loc);
  print_err(err);
}

void print_errs(errs_t *errs) {
  for (size_t i = 0; i < errs->len - 1; i++) {
    print_charred_err(errs->arr[i]);
    puts("");
  }
  print_charred_err(errs->arr[errs->len - 1]);
}

void print_lined_errs(errs_t *errs) {
  for (size_t i = 0; i < errs->len - 1; i++) {
    print_lined_err(errs->arr[i]);
    puts("");
  }
  print_lined_err(errs->arr[errs->len - 1]);
}
