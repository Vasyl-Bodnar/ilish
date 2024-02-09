#include "compiler.h"
#include "env.h"
#include "errs.h"
#include "expr.h"
#include "exprs.h"
#include "strs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

compiler_t *create_compiler() {
  compiler_t *compiler = malloc(sizeof(*compiler));
  compiler->input = 0;
  compiler->heap = 0;
  compiler->heap_size = 0;
  compiler->label = 0;
  compiler->ret_type = None;
  compiler->free = 0;
  compiler->env = create_env(8);
  compiler->fun = create_strs(4);
  push_strs(compiler->fun, strdup(".data\n.text\n.global main"));
  compiler->body = create_strs(8);
  compiler->main = create_strs(6);
  compiler->errs = create_errs(3);
  compiler->src = 0;
  return compiler;
}

void delete_compiler(compiler_t *compiler) {
  if (compiler->input)
    delete_exprs(compiler->input);
  if (compiler->env)
    delete_env(compiler->env);
  if (compiler->fun)
    delete_strs(compiler->fun);
  if (compiler->body)
    delete_strs(compiler->body);
  if (compiler->main)
    delete_strs(compiler->main);
  if (compiler->errs)
    delete_errs(compiler->errs);
  free(compiler);
}

void emit_expr(compiler_t *compiler, expr_t expr);
void emit_load_expr(compiler_t *compiler, expr_t expr, size_t index);
void collect(compiler_t *compiler, size_t request);
void collect_ret(compiler_t *compiler, size_t extra);
size_t spill_args(compiler_t *compiler, size_t count);
void reorganize_args(compiler_t *compiler, size_t count);

int has_errc(compiler_t *compiler) { return compiler->errs->len; }

void errc(compiler_t *compiler, enum err err) {
  push_errs(compiler->errs,
            (err_t){.type = err, .line = compiler->line, .loc = compiler->loc});
}

void emit_func(compiler_t *compiler, char *phrase) {
  push_strs(compiler->fun, phrase);
}

void emit(compiler_t *compiler, char *phrase) {
  if (compiler->free) {
    push_strs(compiler->fun, phrase);
  } else {
    push_strs(compiler->body, phrase);
  }
}

void emit_main(compiler_t *compiler, char *phrase) {
  push_strs(compiler->main, phrase);
}

ssize_t tag_fixnum(ssize_t num) { return num << 2; }
ssize_t tag_char(ssize_t ch) { return (ch << 8) | 0x0f; }
ssize_t tag_bool(ssize_t bool) { return (bool << 7) | 0x1f; }
ssize_t tag_nil() { return 0x2f; }

char *ssize_into_str(const char *str, ssize_t num) {
  int size = snprintf(0, 0, str, num); // "Free" size check
  char *phrase = malloc(sizeof(*phrase) * (size + 1));
  snprintf(phrase, size + 1, str, num);
  return phrase;
}

char *size_into_str(const char *str, size_t num) {
  int size = snprintf(0, 0, str, num); // "Free" size check
  char *phrase = malloc(sizeof(*phrase) * (size + 1));
  snprintf(phrase, size + 1, str, num);
  return phrase;
}

char *duo_size_into_str(const char *str, size_t num) {
  int size = snprintf(0, 0, str, num, num); // "Free" size check
  char *phrase = malloc(sizeof(*phrase) * (size + 1));
  snprintf(phrase, size + 1, str, num, num);
  return phrase;
}

char *str_into_str(const char *str, const char *s) {
  int size = snprintf(0, 0, str, s); // "Free" size check
  char *phrase = malloc(sizeof(*phrase) * (size + 1));
  snprintf(phrase, size + 1, str, s);
  return phrase;
}

char *var32_into_str(const char *str, size_t index) {
  char *reg;
  switch (index) {
  case 0:
    reg = "%edi";
    break;
  case 1:
    reg = "%esi";
    break;
  case 2:
    reg = "%edx";
    break;
  case 3:
    reg = "%ecx";
    break;
  case 4:
    reg = "%r8l";
    break;
  case 5:
    reg = "%r9l";
    break;
  default:
    reg = size_into_str("-%zu(%%rsp)", (index - 5) * 8);
    int size = snprintf(0, 0, str, reg);
    char *phrase = malloc(sizeof(*phrase) * (size + 1));
    snprintf(phrase, size + 1, str, reg);
    free(reg);
    return phrase;
  }
  int size = snprintf(0, 0, str, reg);
  char *phrase = malloc(sizeof(*phrase) * (size + 1));
  snprintf(phrase, size + 1, str, reg);
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
    int size = snprintf(0, 0, str, reg);
    char *phrase = malloc(sizeof(*phrase) * (size + 1));
    snprintf(phrase, size + 1, str, reg);
    free(reg);
    return phrase;
  }
  int size = snprintf(0, 0, str, reg);
  char *phrase = malloc(sizeof(*phrase) * (size + 1));
  snprintf(phrase, size + 1, str, reg);
  return phrase;
}

