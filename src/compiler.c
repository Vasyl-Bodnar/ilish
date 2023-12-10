#include "compiler.h"
#include "env.h"
#include "errs.h"
#include "expr.h"
#include "exprs.h"
#include "strs.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>

compiler_t *create_compiler() {
  compiler_t *compiler = malloc(sizeof(*compiler));
  compiler->input = NULL;
  compiler->label = 0;
  compiler->env = NULL;
  compiler->output = NULL;
  compiler->errs = NULL;
  compiler->src = NULL;
  return compiler;
}

void delete_compiler(compiler_t *compiler) {
  if (compiler->input)
    delete_exprs(compiler->input);
  if (compiler->env)
    delete_env(compiler->env);
  if (compiler->errs)
    delete_errs(compiler->errs);
  free(compiler);
}

int has_errc(compiler_t *compiler) { return compiler->errs->len > 0; }

void errc(compiler_t *compiler, enum err err) {
  push_errs(compiler->errs,
            (err_t){.type = err, .line = compiler->line, .loc = compiler->loc});
}

void emit(compiler_t *compiler, char *phrase) {
  push_strs(compiler->output, phrase);
}

char *ssize_into_str(const char *str, ssize_t num) {
  int size = snprintf(NULL, 0, str, num); // "Free" size check
  char *phrase = malloc(sizeof(*phrase) * (size + 1));
  snprintf(phrase, size + 1, str, num);
  return phrase;
}

char *size_into_str(const char *str, size_t num) {
  int size = snprintf(NULL, 0, str, num); // "Free" size check
  char *phrase = malloc(sizeof(*phrase) * (size + 1));
  snprintf(phrase, size + 1, str, num);
  return phrase;
}

char *var_into_str(const char *str, size_t index) {
  char *reg;
  switch (index) {
  case 0:
    reg = "%rdi";
    break;
  case 1:
    reg = "%rsi";
    break;
  case 2:
    reg = "%rdx";
    break;
  case 3:
    reg = "%rcx";
    break;
  case 4:
    reg = "%r8";
    break;
  case 5:
    reg = "%r9";
    break;
  default:
    reg = size_into_str("-%zu(%%rsp)", (index - 5) * 8);
    int size = snprintf(NULL, 0, str, reg); // "Free" size check
    char *phrase = malloc(sizeof(*phrase) * (size + 1));
    snprintf(phrase, size + 1, str, reg);
    free(reg);
    return phrase;
  }
  int size = snprintf(NULL, 0, str, reg); // "Free" size check
  char *phrase = malloc(sizeof(*phrase) * (size + 1));
  snprintf(phrase, size + 1, str, reg);
  return phrase;
}

void emit_expr(compiler_t *compiler, expr_t expr);
void emit_load_expr(compiler_t *compiler, expr_t expr, size_t index);

/// Action is `strduped`, no need to escape %
void emit_unary(compiler_t *compiler, const char *action, exprs_t args) {
  if (args.len == 1) {
    emit_expr(compiler, args.arr[0]);
    emit(compiler, strdup(action));
  } else {
    errc(compiler, ExpectedUnary);
  }
}

/// Action is in `snprintf`.
/// % must be escaped for registers and used for size_t input
void emit_binary(compiler_t *compiler, const char *action, exprs_t args) {
  if (args.len == 2) {
    ssize_t found = find_unused_null_env(compiler->env);
    if (found == -1) {
      push_env(compiler->env, NULL);
      found = compiler->env->len - 1;
    }
    compiler->env->arr[found].used = 1;
    emit_load_expr(compiler, args.arr[1], found);
    emit_expr(compiler, args.arr[0]);
    emit(compiler, var_into_str(action, found));
    compiler->env->arr[found].used = 0;
    if (found == ((ssize_t)compiler->env->len) - 1) {
      pop_env(compiler->env);
    }
  } else {
    errc(compiler, ExpectedBinary);
  }
}

