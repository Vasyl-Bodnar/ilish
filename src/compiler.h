#ifndef COMPILER_H
#define COMPILER_H

#include "env.h"
#include "errs.h"

/// @file compiler.h
/// @brief SExpr to assembly compiler.

enum emit {
  Bss,
  Data,
  Fun,
  Main,
  Quotes,
  Body,
  End,
};

/// @brief Reusable compiler object
typedef struct compiler_t {
  ///> Sexprs to compile.
  struct exprs_t *input;
  ///> Current file line.
  size_t line;
  ///> Current file loc.
  size_t loc;
  ///> Approximate heap usage.
  size_t heap;
  ///> Size of the heap. Real heap usage will be higher.
  size_t heap_size;
  ///> Latest branch label.
  size_t label;
  ///> Latest lambda label.
  size_t lambda;
  ///> Latest free var index.
  size_t free;
  ///> Type of data currently in rax
  enum val_type ret_type;
  ///> Arguments in case of a lambda
  struct exprs_t *ret_args;
  ///> The environment to keep track of stack and vars.
  struct env_t *env;
  ///> Which buffer to emit to.
  enum emit emit;
  ///> The bss section for quotes.
  struct strs_t *bss;
  ///> The data section for constants.
  struct strs_t *data;
  ///> The function declarations (uses "double" buffer).
  struct dstrs_t *fun;
  ///> The main function.
  struct strs_t *main;
  ///> The quote declarations.
  struct strs_t *quotes;
  ///> The output asm.
  struct strs_t *body;
  ///> The end of main function.
  struct strs_t *end;
  ///> The errors.
  struct errs_t *errs;
  ///> The original source file for spans.
  const char *src;
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
/// @param heap_size The amount of primary heap to allocate.
/// @param src The src file to use for spans.
/// @return strs object containing the compiled asm.
struct strs_t *compile(compiler_t *compiler, struct exprs_t *exprs,
                       size_t heap_size, const char *src);

#endif // COMPILER_H