char *duo_var_into_str(const char *str, size_t index) {
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
    int size = snprintf(0, 0, str, reg, reg);
    char *phrase = malloc(sizeof(*phrase) * (size + 1));
    snprintf(phrase, size + 1, str, reg, reg);
    free(reg);
    return phrase;
  }
  int size = snprintf(0, 0, str, reg, reg);
  char *phrase = malloc(sizeof(*phrase) * (size + 1));
  snprintf(phrase, size + 1, str, reg, reg);
  return phrase;
}

char *str_var_into_str(const char *str, const char *s, size_t index) {
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
    int size = snprintf(0, 0, str, s, reg);
    char *phrase = malloc(sizeof(*phrase) * (size + 1));
    snprintf(phrase, size + 1, str, s, reg);
    free(reg);
    return phrase;
  }
  int size = snprintf(0, 0, str, s, reg);
  char *phrase = malloc(sizeof(*phrase) * (size + 1));
  snprintf(phrase, size + 1, str, s, reg);
  return phrase;
}

char *size_var_into_str(const char *str, size_t num, size_t index) {
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
    int size = snprintf(0, 0, str, num, reg);
    char *phrase = malloc(sizeof(*phrase) * (size + 1));
    snprintf(phrase, size + 1, str, num, reg);
    free(reg);
    return phrase;
  }
  int size = snprintf(0, 0, str, num, reg);
  char *phrase = malloc(sizeof(*phrase) * (size + 1));
  snprintf(phrase, size + 1, str, num, reg);
  return phrase;
}

char *var_size_into_str(const char *str, size_t index, size_t num) {
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
    int size = snprintf(0, 0, str, reg, num);
    char *phrase = malloc(sizeof(*phrase) * (size + 1));
    snprintf(phrase, size + 1, str, reg, num);
    free(reg);
    return phrase;
  }
  int size = snprintf(0, 0, str, reg, num);
  char *phrase = malloc(sizeof(*phrase) * (size + 1));
  snprintf(phrase, size + 1, str, reg, num);
  return phrase;
}

char *two_var_into_str(const char *str, size_t index1, size_t index2) {
  char *reg1;
  switch (index1) {
  case 0:
    reg1 = "%rdi";
    break;
  case 1:
    reg1 = "%rsi";
    break;
  case 2:
    reg1 = "%rdx";
    break;
  case 3:
    reg1 = "%rcx";
    break;
  case 4:
    reg1 = "%r8";
    break;
  case 5:
    reg1 = "%r9";
    break;
  default:
    reg1 = size_into_str("-%zu(%%rsp)", (index1 - 5) * 8);
  }
  char *reg2;
  switch (index2) {
  case 0:
    reg2 = "%rdi";
    break;
  case 1:
    reg2 = "%rsi";
    break;
  case 2:
    reg2 = "%rdx";
    break;
  case 3:
    reg2 = "%rcx";
    break;
  case 4:
    reg2 = "%r8";
    break;
  case 5:
    reg2 = "%r9";
    break;
  default:
    reg2 = size_into_str("-%zu(%%rsp)", (index2 - 5) * 8);
  }
  int size = snprintf(0, 0, str, reg1, reg2);
  char *phrase = malloc(sizeof(*phrase) * (size + 1));
  snprintf(phrase, size + 1, str, reg1, reg2);
  if (index1 > 5)
    free(reg1);
  if (index2 > 5)
    free(reg2);
  return phrase;
}

/// Action is `strduped`, no need to escape %
void emit_unary(compiler_t *compiler, const char *action, exprs_t args) {
  if (args.len == 1) {
    emit_expr(compiler, args.arr[0]);
    emit(compiler, strdup(action));
  } else {
    errc(compiler, ExpectedUnary);
  }
}

void emit_begin(compiler_t *compiler, exprs_t args) {
  if (args.len) {
    for (size_t i = 0; i < args.len; i++) {
      emit_expr(compiler, args.arr[i]);
    }
  } else {
    errc(compiler, ExpectedAtLeastUnary);
  }
}

/// Action is in `snprintf`.
/// % must be escaped for registers and used for size_t input
void emit_binary(compiler_t *compiler, const char *action, exprs_t args) {
  if (args.len == 2) {
    size_t arg1 = get_unused_env(compiler->env);
    emit_load_expr(compiler, args.arr[1], arg1);
    emit_expr(compiler, args.arr[0]);
    emit(compiler, var_into_str(action, arg1));
    pop_or_remove_env(compiler->env, arg1);
  } else {
    errc(compiler, ExpectedBinary);
  }
}

