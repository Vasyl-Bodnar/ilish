#ifndef ERRS_H_
#define ERRS_H_

#include <stddef.h>

enum err {
    Eof,
    CharUnfinishedEof,
    StrUnfinishedEof,
    ListUnfinishedEof,
    ListUnmatchedRight,
    EmptyList,

    // TODO: Move to a different err type
    ParserFailure,
    UnmatchedFun,
    UndefinedSymb,
    ExpectedFunSymb,
    ExpectedSymb,
    ExpectedList,
    ExpectedUnary,
    ExpectedAtLeastUnary,
    ExpectedBinary,
    ExpectedAtLeastBinary,
    ExpectedTrinary,
};

typedef struct err_t {
  enum err type;
  size_t line;
  size_t loc;
} err_t;

typedef struct errs_t {
  err_t *arr;
  size_t len;
  size_t cap;
} errs_t;

/// Mallocs an `errs` with initial capacity of `cap`
errs_t *create_errs(size_t cap);

/// `errs` should own the `err`
void push_errs(errs_t *errs, err_t err);

/// Deletes the inner `err`s as well
void delete_errs(errs_t *errs);

void print_err(err_t err);
void print_errs(errs_t *errs);
void print_lined_err(err_t err);
void print_lined_errs(errs_t *errs);

#endif // ERRS_H_