int emit_load_bind(compiler_t *compiler, expr_t bind, size_t index) {
  int err_code;
  if (bind.type == List) {
    if (bind.exprs->len == 2) {
      switch (bind.exprs->arr[0].type) {
      case SymbSpan:
        //span_to_str(&bind.exprs->arr[0], compiler->src);
        /* Fallthrough */
      case Symb:
        emit_load_expr(compiler, bind.exprs->arr[1], index);
        return 1;
      default:
        err_code = ExpectedSymb;
        break;
      }
    } else {
      err_code = ExpectedBinary;
    }
  }
  err_code = ExpectedList;
  compiler->line = bind.line;
  compiler->loc = bind.loc;
  errc(compiler, err_code);
  return 0;
}

void emit_letstar(compiler_t *compiler, exprs_t rest) {
  if (rest.len > 1) {
    exprs_t *binds = rest.arr[0].exprs;
    for (size_t i = 0; i < binds->len; i++) {
      if (emit_load_bind(compiler, binds->arr[i], compiler->env->len)) {
        push_env(compiler->env, strdup(binds->arr[i].exprs->arr[0].str));
      } else {
        return;
      }
    }
    for (size_t i = 1; i < rest.len; i++) {
      emit_expr(compiler, rest.arr[i]);
    }
    popn_env(compiler->env, binds->len);
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedAtLeastBinary);
  }
}

void emit_let(compiler_t *compiler, exprs_t rest) {
  if (rest.len > 1) {
    exprs_t *binds = rest.arr[0].exprs;
    for (size_t i = 0; i < binds->len; i++) {
      if (!emit_load_bind(compiler, binds->arr[i], compiler->env->len + i)) {
        return;
      }
    }
    for (size_t i = 0; i < binds->len; i++) {
      push_env(compiler->env, strdup(binds->arr[i].exprs->arr[0].str));
    }
    for (size_t i = 1; i < rest.len; i++) {
      emit_expr(compiler, rest.arr[i]);
    }
    popn_env(compiler->env, binds->len);
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedAtLeastBinary);
  }
}

void emit_if(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 3) {
    size_t l0 = compiler->label++;
    size_t l1 = compiler->label++;
    emit_expr(compiler, rest.arr[0]); // Test
    emit(compiler, size_into_str("cmpq $0, %rax\nje L%zu", l0));
    emit_expr(compiler, rest.arr[1]);
    emit(compiler, size_into_str("jmp L%zu", l1));
    emit(compiler, size_into_str("L%zu:", l0));
    emit_expr(compiler, rest.arr[2]);
    emit(compiler, size_into_str("L%zu:", l1));
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedTrinary);
  }
}

void emit_fun(compiler_t *compiler, expr_t first, exprs_t rest) {
  switch (first.type) {
  case SymbSpan:
    //span_to_str(&first, compiler->src);
    /* Fallthrough */
  case Symb:
    if (!strcmp(first.str, "1+")) {
      emit_unary(compiler, "incq %rax", rest);
    } else if (!strcmp(first.str, "1-")) {
      emit_unary(compiler, "decq %rax", rest);
    } else if (!strcmp(first.str, "zero?")) {
      emit_unary(compiler, "cmpq $0, %rax\nmovl $0, %eax\nsete %al", rest);
    } else if (!strcmp(first.str, "+")) {
      emit_binary(compiler, "addq %s, %%rax", rest);
    } else if (!strcmp(first.str, "-")) {
      emit_binary(compiler, "subq %s, %%rax", rest);
    } else if (!strcmp(first.str, "*")) {
      emit_binary(compiler, "imulq %s, %%rax", rest);
    } else if (!strcmp(first.str, "/")) {
      emit_binary(compiler, "divq %s, %%rax", rest);
    } else if (!strcmp(first.str, "=")) {
      emit_binary(compiler, "cmpq %s, %%rax\nmovl $0, %%eax\nsete %%al", rest);
    } else if (!strcmp(first.str, ">")) {
      emit_binary(compiler, "cmpq %s, %%rax\nmovl $0, %%eax\nsetg %%al", rest);
    } else if (!strcmp(first.str, ">=")) {
      emit_binary(compiler, "cmpq %s, %%rax\nmovl $0, %%eax\nsetge %%al", rest);
    } else if (!strcmp(first.str, "<")) {
      emit_binary(compiler, "cmpq %s, %%rax\nmovl $0, %%eax\nsetl %%al", rest);
    } else if (!strcmp(first.str, "<=")) {
      emit_binary(compiler, "cmpq %s, %%rax\nmovl $0, %%eax\nsetle %%al", rest);
    } else if (!strcmp(first.str, "let")) {
      emit_let(compiler, rest);
    } else if (!strcmp(first.str, "let*")) {
      emit_letstar(compiler, rest);
    } else if (!strcmp(first.str, "if")) {
      emit_if(compiler, rest);
    } else {
      errc(compiler, UnmatchedFun);
    }
    break;
  default:
    errc(compiler, ExpectedFunSymb);
    break;
  }
}