/// Specialized emit_binary for modulo
void emit_mod(compiler_t *compiler, exprs_t args) {
  if (args.len == 2) {
    size_t arg1 = get_unused_env(compiler->env);
    emit_load_expr(compiler, args.arr[1], arg1);
    emit_expr(compiler, args.arr[0]);
    if (compiler->env->arr[2].used) {
      size_t tmp = get_unused_env(compiler->env);
      emit(compiler, var_into_str("movq %%rdx, %s\ncqto", tmp));
      emit(compiler,
           two_var_into_str("idivq %s\nmovq %%rdx, %%rax\nmovq %s, %%rdx", arg1,
                            tmp));
      pop_or_remove_env(compiler->env, tmp);
    } else {
      emit(compiler, var_into_str("cqto\nidivq %s\nmovq %%rdx, %%rax", arg1));
    }
    pop_or_remove_env(compiler->env, arg1);
  } else {
    errc(compiler, ExpectedBinary);
  }
}

/// Optional action, put null if you want to kill it
void emit_quest(compiler_t *compiler, const char *action, size_t constant,
                exprs_t args) {
  if (args.len == 1) {
    emit_expr(compiler, args.arr[0]);
    if (action) {
      emit(compiler, strdup(action));
    }
    emit(compiler,
         size_into_str("cmpq $%zu, %%rax\nmovl $0, %%eax\nsete %%al\nshll "
                       "$7, %%eax\norl $67, %%eax",
                       constant));
  } else {
    errc(compiler, ExpectedUnary);
  }
}

int emit_load_bind(compiler_t *compiler, expr_t bind, size_t index) {
  int err_code;
  if (bind.type == List) {
    if (bind.exprs->len == 2) {
      switch (bind.exprs->arr[0].type) {
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
    ssize_t *all_found = malloc(sizeof(*all_found) * binds->len);
    for (size_t i = 0; i < binds->len; i++) {
      size_t found = get_unused_env(compiler->env);
      all_found[i] = found;
      if (!emit_load_bind(compiler, binds->arr[i], found)) {
        return;
      }
      compiler->env->arr[found].str = strdup(binds->arr[i].exprs->arr[0].str);
      if (compiler->ret_type == Lamb) {
        compiler->env->arr[all_found[i]].points = 2;
      } else {
        compiler->env->arr[all_found[i]].points =
            compiler->ret_type == Cons || compiler->ret_type == Vector;
      }
    }
    for (size_t i = 1; i < rest.len; i++) {
      emit_expr(compiler, rest.arr[i]);
    }
    for (size_t i = 0; i < binds->len; i++) {
      pop_or_remove_env(compiler->env, all_found[i]);
    }
    free(all_found);
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedAtLeastBinary);
  }
}

void emit_let(compiler_t *compiler, exprs_t rest) {
  if (rest.len > 1) {
    exprs_t *binds = rest.arr[0].exprs;
    ssize_t *all_found = malloc(sizeof(*all_found) * binds->len);
    for (size_t i = 0; i < binds->len; i++) {
      size_t found = get_unused_env(compiler->env);
      all_found[i] = found;
      if (!emit_load_bind(compiler, binds->arr[i], found)) {
        return;
      }
    }
    for (size_t i = 0; i < binds->len; i++) {
      compiler->env->arr[all_found[i]].str =
          strdup(binds->arr[i].exprs->arr[0].str);
      if (compiler->ret_type == Lamb) {
        compiler->env->arr[all_found[i]].points = 2;
      } else {
        compiler->env->arr[all_found[i]].points =
            compiler->ret_type == Cons || compiler->ret_type == Vector;
      }
    }
    for (size_t i = 1; i < rest.len; i++) {
      emit_expr(compiler, rest.arr[i]);
    }
    for (size_t i = 0; i < binds->len; i++) {
      pop_or_remove_env(compiler->env, all_found[i]);
    }
    free(all_found);
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
    emit(compiler, size_into_str("cmpq $31, %rax\nje L%zu", l0));
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

void emit_cons(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 2) {
    collect(compiler, 16);
    size_t arg1 = get_unused_env(compiler->env);
    emit_load_expr(compiler, rest.arr[1], arg1);
    emit_expr(compiler, rest.arr[0]);
    emit(compiler, strdup("movq gen0_ptr(%rip), %r11\nmovq %rax, (%r11)"));
    emit(compiler,
         var_into_str("movq %s, 8(%%r11)\nmovq %%r11, %%rax\norq $1, %%rax",
                      arg1));
    emit(compiler, strdup("addq $16, gen0_ptr(%rip)"));
    remove_env(compiler->env, arg1);
    compiler->heap += 16;
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedBinary);
  }
}

// Possible Optimizations:
// - Consider the case of a fixnum in the first argument, generates less noise
void emit_mkvec(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 1) {
    emit_expr(compiler, rest.arr[0]);
    collect_ret(compiler, 8);
    size_t len = get_unused_env(compiler->env);
    emit_load_expr(compiler, rest.arr[0], len);
    emit(compiler,
         var_into_str("movq gen0_ptr(%%rip), %%r11\nmovq %s, (%%r11)", len));
    emit(compiler, strdup("movq %r11, %rax\norq $2, %rax"));
    emit(compiler, strdup("movq gen0_ptr(%rip), %r11"));
    emit(compiler, var_into_str("leaq 8(%%r11,%s,2), %%r11", len));
    emit(compiler, strdup("movq %r11, gen0_ptr(%rip)"));
    remove_env(compiler->env, len);
    compiler->heap += 8;
  } else if (rest.len == 2) {
    emit_expr(compiler, rest.arr[0]);
    collect_ret(compiler, 8);
    size_t label = compiler->label++;
    size_t len = get_unused_env(compiler->env);
    size_t counter = get_unused_env(compiler->env);
    emit_load_expr(compiler, rest.arr[0], len);
    emit_expr(compiler, rest.arr[1]);
    emit(compiler,
         var_into_str("movq gen0_ptr(%%rip), %%r11\nmovq %s, (%%r11)", len));
    emit(compiler, two_var_into_str("movq %s, %s", len, counter));
    emit(compiler, var_into_str("shr $2, %s", counter));
    emit(compiler, size_into_str("L%zu:", label));
    emit(compiler,
         duo_var_into_str("movq %%rax, (%%r11,%s,8)\ndecq %s", counter));
    emit(compiler, var_into_str("cmpq $0, %s", counter));
    emit(compiler, size_into_str("jne L%zu", label));
    emit(compiler, strdup("movq %r11, %rax\norq $2, %rax"));
    emit(compiler, strdup("movq gen0_ptr(%rip), %r11"));
    emit(compiler, var_into_str("leaq 8(%%r11,%s,2), %%r11", len));
    emit(compiler, strdup("movq %r11, gen0_ptr(%rip)"));
    remove_env(compiler->env, len);
    remove_env(compiler->env, counter);
    compiler->heap += 8;
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    if (!rest.len) {
      errc(compiler, ExpectedAtLeastUnary);
    } else {
      errc(compiler, ExpectedAtMostBinary);
    }
  }
}

