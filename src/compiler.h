#ifndef COMPILER_H
#define COMPILER_H

#include "errs.h"

typedef struct compiler_t {
  struct exprs_t *input;
  size_t line;
  size_t loc;
  size_t label;
  struct env_t *env;
  struct strs_t *output;
  struct errs_t *errs;
  const char *src; // For spans
} compiler_t;

compiler_t *create_compiler();

/// Deletes `input` and `compiler`
void delete_compiler(compiler_t *compiler);

/// Check for errors in the compiler
int has_errc(compiler_t *compiler);

/// Forces compiler error at specific line and loc
void errc(compiler_t *compiler, enum err err);

// WIP and mispelled
// void uniquefy(exprs_t *exprs, exprs_t *known, const char *src);

/// Input `exprs` is managed by the compiler.
/// `src` must be manually managed.
/// Output `strs` uses stack allocated strings, the struct must be freed with
/// `delete_ref_strs`, not `delete_strs`.
struct strs_t *compile(compiler_t *compiler, struct exprs_t *exprs,
                       const char *src);

#endif // COMPILER_H
