#ifndef ERRS_H_
#define ERRS_H_

#include <stddef.h>

/// @file errs.h
/// @brief Error and vector of errors for parser and compiler error handling

/// @brief Error enum type
/// TODO: Move compiler errors to a different err type
enum err {
  Eof,

  QuoteUnfinishedEof,
  UnquoteOutsideQuote,
  SpliceOutsideQuote,

  ImproperSpecial,
  ImproperFalse,
  ImproperTrue,

  ImproperCharName,
  ImproperAlarmName,
  ImproperBackspaceName,
  ImproperDeleteName,
  ImproperEscapeName,
  ImproperNewlineNullName,
  ImproperReturnName,
  ImproperSpaceName,
  ImproperTabName,

  StrUnfinishedEof,
  SymbUnfinishedEof,

  ListUnfinishedEof,
  ListUnmatchedRight,
  EmptyList,

  VecUnfinishedEof,

  ParserFailure,
  UnmatchedFun,
  UndefinedSymb,
  ExpectedFixnum,
  ExpectedNonUniChar,
  ExpectedFunSymb,
  ExpectedSymb,
  ExpectedList,
  ExpectedAtLeastUnary,
  ExpectedAtLeastBinary,
  ExpectedAtMostBinary,
  ExpectedNoArg,
  ExpectedUnary,
  ExpectedBinary,
  ExpectedTrinary,
};

/// @brief Error struct with type and location
typedef struct err_t {
  enum err type;
  size_t line;
  size_t loc;
} err_t;

/// @brief Vector of errors
typedef struct errs_t {
  err_t *arr;
  size_t len;
  size_t cap;
} errs_t;

/// @brief Creates an `errs` object with initial capacity of `cap`
/// @param cap Initial capacity
/// @return `errs` object
errs_t *create_errs(size_t cap);

/// @brief Free the `errs`
void delete_errs(errs_t *errs);

/// @brief Push `err` to `errs`
/// @param err Error object
void push_errs(errs_t *errs, err_t err);

/// @brief Print single error
void print_err(err_t err);
/// @brief Print vector of errors
void print_errs(errs_t *errs);
/// @brief Print single error with an newlines
void print_lined_err(err_t err);
/// @brief Print vector of errors separated by newlines
void print_lined_errs(errs_t *errs);

#endif // ERRS_H_