void emit_vec(compiler_t *compiler, exprs_t args) {
  exprs_t *arg_len = create_exprs(1);
  push_exprs(arg_len, (expr_t){.type = Num, .num = args.len});
  emit_mkvec(compiler, *arg_len);
  size_t obj = get_unused_env(compiler->env);
  size_t loc = get_unused_env(compiler->env);
  for (size_t i = 0; i < args.len; i++) {
    emit_load_expr(compiler, args.arr[i], obj);
    emit(compiler, size_var_into_str("movq $%zu, %s", i << 3, loc));
    emit(compiler, two_var_into_str("movq %s, 6(%%rax,%s)", obj, loc));
  }
}

void emit_vecref(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 2) {
    size_t loc = get_unused_env(compiler->env);
    emit_load_expr(compiler, rest.arr[1], loc);
    emit_expr(compiler, rest.arr[0]);
    emit(compiler, var_into_str("movq 6(%%rax,%s,2), %%rax", loc));
    remove_env(compiler->env, loc);
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedBinary);
  }
}

void emit_vecset(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 3) {
    size_t obj = get_unused_env(compiler->env);
    emit_load_expr(compiler, rest.arr[2], obj);
    size_t loc = get_unused_env(compiler->env);
    emit_load_expr(compiler, rest.arr[1], loc);
    emit_expr(compiler, rest.arr[0]);
    emit(compiler, two_var_into_str("movq %s, 6(%%rax,%s,2)", obj, loc));
    remove_env(compiler->env, obj);
    remove_env(compiler->env, loc);
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedTrinary);
  }
}

void emit_cdrset(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 2) {
    size_t obj = get_unused_env(compiler->env);
    emit_load_expr(compiler, rest.arr[1], obj);
    emit_expr(compiler, rest.arr[0]);
    emit(compiler, var_into_str("movq %s, 7(%%rax)", obj));
    remove_env(compiler->env, obj);
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedBinary);
  }
}

