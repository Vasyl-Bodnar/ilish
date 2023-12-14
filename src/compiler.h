#ifndef COMPILER_H
#define COMPILER_H

#include "errs.h"

/// @file compiler.h
/// @brief SExpr to assembly compiler.

/// @brief Reusable compiler object
typedef struct compiler_t {
  struct exprs_t *input; ///> Sexprs to compile.
  size_t line;           ///> Current file line.
  size_t loc;            ///> Current file loc.
  size_t label;          ///> Latest in-use label.
  struct env_t *env;     ///> The environment to keep track of stack and vars.
  struct strs_t *output; ///> The output asm.
  struct errs_t *errs;   ///> The errors.
  const char *src;       ///> The original source file for spans..
} compiler_t;

/// @brief Create a compiler object. Does not create any inner objects.
compiler_t *create_compiler();

/// @brief Frees the compiler along with any nonfreed `exprs`, `env`, and
/// `errs`.
void delete_compiler(compiler_t *compiler);

/// @brief Check for errors in the compiler.
int has_errc(compiler_t *compiler);

/// @brief Forces compiler error at a specific line and loc.
/// @param compiler The compiler and its `line` and `loc`.
/// @param err The error enum code.
void errc(compiler_t *compiler, enum err err);

/// @brief Main recallable function for all compilations.
/// @param compiler The reusable compiler.
/// @param exprs The exprs to compile. Managed by the compiler.
/// @param src The src file to use for spans.
/// @return strs object containing the compiled asm.
struct strs_t *compile(compiler_t *compiler, struct exprs_t *exprs,
                       const char *src);

#endif // COMPILER_H