void emit_load_expr(compiler_t *compiler, expr_t expr, size_t index) {
  compiler->line = expr.line;
  compiler->loc = expr.loc;
  switch (expr.type) {
  case Num: {
    char *numbered = ssize_into_str("movq $%zd, %%s", expr.num);
    emit(compiler, var_into_str(numbered, index));
    free(numbered);
    break;
  }
  case Err:
    errc(compiler, ParserFailure);
    break;
  case Char:
  case StrSpan:
  case Str:
    // WIP
    break;
  case SymbSpan:
    //span_to_str(&expr, compiler->src);
    /* Fallthrough */
  case Symb: {
    int found = find_env(compiler->env, expr.str);
    if (found == -1) {
      errc(compiler, UndefinedSymb);
    } else {
      char *symbolized = var_into_str("movq %s, %%s", found);
      emit(compiler, var_into_str(symbolized, index));
      free(symbolized);
    }
    break;
  }
  case List:
    emit_fun(compiler, expr.exprs->arr[0], slice_start_exprs(expr.exprs, 1));
    emit(compiler, var_into_str("movq %%rax, %s", index));
    break;
  }
}

void emit_expr(compiler_t *compiler, expr_t expr) {
  switch (expr.type) {
  case Num:
    emit(compiler, ssize_into_str("movq $%zd, %%rax", expr.num));
    break;
  case Err:
    errc(compiler, ParserFailure);
    break;
  case Char:
  case StrSpan:
  case Str:
    // WIP
    break;
  case SymbSpan:
    //span_to_str(&expr, compiler->src);
    /* Fallthrough */
  case Symb: {
    int found = find_env(compiler->env, expr.str);
    if (found == -1) {
      errc(compiler, UndefinedSymb);
    } else {
      emit(compiler, var_into_str("movq %s, %%rax", found));
    }
    break;
  }
  case List:
    emit_fun(compiler, expr.exprs->arr[0], slice_start_exprs(expr.exprs, 1));
    break;
  }
}

void emit_exprs(compiler_t *compiler) {
  for (size_t i = 0; i < compiler->input->len; i++) {
    compiler->line = compiler->input->arr[i].line;
    compiler->loc = compiler->input->arr[i].loc;
    emit_expr(compiler, compiler->input->arr[i]);
  }
}

void emit_main(compiler_t *compiler) {
  if (compiler->env->req > 6) {
    compiler->output->arr[0] = size_into_str(
        ".global main\nmain:\nsubq $%zu, %%rsp", (compiler->env->req - 6) * 8);
    // ...
    emit(compiler,
         size_into_str("addq $%zu, %%rsp\nretq", (compiler->env->req - 6) * 8));
  } else {
    compiler->output->arr[0] = strdup(".global main\nmain:");
    emit(compiler, strdup("retq"));
  }
}

strs_t *compile(compiler_t *compiler, exprs_t *exprs, const char *src) {
  if (compiler->input)
    delete_exprs(compiler->input);
  if (compiler->env)
    delete_env(compiler->env);
  if (compiler->errs)
    delete_errs(compiler->errs);

  compiler->input = exprs;
  compiler->label = 0;
  compiler->env = create_env(2);
  compiler->output = create_strs(8); // Fixed but arbitrary
  compiler->src = src;
  compiler->errs = create_errs(3);

  // asm emit
  compiler->output->len = 1; // account for main
  emit_exprs(compiler);
  emit_main(compiler);
  return compiler->output;
}