void emit_carset(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 2) {
    size_t obj = get_unused_env(compiler->env);
    emit_load_expr(compiler, rest.arr[1], obj);
    emit_expr(compiler, rest.arr[0]);
    emit(compiler, var_into_str("movq %s, -1(%%rax)", obj));
    remove_env(compiler->env, obj);
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedBinary);
  }
}

void emit_set(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 2) {
    emit_expr(compiler, rest.arr[1]);
    ssize_t found = find_env(compiler->env, rest.arr[0].str);
    if (found == -1) {
      errc(compiler, UndefinedSymb);
    } else {
      emit(compiler, var_into_str("movq %rax, %s", found));
    }
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedBinary);
  }
}

void emit_closure(compiler_t *compiler, size_t lamb) {
  size_t free_len = compiler->free->len;
  size_t *free_vars = malloc(sizeof(*free_vars) * free_len);
  for (size_t i = 0; i < free_len; i++) {
    ssize_t found = find_env(compiler->env, compiler->free->arr[i]);
    if (found == -1) {
      errc(compiler, UndefinedSymb);
    } else {
      free_vars[i] = found;
    }
  }
  delete_strs(compiler->free);
  compiler->free = 0;
  collect(compiler, free_len * 8 + 16);
  emit(compiler,
       size_into_str("movq gen0_ptr(%%rip), %%r11\nmovq $%zu, (%%r11)",
                     free_len));
  size_t tmp = get_unused_env(compiler->env);
  emit(compiler, size_var_into_str("movq lambda%zu(%%rip), %s", lamb, tmp));
  emit(compiler, var_into_str("movq %s, 8(%%r11)", tmp));
  remove_env(compiler->env, tmp);
  for (size_t i = 0; i < free_len; i++) {
    emit(compiler,
         var_size_into_str("movq %s, %zu(%%r11)", free_vars[i], i * 8 + 16));
  }
  emit(compiler, strdup("movq %r11, %rax\norq $6, %rax"));
  emit(compiler, strdup("movq gen0_ptr(%rip), %r11"));
  emit(compiler, size_into_str("addq $%zu, %%r11", free_len * 8 + 16));
  emit(compiler, strdup("movq %r11, gen0_ptr(%rip)"));
  compiler->heap += free_len * 8 + 16;
  free(free_vars);
}

void emit_lambda(compiler_t *compiler, exprs_t rest) {
  int err_code = ExpectedAtLeastBinary;
  if (rest.len > 1) {
    if (rest.arr[0].type == List) {
      env_t *saved_env = compiler->env;
      strs_t *saved_free = compiler->free;
      compiler->env = create_env(6);
      compiler->free = create_strs(4);
      for (size_t i = 0; i < rest.arr[0].exprs->len; i++) {
        if (rest.arr[0].exprs->arr[i].type == Symb) {
          push_env(compiler->env, strdup(rest.arr[0].exprs->arr[i].str), 1,
                   0); // May be an issue
        } else {
          err_code = ExpectedSymb;
          break;
        }
      }
      emit_func(compiler, size_into_str("lambda%zu:", compiler->lambda));
      size_t lamb = compiler->lambda;
      compiler->lambda++;
      for (size_t i = 1; i < rest.len; i++) {
        emit_expr(compiler, rest.arr[i]);
      }
      emit_func(compiler, strdup("retq"));
      delete_env(compiler->env);
      compiler->env = saved_env;
      emit_closure(compiler, lamb);
      compiler->free = saved_free;
      return;
    } else {
      err_code = ExpectedList;
    }
  }
  compiler->line = rest.arr[0].line;
  compiler->loc = rest.arr[0].loc;
  errc(compiler, err_code);
}

void emit_call(compiler_t *compiler, const char *name, exprs_t rest) {
  size_t a_count = spill_args(compiler, rest.len);
  emit(compiler, str_into_str("callq %s(%%rip)", name));
  reorganize_args(compiler, a_count);
}

void emit_fun(compiler_t *compiler, expr_t first, exprs_t rest) {
  switch (first.type) {
  case Symb:
    switch (first.str[0]) {
    case '1':
      if (!strcmp(first.str, "1+")) {
        emit_unary(compiler, "addq $4, %rax", rest);
      } else if (!strcmp(first.str, "1-")) {
        emit_unary(compiler, "subq $4, %rax", rest);
      } else
        goto Unmatched;
      break;
    case '+':
      if (!strcmp(first.str, "+")) {
        emit_binary(compiler, "addq %s, %%rax", rest);
      } else
        goto Unmatched;
      break;
    case '-':
      if (!strcmp(first.str, "-")) {
        emit_binary(compiler, "subq %s, %%rax", rest);
      } else
        goto Unmatched;
      break;
    case '*':
      if (!strcmp(first.str, "*")) {
        emit_binary(compiler, "shr $2, %%rax\nimulq %s, %%rax", rest);
      } else
        goto Unmatched;
      break;
    case '/':
      if (!strcmp(first.str, "/")) {
        // Deal with the Devil
        emit_binary(compiler, "cqto\nidivq %s, %%rax\nshl $2, %%rax", rest);
      } else
        goto Unmatched;
      break;
    case '=':
      if (!strcmp(first.str, "=")) {
        emit_binary(compiler,
                    "cmpq %s, %%rax\nmovl $0, %%eax\nsete %%al\nshll $7, "
                    "%%eax\norl $31, %%eax",
                    rest);
        compiler->ret_type = Boolean;
      } else
        goto Unmatched;
      break;
    case '>':
      if (!strcmp(first.str, ">")) {
        emit_binary(compiler,
                    "cmpq %s, %%rax\nmovl $0, %%eax\nsetg %%al\nshll $7, "
                    "%%eax\norl $31, %%eax",
                    rest);
        compiler->ret_type = Boolean;
      } else if (!strcmp(first.str, ">=")) {
        emit_binary(compiler,
                    "cmpq %s, %%rax\nmovl $0, %%eax\nsetge %%al\nshll $7, "
                    "%%eax\norl $31, %%eax",
                    rest);
        compiler->ret_type = Boolean;
      } else
        goto Unmatched;
      break;
    case '<':
      if (!strcmp(first.str, "<")) {
        emit_binary(compiler,
                    "cmpq %s, %%rax\nmovl $0, %%eax\nsetl %%al\nshll $7, "
                    "%%eax\norl $31, %%eax",
                    rest);
        compiler->ret_type = Boolean;
      } else if (!strcmp(first.str, "<=")) {
        emit_binary(compiler,
                    "cmpq %s, %%rax\nmovl $0, %%eax\nsetle %%al\nshll $7, "
                    "%%eax\norl $31, %%eax",
                    rest);
        compiler->ret_type = Boolean;
      } else
        goto Unmatched;
      break;
    case 'b':
      if (!strcmp(first.str, "begin")) {
        emit_begin(compiler, rest);
      } else
        goto Unmatched;
      break;
    case 'c':
      if (!strcmp(first.str, "cons")) {
        emit_cons(compiler, rest);
        compiler->ret_type = Cons;
      } else if (!strcmp(first.str, "car")) {
        emit_unary(compiler, "movq -1(%rax), %rax", rest);
      } else if (!strcmp(first.str, "cdr")) {
        emit_unary(compiler, "movq 7(%rax), %rax", rest);
      } else if (!strcmp(first.str, "caar")) {
        emit_unary(compiler, "movq -1(%rax), %rax", rest);
        emit(compiler, strdup("movq -1(%rax), %rax"));
      } else if (!strcmp(first.str, "cadr")) {
        emit_unary(compiler, "movq 7(%rax), %rax", rest);
        emit(compiler, strdup("movq -1(%rax), %rax"));
      } else if (!strcmp(first.str, "cdar")) {
        emit_unary(compiler, "movq -1(%rax), %rax", rest);
        emit(compiler, strdup("movq 7(%rax), %rax"));
      } else if (!strcmp(first.str, "cddr")) {
        emit_unary(compiler, "movq 7(%rax), %rax", rest);
        emit(compiler, strdup("movq 7(%rax), %rax"));
      } else
        goto Unmatched;
      break;
    case 'e':
      if (!strcmp(first.str, "exit")) {
        emit(compiler, strdup("movq $0, %rdi\nmovq $60, %rax\nsyscall"));
      } else
        goto Unmatched;
      break;
    case 'i':
      if (!strcmp(first.str, "if")) {
        emit_if(compiler, rest);
      } else
        goto Unmatched;
      break;
    case 's':
      if (!strcmp(first.str, "set!")) {
        emit_set(compiler, rest);
      } else if (!strcmp(first.str, "set-car!")) {
        emit_carset(compiler, rest);
      } else if (!strcmp(first.str, "set-cdr!")) {
        emit_cdrset(compiler, rest);
      } else
        goto Unmatched;
      break;
    case 'p':
      if (!strcmp(first.str, "pair?")) {
        emit_quest(compiler, "andl $7, %eax", 1, rest);
        compiler->ret_type = Boolean;
      } else
        goto Unmatched;
      break;
    case 'm':
      if (!strcmp(first.str, "make-vector")) {
        emit_mkvec(compiler, rest);
        compiler->ret_type = Vector;
      } else if (!strcmp(first.str, "modulo")) {
        emit_mod(compiler, rest);
        compiler->ret_type = Fixnum;
      } else
        goto Unmatched;
      break;
    case 'n':
      if (!strcmp(first.str, "null?")) {
        emit_quest(compiler, "andl $7, %eax", 0, rest); // TODO
        compiler->ret_type = Boolean;
      } else
        goto Unmatched;
      break;
    case 'l':
      if (!strcmp(first.str, "lambda")) {
        emit_lambda(compiler, rest);
        compiler->ret_type = Lamb;
      } else if (!strcmp(first.str, "let")) {
        emit_let(compiler, rest);
      } else if (!strcmp(first.str, "let*")) {
        emit_letstar(compiler, rest);
      } else
        goto Unmatched;
      break;
    case 'v':
      if (!strcmp(first.str, "vector")) {
        emit_vec(compiler, rest);
      } else if (!strcmp(first.str, "vector?")) {
        emit_quest(compiler, "andl $7, %eax", 2, rest);
        compiler->ret_type = Boolean;
      } else if (!strcmp(first.str, "vector-length")) {
        emit_unary(compiler, "movq -2(%rax), %rax", rest);
        compiler->ret_type = Fixnum;
      } else if (!strcmp(first.str, "vector-ref")) {
        emit_vecref(compiler, rest);
      } else if (!strcmp(first.str, "vector-set!")) {
        emit_vecset(compiler, rest);
      } else
        goto Unmatched;
      break;
    case 'z':
      if (!strcmp(first.str, "zero?")) {
        emit_quest(compiler, 0, 0, rest);
        compiler->ret_type = Boolean;
      } else
        goto Unmatched;
      break;
    default:
    Unmatched : {
      ssize_t found = find_env(compiler->env, first.str);
      if (compiler->env->arr[found].points == 2) {
        emit_call(compiler, first.str, rest);
      } else {
        errc(compiler, UnmatchedFun);
      }
      break;
    }
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
  case Null: {
    char *nilled = ssize_into_str("movq $%zd, %%s", tag_nil());
    emit(compiler, var_into_str(nilled, index));
    free(nilled);
    break;
  }
  case Num: {
    char *numbered = ssize_into_str("movq $%zd, %%s", tag_fixnum(expr.num));
    emit(compiler, var_into_str(numbered, index));
    free(numbered);
    break;
  }
  case Chr: {
    char *charred = ssize_into_str("movq $%zd, %%s", tag_char(expr.ch));
    emit(compiler, var_into_str(charred, index));
    free(charred);
    break;
  }
  case Bool: {
    char *booled = ssize_into_str("movq $%zd, %%s", tag_bool(expr.ch));
    emit(compiler, var_into_str(booled, index));
    free(booled);
    break;
  }
  case Str:
    // WIP
    break;
  case Symb: {
    int found = find_env(compiler->env, expr.str);
    if (found == -1) {
      if (compiler->free) {
        push_strs(compiler->free, expr.str);
      } else {
        errc(compiler, UndefinedSymb);
      }
    } else {
      emit(compiler, two_var_into_str("movq %s, %s", found, index));
    }
    break;
  }
  case List:
    emit_fun(compiler, expr.exprs->arr[0], slice_start_exprs(expr.exprs, 1));
    emit(compiler, var_into_str("movq %%rax, %s", index));
    break;
  case Vec:
    emit_vec(compiler, slice_start_exprs(expr.exprs, 0));
    emit(compiler, var_into_str("movq %%rax, %s", index));
    break;
  case Err:
    errc(compiler, ParserFailure);
    break;
  }
}

void emit_expr(compiler_t *compiler, expr_t expr) {
  switch (expr.type) {
  case Null:
    emit(compiler, ssize_into_str("movq $%zd, %%rax", tag_nil()));
    compiler->ret_type = Nil;
    break;
  case Num:
    emit(compiler, ssize_into_str("movq $%zd, %%rax", tag_fixnum(expr.num)));
    compiler->ret_type = Fixnum;
    break;
  case Bool:
    emit(compiler, ssize_into_str("movq $%zd, %%rax", tag_bool(expr.ch)));
    compiler->ret_type = Boolean;
    break;
  case Chr:
    emit(compiler, ssize_into_str("movq $%zd, %%rax", tag_char(expr.ch)));
    compiler->ret_type = Char;
    break;
  case Str:
    // WIP
    break;
  case Symb: {
    int found = find_env(compiler->env, expr.str);
    if (found == -1) {
      if (compiler->free) {
        push_strs(compiler->free, expr.str);
      } else {
        errc(compiler, UndefinedSymb);
      }
    } else {
      emit(compiler, var_into_str("movq %s, %%rax", found));
    }
    break;
  }
  case List:
    emit_fun(compiler, expr.exprs->arr[0], slice_start_exprs(expr.exprs, 1));
    break;
  case Vec:
    emit_vec(compiler, slice_start_exprs(expr.exprs, 0));
    break;
  case Err:
    errc(compiler, ParserFailure);
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

size_t spill_pointers(compiler_t *compiler) {
  size_t count = 0;
  for (size_t i = 0; i < compiler->env->len; i++) {
    if (compiler->env->arr[i].str && compiler->env->arr[i].used &&
        compiler->env->arr[i].points) {
      compiler->env->arr[i].root_spill = 1;
      emit(compiler, var_into_str("movq %s, (%%r15)\naddq $8, %%r15", i));
      count++;
    }
  }
  return count;
}

void reorganize_pointers(compiler_t *compiler, size_t count) {
  emit(compiler, strdup("movq rs_begin(%rip), %r15"));
  for (size_t i = 0, j = 0; j < count && i < compiler->env->len; i++) {
    if (compiler->env->arr[i].root_spill) {
      compiler->env->arr[i].root_spill = 0;
      char *numbered = size_into_str("movq %zu(%%%r15), %%s", i * 8);
      emit(compiler, var_into_str(numbered, i));
      free(numbered);
      j++;
    }
  }
}

size_t spill_args(compiler_t *compiler, size_t count) {
  size_t spilled = 0;
  for (size_t i = 0; i < count && i < compiler->env->len; i++) {
    if (compiler->env->arr[i].used && !compiler->env->arr[i].root_spill) {
      compiler->env->arr[i].arg_spill = 1;
      emit(compiler, var_into_str("pushq %s", i));
      spilled++;
    }
  }
  return spilled;
}

void reorganize_args(compiler_t *compiler, size_t count) {
  for (size_t i = compiler->env->len, j = 0; j < count && i > 0; i--) {
    if (compiler->env->arr[i - 1].arg_spill) {
      compiler->env->arr[i - 1].arg_spill = 0;
      emit(compiler, var_into_str("popq %s", i - 1));
      j++;
    }
  }
}

void collect(compiler_t *compiler, size_t request) {
  size_t p_count = spill_pointers(compiler);
  size_t a_count = spill_args(compiler, 6); // 6 is when stack begins
  emit(compiler,
       size_into_str("movq %%r15, %%rdi\nmovq $%zu, %%rsi\ncallq collect",
                     request));
  reorganize_args(compiler, a_count);
  reorganize_pointers(compiler, p_count);
}

void collect_ret(compiler_t *compiler, size_t extra) {
  size_t p_count = spill_pointers(compiler);
  size_t a_count = spill_args(compiler, 6); // 6 is when stack begins
  emit(compiler, strdup("pushq %rax"));
  emit(
      compiler,
      size_into_str(
          "movq %%r15, %%rdi\nleaq %zu(,%rax,2), %%rsi\ncallq collect", extra));
  emit(compiler, strdup("popq %rax"));
  reorganize_args(compiler, a_count);
  reorganize_pointers(compiler, p_count);
}

void emit_start_end(compiler_t *compiler) {
  emit_main(compiler, strdup("main:"));
  if (compiler->env->req > 6) {
    emit_main(compiler,
              size_into_str("subq $%zu, %%rsp", (compiler->env->req - 6) * 8));
  }
  if (compiler->heap) {
    emit_main(compiler,
              duo_size_into_str(
                  "movq $%zu, %%rdi\nmovq $%zu, %%rsi\ncallq init_gc\nmovq "
                  "rs_begin(%%rip), %%r15",
                  compiler->heap_size));
  }
  if (compiler->env->req > 6) {
    emit(compiler,
         size_into_str("addq $%zu, %%rsp", (compiler->env->req - 6) * 8));
  }
  emit(compiler, strdup("movq %rax, %rdi\ncallq print")); //\ncallq cleanup
}

strs_t *compile(compiler_t *compiler, exprs_t *exprs, size_t heap_size,
                const char *src) {
  // ensure no garbage
  if (compiler->input)
    delete_exprs(compiler->input);
  if (compiler->errs) {
    delete_errs(compiler->errs);
    compiler->errs = create_errs(3);
  }
  if (compiler->main) {
    delete_strs(compiler->main);
    compiler->main = create_strs(6);
  }

  // init
  compiler->input = exprs;
  compiler->heap_size = heap_size;
  compiler->src = src;

  // asm emit
  emit_exprs(compiler);
  emit_start_end(compiler);
  const strs_t *ingredients[3] = {compiler->fun, compiler->main,
                                  compiler->body};
  return union_strs(ingredients, 3);
}
