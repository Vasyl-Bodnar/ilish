#include "compiler.h"
#include "bitmat.h"
#include "dstrs.h"
#include "env.h"
#include "errs.h"
#include "expr.h"
#include "exprs.h"
#include "strs.h"
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define args(...) __VA_ARGS__

/// Mallocing automatically for snprintf with as many args as you want
#define mal_sprintf(str, name, args)                                           \
  int size = snprintf(0, 0, str, args);                                        \
  char *name = malloc(sizeof(*name) * (size + 1));                             \
  snprintf(name, size + 1, str, args);

compiler_t *create_compiler() {
  compiler_t *compiler = malloc(sizeof(*compiler));
  compiler->input = 0;
  compiler->line = 0;
  compiler->loc = 0;
  compiler->heap = 0;
  compiler->heap_size = 0;
  compiler->lambda = 0;
  compiler->label = 0;
  compiler->ret_type = None;
  compiler->ret_args = 0;
  compiler->free = 0;
  compiler->env = create_env(8, 3, 3, 0);
  compiler->fun = create_dstrs(4);
  push_dstrs(compiler->fun, strdup(".data\n.text\n.global main"));
  compiler->body = create_strs(8);
  compiler->main = create_strs(6);
  compiler->end = create_strs(2);
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
    delete_dstrs(compiler->fun);
  if (compiler->body)
    delete_strs(compiler->body);
  if (compiler->main)
    delete_strs(compiler->main);
  if (compiler->end)
    delete_strs(compiler->end);
  if (compiler->errs)
    delete_errs(compiler->errs);
  free(compiler);
}

void emit_expr(compiler_t *compiler, expr_t expr);
void emit_load_expr(compiler_t *compiler, expr_t expr, size_t index);
void collect(compiler_t *compiler, size_t request);
void collect_ret(compiler_t *compiler, size_t extra);
size_t spill_args(compiler_t *compiler);
void reorganize_args(compiler_t *compiler, size_t count);
void try_emit_tail_call(compiler_t *compiler, const char *name, exprs_t args,
                        expr_t last);

int has_errc(compiler_t *compiler) { return compiler->errs->len; }

void errc(compiler_t *compiler, enum err err) {
  push_errs(compiler->errs,
            (err_t){.type = err, .line = compiler->line, .loc = compiler->loc});
}

void emit_main(compiler_t *compiler, char *phrase) {
  push_strs(compiler->main, phrase);
}

void emit_end(compiler_t *compiler, char *phrase) {
  push_strs(compiler->end, phrase);
}

void emit_func(compiler_t *compiler, char *phrase) {
  push_dstrs(compiler->fun, phrase);
}

void emit(compiler_t *compiler, char *phrase) {
  if (compiler->free) {
    push_dstrs(compiler->fun, phrase);
  } else {
    push_strs(compiler->body, phrase);
  }
}

ssize_t tag_fixnum(ssize_t num) { return num << 2; }
size_t tag_char(size_t ch) { return (ch << 8) | 0x0f; }
size_t tag_bool(size_t bool) { return (bool << 7) | 0x1f; }
size_t tag_nil() { return 0x2f; }

enum reg {
  Rax = 0,
  Rdi, // Start of volatile and args (for compiler)
  Rsi,
  Rdx,
  Rcx,
  R8,
  R9, // End of args
  R10,
  R11, // End of volatile
  Rbx, // Start of non-volatile
  Rbp,
  R12, // End of non-volatile
  R13, // Reserved for closure environments
  R14, // Reserved for GC pointer
  R15, // Reserved for root stack (of pointers)
  Rsp, // Reserved for stack pointer
};

char *reg_to_str(enum reg reg) {
  switch (reg) {
  case Rax:
    return "%rax";
  case Rdi:
    return "%rdi";
  case Rsi:
    return "%rsi";
  case Rdx:
    return "%rdx";
  case Rcx:
    return "%rcx";
  case R8:
    return "%r8";
  case R9:
    return "%r9";
  case R10:
    return "%r10";
  case R11:
    return "%r11";
  case Rbx:
    return "%rbx";
  case Rbp:
    return "%rbp";
  case R12:
    return "%r12";
  case R13:
    return "%r13";
  case R14:
    return "%r14";
  case R15:
    return "%r15";
  case Rsp:
    return "%rsp";
  default:
    return "FAIL"; // Should not reach here
  }
}

// HACK: TEMPORARY
void emit_var_into_str(compiler_t *compiler, const char *str, size_t var) {
  mal_sprintf(str, phrase, args(reg_to_str(var + 1)));
  emit(compiler, phrase);
}

void emit_str(compiler_t *compiler, const char *str) {
  emit(compiler, strdup(str));
}

// NOTE: Potential generalization in case if too many specific instructions are
// needed with similar properties
void emit_genins_reg_reg(compiler_t *compiler, const char *ins, enum reg reg1,
                         enum reg reg2) {
  mal_sprintf("%s %s, %s", phrase,
              args(ins, reg_to_str(reg1), reg_to_str(reg2)));
  emit(compiler, phrase);
}

ssize_t calc_stack(ssize_t stack_loc) { return stack_loc * 8 - 8; }

void emit_movq_reg_reg(compiler_t *compiler, enum reg reg1, enum reg reg2) {
  if (reg1 != reg2) {
    mal_sprintf("movq %s, %s", phrase,
                args(reg_to_str(reg1), reg_to_str(reg2)));
    emit(compiler, phrase);
  }
}

void emit_movq_regmem_reg(compiler_t *compiler, ssize_t add, enum reg mem_reg,
                          enum reg reg) {
  if (add) {
    mal_sprintf("movq %zd(%s), %s", phrase,
                args(add, reg_to_str(mem_reg), reg_to_str(reg)));
    emit(compiler, phrase);
  } else {
    mal_sprintf("movq (%s), %s", phrase,
                args(reg_to_str(mem_reg), reg_to_str(reg)));
    emit(compiler, phrase);
  }
}

void emit_movq_var_reg(compiler_t *compiler, size_t var, enum reg reg2) {
  if (var >= compiler->env->stack) {
    emit_movq_regmem_reg(compiler, calc_stack(compiler->env->stack - var), Rsp,
                         reg2);
  } else {
    emit_movq_reg_reg(compiler, var + 1,
                      reg2); // rax does not contain any vars
  }
}

void emit_movq_reg_regmem(compiler_t *compiler, enum reg reg, ssize_t add,
                          enum reg mem_reg) {
  if (add) {
    mal_sprintf("movq %s, %zd(%s)", phrase,
                args(reg_to_str(reg), add, reg_to_str(mem_reg)));
    emit(compiler, phrase);
  } else {
    mal_sprintf("movq %s, (%s)", phrase,
                args(reg_to_str(reg), reg_to_str(mem_reg)));
    emit(compiler, phrase);
  }
}

void emit_movq_reg_var(compiler_t *compiler, enum reg reg, size_t var) {
  if (var >= compiler->env->stack) {
    emit_movq_reg_regmem(compiler, reg, calc_stack(compiler->env->stack - var),
                         Rsp);
  } else {
    emit_movq_reg_reg(compiler, reg, var + 1); // rax does not contain any vars
  }
}

void emit_movq_regmem_regmem(compiler_t *compiler, size_t add1,
                             enum reg mem_reg1, size_t add2,
                             enum reg mem_reg2) {
  if (mem_reg1 == mem_reg2 && add1 == add2) {
    return;
  }
  size_t tmp = get_unused_pren_env(compiler->env, compiler->env->res);
  if (tmp & 1) {
    size_t new = reassign_postn_env(compiler->env, tmp, compiler->env->nvol);
    emit_movq_reg_reg(compiler, (tmp >> 1) + 1, new + 1);
  }
  if (add1 && add2) {
    mal_sprintf("movq %zu(%s) %s\nmovq %s, %zu(%s)", phrase,
                args(add1, reg_to_str(mem_reg1), reg_to_str((tmp >> 1) + 1),
                     reg_to_str((tmp >> 1) + 1), add2, reg_to_str(mem_reg2)));
    emit(compiler, phrase);
  } else if (add1) {
    mal_sprintf("movq %zu(%s) %s\nmovq %s, (%s)", phrase,
                args(add1, reg_to_str(mem_reg1), reg_to_str((tmp >> 1) + 1),
                     reg_to_str((tmp >> 1) + 1), reg_to_str(mem_reg2)));
    emit(compiler, phrase);
  } else if (add2) {
    mal_sprintf("movq (%s) %s\nmovq %s, %zu(%s)", phrase,
                args(reg_to_str(mem_reg1), reg_to_str((tmp >> 1) + 1),
                     reg_to_str((tmp >> 1) + 1), add2, reg_to_str(mem_reg2)));
    emit(compiler, phrase);
  }
}

void emit_movq_var_regmem(compiler_t *compiler, size_t var, ssize_t add,
                          enum reg mem_reg) {
  if (var >= compiler->env->stack) {
    emit_movq_regmem_regmem(compiler, calc_stack(compiler->env->stack - var),
                            Rsp, add, mem_reg);
  } else {
    emit_movq_reg_regmem(compiler, var + 1, add, mem_reg);
  }
}

void emit_movq_regmem_var(compiler_t *compiler, size_t add, enum reg mem_reg,
                          size_t var) {
  if (var >= compiler->env->stack) {
    emit_movq_regmem_regmem(compiler, add, mem_reg,
                            calc_stack(compiler->env->stack - var), Rsp);
  } else {
    emit_movq_regmem_reg(compiler, add, mem_reg, var + 1);
  }
}

void emit_movq_var_var(compiler_t *compiler, size_t var1, size_t var2) {
  if (var1 >= compiler->env->stack && var2 >= compiler->env->stack) {
    emit_movq_regmem_regmem(compiler, calc_stack(compiler->env->stack - var1),
                            Rsp, calc_stack(compiler->env->stack - var2), Rsp);
  } else if (var1 >= compiler->env->stack) {
    emit_movq_regmem_reg(compiler, calc_stack(compiler->env->stack - var1), Rsp,
                         var2 + 1);
  } else if (var2 >= compiler->env->stack) {
    emit_movq_reg_regmem(compiler, var1 + 1,
                         calc_stack(compiler->env->stack - var2), Rsp);
  } else {
    emit_movq_reg_reg(compiler, var1 + 1, var2 + 1);
  }
}

void emit_movq_imm_reg(compiler_t *compiler, ssize_t imm, enum reg reg) {
  mal_sprintf("movq $%zd, %s", phrase, args(imm, reg_to_str(reg)));
  emit(compiler, phrase);
}

void emit_movq_imm_regmem(compiler_t *compiler, ssize_t imm, ssize_t add,
                          enum reg mem_reg) {
  if (add) {
    mal_sprintf("movq $%zd, %zd(%s)", phrase,
                args(imm, add, reg_to_str(mem_reg)));
    emit(compiler, phrase);
  } else {
    mal_sprintf("movq $%zd, (%s)", phrase, args(imm, reg_to_str(mem_reg)));
    emit(compiler, phrase);
  }
}

void emit_movq_imm_var(compiler_t *compiler, ssize_t imm, size_t var) {
  if (var >= compiler->env->stack &&
      compiler->env->stack > compiler->env->res) {
    emit_movq_imm_regmem(compiler, imm, calc_stack(compiler->env->stack - var),
                         Rsp);
  } else {
    emit_movq_imm_reg(compiler, imm,
                      var + 1); // rax does not contain any vars
  }
}

void emit_movq_reg_fullmem(compiler_t *compiler, enum reg reg, ssize_t add,
                           enum reg mem_reg, enum reg add_reg, int mult) {
  mal_sprintf("movq %s, %zd(%s, %s, %d)", phrase,
              args(reg_to_str(reg), add, reg_to_str(mem_reg),
                   reg_to_str(add_reg), mult));
  emit(compiler, phrase);
}

void emit_movq_regmem_fullmem(compiler_t *compiler, size_t add1,
                              enum reg mem_reg1, ssize_t add2,
                              enum reg mem_reg2, enum reg add_reg, int mult) {
  size_t tmp = get_unused_pren_env(compiler->env, compiler->env->res);
  if (tmp & 1) {
    size_t new = reassign_postn_env(compiler->env, tmp, compiler->env->nvol);
    emit_movq_reg_reg(compiler, (tmp >> 1) + 1, new + 1);
  }
  if (add1 && add2) {
    mal_sprintf("movq %zu(%s) %s\nmovq %s, %zu(%s, %s, %d)", phrase,
                args(add1, reg_to_str(mem_reg1), reg_to_str((tmp >> 1) + 1),
                     reg_to_str((tmp >> 1) + 1), add2, reg_to_str(mem_reg2),
                     reg_to_str(add_reg), mult));
    emit(compiler, phrase);
  } else if (add1) {
    mal_sprintf("movq %zu(%s) %s\nmovq %s, (%s, %s, %d)", phrase,
                args(add1, reg_to_str(mem_reg1), reg_to_str((tmp >> 1) + 1),
                     reg_to_str((tmp >> 1) + 1), reg_to_str(mem_reg2),
                     reg_to_str(add_reg), mult));
    emit(compiler, phrase);
  } else if (add2) {
    mal_sprintf("movq (%s) %s\nmovq %s, %zu(%s, %s, %d)", phrase,
                args(reg_to_str(mem_reg1), reg_to_str((tmp >> 1) + 1),
                     reg_to_str((tmp >> 1) + 1), add2, reg_to_str(mem_reg2),
                     reg_to_str(add_reg), mult));
    emit(compiler, phrase);
  }
}

void emit_movq_var_fullmem(compiler_t *compiler, size_t var, ssize_t add,
                           enum reg mem_reg, enum reg add_reg, int mult) {
  if (var >= compiler->env->stack) {
    emit_movq_regmem_fullmem(compiler, calc_stack(compiler->env->stack - var),
                             Rsp, add, mem_reg, add_reg, mult);
  } else {
    emit_movq_reg_fullmem(compiler, var + 1, add, mem_reg, add_reg, mult);
  }
}

void emit_leaq_label_reg(compiler_t *compiler, const char *label_name,
                         size_t label_num, enum reg reg) {
  mal_sprintf("leaq %s%zu(%%rip), %s", phrase,
              args(label_name, label_num, reg_to_str(reg)));
  emit(compiler, phrase);
}

void emit_leaq_label_regmem(compiler_t *compiler, const char *label_name,
                            size_t label_num, size_t add, enum reg mem_reg) {
  if (add) {
    mal_sprintf("leaq %s%zu(%%rip), %zu(%s)", phrase,
                args(label_name, label_num, add, reg_to_str(mem_reg)));
    emit(compiler, phrase);
  } else {
    mal_sprintf("leaq %s%zu(%%rip), (%s)", phrase,
                args(label_name, label_num, reg_to_str(mem_reg)));
    emit(compiler, phrase);
  }
}

void emit_leaq_label_var(compiler_t *compiler, const char *label_name,
                         size_t label_num, size_t var) {
  if (var >= compiler->env->stack &&
      compiler->env->stack > compiler->env->res) {
    emit_leaq_label_regmem(compiler, label_name, label_num,
                           calc_stack(compiler->env->stack - var), Rsp);
  } else {
    emit_leaq_label_reg(compiler, label_name, label_num, var + 1);
  }
}

void emit_decq_reg(compiler_t *compiler, enum reg reg) {
  mal_sprintf("decq %s", phrase, args(reg_to_str(reg)));
  emit(compiler, phrase);
}

void emit_decq_regmem(compiler_t *compiler, size_t add, enum reg mem_reg) {
  if (add) {
    mal_sprintf("decq %zu(%s)", phrase, args(add, reg_to_str(mem_reg)));
    emit(compiler, phrase);
  } else {
    mal_sprintf("decq (%s)", phrase, args(reg_to_str(mem_reg)));
    emit(compiler, phrase);
  }
}

void emit_decq_var(compiler_t *compiler, size_t var) {
  if (var >= compiler->env->stack &&
      compiler->env->stack > compiler->env->res) {
    emit_decq_regmem(compiler, calc_stack(compiler->env->stack - var), Rsp);
  } else {
    emit_decq_reg(compiler, var + 1);
  }
}

char *ssize_into_str(const char *str, ssize_t num) {
  mal_sprintf(str, phrase, args(num));
  return phrase;
}

char *size_into_str(const char *str, size_t num) {
  mal_sprintf(str, phrase, args(num));
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
    emit_var_into_str(compiler, action, arg1);
    remove_env(compiler->env, arg1);
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
      emit_movq_reg_var(compiler, Rdx, tmp);
      emit_str(compiler, "cqto");
      emit_var_into_str(compiler, "idivq %s", arg1);
      emit_movq_reg_reg(compiler, Rdx, Rax);
      emit_movq_var_reg(compiler, tmp, Rdx);
      remove_env(compiler->env, tmp);
    } else {
      emit_str(compiler, "cqto");
      emit_var_into_str(compiler, "idivq %s", arg1);
      emit_movq_reg_reg(compiler, Rdx, Rax);
    }
    remove_env(compiler->env, arg1);
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
                       "$7, %%eax\norl $31, %%eax",
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
      case Sym:
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
      compiler->env->arr[found].used =
          Unknown; // HACK: We know it is ret_type, but lambs need args, and I
                   // can't find them
    }
    for (size_t i = 1; i < rest.len; i++) {
      emit_expr(compiler, rest.arr[i]);
    }
    for (size_t i = 0; i < binds->len; i++) {
      remove_env(compiler->env, all_found[i]);
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
      compiler->env->arr[all_found[i]].used =
          Unknown; // HACK: We know its ret_type, but lambs need args, and I
                   // can't find them
    }
    for (size_t i = 1; i < rest.len; i++) {
      emit_expr(compiler, rest.arr[i]);
    }
    for (size_t i = 0; i < binds->len; i++) {
      remove_env(compiler->env, all_found[i]);
    }
    free(all_found);
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedAtLeastBinary);
  }
}
void emit_if(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 2) {
    size_t l0 = compiler->label++;
    emit_expr(compiler, rest.arr[0]); // Test
    emit(compiler, size_into_str("cmpq $31, %rax\nje L%zu", l0));
    emit_expr(compiler, rest.arr[1]);
    emit(compiler, size_into_str("L%zu:", l0));
  } else if (rest.len == 3) {
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
    errc(compiler, ExpectedAtLeastBinary);
  }
}

void emit_cons(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 2) {
    collect(compiler, 16);
    size_t arg1 = get_unused_env(compiler->env);
    emit_load_expr(compiler, rest.arr[1], arg1);
    emit_expr(compiler, rest.arr[0]);
    emit(compiler, strdup("movq gen0_ptr(%rip), %r14\nmovq %rax, (%r14)"));
    emit_movq_var_regmem(compiler, arg1, 8, R14);
    emit(compiler, strdup("movq %r14, %rax\norq $1, %rax"));
    emit(compiler, strdup("addq $16, gen0_ptr(%rip)"));
    remove_env(compiler->env, arg1);
    compiler->heap += 16;
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedBinary);
  }
}

// PERF: Consider the case of a fixnum in the first argument, generates less
// noise
void emit_mkvec(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 1) {
    emit_expr(compiler, rest.arr[0]);
    collect_ret(compiler, 8);
    size_t len = get_unused_env(compiler->env);
    emit_load_expr(compiler, rest.arr[0], len);
    emit_var_into_str(compiler, "movq gen0_ptr(%%rip), %%r14", len);
    emit_movq_var_regmem(compiler, len, 0, R14);
    emit(compiler, strdup("movq %r14, %rax\norq $2, %rax"));
    emit(compiler, strdup("movq gen0_ptr(%rip), %r14"));
    emit_var_into_str(compiler, "leaq 8(%%r14,%s,2), %%r14", len);
    emit(compiler, strdup("movq %r14, gen0_ptr(%rip)"));
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
    emit(compiler, "movq gen0_ptr(%%rip), %%r14");
    emit_movq_var_regmem(compiler, len, 0, R14);
    emit_movq_var_var(compiler, len, counter);
    emit_var_into_str(compiler, "shr $2, %s", counter);
    emit(compiler, size_into_str("L%zu:", label));
    emit_movq_reg_fullmem(compiler, Rax, 0, R14, counter + 1, 8);
    emit_decq_var(compiler, counter);
    emit_var_into_str(compiler, "cmpq $0, %s", counter);
    emit(compiler, size_into_str("jne L%zu", label));
    emit(compiler, strdup("movq %r14, %rax\norq $2, %rax"));
    emit(compiler, strdup("movq gen0_ptr(%rip), %r14"));
    emit_var_into_str(compiler, "leaq 8(%%r14,%s,2), %%r14", len);
    emit(compiler, strdup("movq %r14, gen0_ptr(%rip)"));
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
    emit_movq_imm_var(compiler, i << 3, loc);
    emit_movq_var_fullmem(compiler, obj, 6, Rax, loc + 1, 1);
  }
}

void emit_vecref(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 2) {
    size_t loc = get_unused_env(compiler->env);
    emit_load_expr(compiler, rest.arr[1], loc);
    emit_expr(compiler, rest.arr[0]);
    emit_var_into_str(compiler, "movq 6(%%rax,%s,2), %%rax", loc);
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
    emit_movq_var_fullmem(compiler, obj, 6, Rax, loc + 1, 2);
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
    emit_movq_reg_regmem(compiler, obj, 7, Rax);
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
    emit_movq_reg_regmem(compiler, obj, -1, Rax);
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
      emit_var_into_str(compiler, "movq %rax, %s", found);
    }
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedBinary);
  }
}

void solve_call_order(compiler_t *compiler, exprs_t args, exprs_t rest) {
  bitmat_t *bitmat = create_bitmat(rest.len, args.len);
  size_t *order = calloc(rest.len, sizeof(*order));
  // Setup
  for (size_t i = 0; i < rest.len; i++) {
    order[i] = i;
    for (size_t j = 0; j < args.len; j++) {
      set_bitmat(bitmat, j, i, check_symb_expr(rest.arr[j], args.arr[i].str));
    }
  }
  // Reorganize to Solve
  for (size_t i = 0; i < rest.len; i++) {
    size_t res = is_ef_pivot_col_bitmat(bitmat, order[i], i);
    if (res) {
      swap_row_bitmat(bitmat, order[i], order[res]);
      size_t tmp = order[i];
      order[i] = order[res];
      order[res] = tmp;
    }
  }
  // Resolve and Reap
  for (size_t i = 0; i < rest.len; i++) {
    if (is_row_unique_bitmat(bitmat, order[i])) {
      size_t new = reassign_postn_env(compiler->env, order[i], rest.len);
      emit_movq_var_var(compiler, order[i], new);
    }
    emit_load_expr(compiler, rest.arr[order[i]], order[i]);
  }
  free(order);
  delete_bitmat(bitmat);
}

void emit_closure(compiler_t *compiler, size_t lamb, size_t arity,
                  size_t *free_vars, size_t free_len) {
  collect(compiler, free_len * 8 + 16);
  emit(compiler,
       size_into_str("movq gen0_ptr(%%rip), %%r14\nmovq $%zu, (%%r14)", arity));
  size_t tmp = get_unused_env(compiler->env);
  emit_leaq_label_var(compiler, "lambda", lamb, tmp);
  emit_movq_var_regmem(compiler, tmp, 8, R14);
  remove_env(compiler->env, tmp);
  for (size_t i = 0; i < free_len; i++) {
    emit_movq_var_regmem(compiler, free_vars[i], i * 8 + 16, R14);
  }
  emit(compiler, strdup("movq %r14, %rax\norq $6, %rax"));
  emit(compiler, strdup("movq gen0_ptr(%rip), %r14"));
  emit(compiler, size_into_str("addq $%zu, %%r14", free_len * 8 + 16));
  emit(compiler, strdup("movq %r14, gen0_ptr(%rip)"));
  compiler->heap += free_len * 8 + 16;
}

void emit_tail_call(compiler_t *compiler, exprs_t args, exprs_t rest) {
  solve_call_order(compiler, args, rest);
  if (compiler->env->len > compiler->env->stack + 1) {
    emit_func(compiler,
              size_into_str("addq $%zu, %%rsp",
                            (compiler->env->len - compiler->env->stack) * 8));
  }
  emit(compiler, strdup("movq 2(%r13), %rax"));
  emit(compiler, strdup("jmp *%rax"));
}

void try_emit_tail_if(compiler_t *compiler, const char *name, exprs_t args,
                      exprs_t rest) {
  if (rest.len == 2) {
    size_t l0 = compiler->label++;
    emit_expr(compiler, rest.arr[0]); // Test
    emit(compiler, size_into_str("cmpq $31, %rax\nje L%zu", l0));
    try_emit_tail_call(compiler, name, args, rest.arr[1]);
    emit(compiler, size_into_str("L%zu:", l0));
  } else if (rest.len == 3) {
    size_t l0 = compiler->label++;
    size_t l1 = compiler->label++;
    emit_expr(compiler, rest.arr[0]); // Test
    emit(compiler, size_into_str("cmpq $31, %rax\nje L%zu", l0));
    try_emit_tail_call(compiler, name, args, rest.arr[1]);
    emit(compiler, size_into_str("jmp L%zu", l1));
    emit(compiler, size_into_str("L%zu:", l0));
    try_emit_tail_call(compiler, name, args, rest.arr[2]);
    emit(compiler, size_into_str("L%zu:", l1));
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedAtLeastBinary);
  }
}

void try_emit_tail_call(compiler_t *compiler, const char *name, exprs_t args,
                        expr_t last) {
  if (last.type == List) {
    if (last.exprs->arr[0].type == Sym) {
      if (!strcmp(last.exprs->arr[0].str, name)) {
        if (args.len == last.exprs->len - 1) {
          emit_tail_call(compiler, args, slice_start_exprs(last.exprs, 1));
          return;
        } else {
          errc(compiler, ExpectedNoArg + args.len);
          return;
        }
      } else if (!strcmp(last.exprs->arr[0].str, "if")) {
        try_emit_tail_if(compiler, name, args,
                         slice_start_exprs(last.exprs, 1));
        return;
      }
    }
  }
  emit_expr(compiler, last);
}

void emit_lambda(compiler_t *compiler, const char *name, exprs_t rest) {
  int err_code = ExpectedAtLeastBinary;
  if (rest.len > 1) {
    if (rest.arr[0].type == List) {
      env_t *saved_env = compiler->env;
      strs_t *saved_free = compiler->free;
      compiler->env = create_full_env(8, 3, 3, 0);
      compiler->free = create_strs(4);
      if (name) { // Recursion
        compiler->env->arr[R13 - 1].str = strdup(name);
        compiler->env->arr[R13 - 1].used = Lambda;
        compiler->env->arr[R13 - 1].args = clone_exprs(rest.arr[0].exprs);
      }
      for (size_t i = 0; i < rest.arr[0].exprs->len; i++) {
        if (rest.arr[0].exprs->arr[i].type == Sym) {
          size_t var = get_unused_env(compiler->env);
          compiler->env->arr[var].str = strdup(rest.arr[0].exprs->arr[i].str);
          compiler->env->arr[var].used = Unknown;
        } else {
          err_code = ExpectedSymb;
          goto LambErr;
        }
      }
      lock_dstrs(compiler->fun);
      emit_func(compiler, size_into_str("lambda%zu:", compiler->lambda));
      size_t lamb = compiler->lambda;
      compiler->lambda++;
      lock_dstrs(compiler->fun);
      if (name && rest.len > 1) {
        for (size_t i = 1; i < rest.len - 1; i++) {
          emit_expr(compiler, rest.arr[i]);
        }
        try_emit_tail_call(compiler, name, *rest.arr[0].exprs,
                           rest.arr[rest.len - 1]);
      } else {
        for (size_t i = 1; i < rest.len; i++) {
          emit_expr(compiler, rest.arr[i]);
        }
      }
      if (compiler->env->len > compiler->env->stack) {
        emit_func(
            compiler,
            size_into_str("addq $%zu, %%rsp",
                          (compiler->env->len - compiler->env->stack) * 8));
      }
      emit_func(compiler, strdup("retq"));
      unlock_dstrs(compiler->fun);
      if (compiler->env->len > compiler->env->stack + 1) {
        emit_func(
            compiler,
            size_into_str("subq $%zu, %%rsp",
                          (compiler->env->len - compiler->env->stack) * 8));
      }
      unlock_dstrs(compiler->fun);
      delete_env(compiler->env);
      compiler->env = saved_env;
      size_t free_len = compiler->free->len;
      size_t *free_vars = calloc(sizeof(*free_vars), free_len);
      for (size_t i = 0; i < free_len; i++) {
        ssize_t found = find_env(compiler->env, compiler->free->arr[i]);
        if (found == -1) {
          err_code = UndefinedSymb;
          goto LambErr;
        } else {
          free_vars[i] = found;
        }
      }
      delete_strs(compiler->free);
      compiler->free = saved_free;
      emit_closure(compiler, lamb, rest.arr[0].exprs->len, free_vars, free_len);
      compiler->ret_type = Lambda;
      if (rest.arr[0].exprs) {
        compiler->ret_args = clone_exprs(rest.arr[0].exprs);
      }
      free(free_vars);
      if (!compiler->fun->lock) {
        collapse_dstrs(compiler->fun);
      }
      return;
    } else {
      err_code = ExpectedList;
    }
  }
LambErr:
  compiler->line = rest.arr[0].line;
  compiler->loc = rest.arr[0].loc;
  errc(compiler, err_code);
}

void emit_define(compiler_t *compiler, exprs_t rest) {
  if (rest.arr[0].type == List) {
    if (rest.len >= 2) {
      exprs_t *tmp = create_exprs(2);
      exprs_t slice = slice_start_exprs(rest.arr[0].exprs, 1);
      push_exprs(tmp, (expr_t){.type = List, .exprs = &slice});
      for (size_t i = 1; i < rest.len; i++) {
        push_exprs(tmp, rest.arr[i]);
      }
      emit_lambda(compiler, rest.arr[0].exprs->arr[0].str, *tmp);
      compiler->ret_type = Lambda;
      free(tmp);
      ssize_t found = find_env(compiler->env, rest.arr[0].exprs->arr[0].str);
      if (found == -1) {
        found = get_unused_postn_env(compiler->env, 6);
      } else {
        remove_env(compiler->env, found);
      }
      emit_movq_reg_var(compiler, Rax, found);
      compiler->env->arr[found].str = strdup(rest.arr[0].exprs->arr[0].str);
      compiler->env->arr[found].used = compiler->ret_type;
      compiler->env->arr[found].args =
          slice_start_clone_exprs(rest.arr[0].exprs, 1);
    } else {
      compiler->line = rest.arr[0].line;
      compiler->loc = rest.arr[0].loc;
      errc(compiler, ExpectedAtLeastBinary);
    }
  } else {
    if (rest.len == 2) {
      ssize_t found = find_env(compiler->env, rest.arr[0].str);
      if (found == -1) {
        found = get_unused_postn_env(compiler->env, 6);
      } else {
        remove_env(compiler->env, found);
      }
      emit_load_bind(compiler, (expr_t){.type = List, .exprs = &rest}, found);
      compiler->env->arr[found].str = strdup(rest.arr[0].str);
    } else {
      compiler->line = rest.arr[0].line;
      compiler->loc = rest.arr[0].loc;
      errc(compiler, ExpectedBinary);
    }
  }
}

void emit_ufun(compiler_t *compiler, const char *str, exprs_t rest) {
  ssize_t found = find_env(compiler->env, str);
  if (found == -1 && compiler->free) {
    found = find_strs(compiler->free, str);
    if (found == -1) {
      found = compiler->free->len;
      push_strs(compiler->free, strdup(str));
    }
    // NOTE: This is the worst case. Gates have fallen.
    // We cannot tell if we are looking at a lamb or a number.
    if (found + 1 != R13) {
      emit_movq_var_reg(compiler, found, R13);
    }
    size_t l0 = compiler->label++;
    emit(compiler, size_into_str("movq %zu(%%r13), %%rax", found * 8 + 10));
    emit(compiler, strdup("pushq %r13\nmovq %rax, %r13"));
    emit(compiler, strdup("movq -6(%r13), %rax"));
    emit(compiler, size_into_str("cmpq $%zu, %%rax", rest.len));
    emit(compiler, size_into_str("jne L%zu", l0));
    size_t a_count = spill_args(compiler);
    // TODO: Try to order this if possible at all, since otherwise (f (n-1) n)
    // will always cause unexpected behaviour.
    // Worst case have to reassign and save all variables to guarantee proper
    // result.
    for (size_t i = 0; i < rest.len; i++) {
      size_t new = reassign_postn_env(compiler->env, i, rest.len);
      emit_movq_reg_reg(compiler, i + 1, new + 1);
      emit_load_expr(compiler, rest.arr[i], i);
    }
    emit(compiler, strdup("movq 2(%r13), %rax"));
    emit(compiler, strdup("callq *%rax"));
    reorganize_args(compiler, a_count);
    emit(compiler, size_into_str("L%zu:", l0));
    emit(compiler, strdup("popq %r13"));
  } else if (compiler->env->arr[found].used == Lambda) {
    if (compiler->env->arr[found].args->len == rest.len) {
      if (found + 1 != R13) {
        emit_movq_var_reg(compiler, found, R13);
      }
      size_t a_count = spill_args(compiler);
      solve_call_order(compiler, *compiler->env->arr[found].args, rest);
      emit(compiler, strdup("movq 2(%r13), %rax"));
      emit(compiler, strdup("callq *%rax"));
      reorganize_args(compiler, a_count);
    } else {
      errc(compiler, ExpectedNoArg + compiler->env->arr[found].args->len);
    }
  } else if (compiler->free) {
    emit(compiler, strdup("pushq %r13"));
    emit_movq_var_reg(compiler, found, R13);
    size_t a_count = spill_args(compiler);
    solve_call_order(compiler, *compiler->env->arr[found].args, rest);
    emit(compiler, strdup("movq 2(%r13), %rax"));
    emit(compiler, strdup("callq *%rax"));
    reorganize_args(compiler, a_count);
    emit(compiler, strdup("popq %r13"));
  } else {
    errc(compiler, UnmatchedFun);
  }
}

void emit_fun(compiler_t *compiler, expr_t first, exprs_t rest) {
  switch (first.type) {
  case Sym:
    switch (first.str[0]) {
    case '1':
      if (!strcmp(first.str, "1+")) {
        emit_unary(compiler, "addq $4, %rax", rest);
        compiler->ret_type = Unknown;
      } else if (!strcmp(first.str, "1-")) {
        emit_unary(compiler, "subq $4, %rax", rest);
        compiler->ret_type = Unknown;
      } else
        goto Unmatched;
      break;
    case '+':
      if (!strcmp(first.str, "+")) {
        emit_binary(compiler, "addq %s, %%rax", rest);
        compiler->ret_type = Unknown;
      } else
        goto Unmatched;
      break;
    case '-':
      if (!strcmp(first.str, "-")) {
        emit_binary(compiler, "subq %s, %%rax", rest);
        compiler->ret_type = Unknown;
      } else
        goto Unmatched;
      break;
    case '*':
      if (!strcmp(first.str, "*")) {
        emit_binary(compiler, "shr $2, %%rax\nimulq %s, %%rax", rest);
        compiler->ret_type = Unknown;
      } else
        goto Unmatched;
      break;
    case '/':
      if (!strcmp(first.str, "/")) {
        // Deal with the Devil
        emit_binary(compiler, "cqto\nidivq %s, %%rax\nshl $2, %%rax", rest);
        compiler->ret_type = Unknown;
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
    case 'a':
      if (!strcmp(first.str, "and")) {
        emit_binary(compiler, "andq %s, %%rax", rest);
        compiler->ret_type = Boolean;
      } else
        goto Unmatched;
      break;
    case 'b':
      if (!strcmp(first.str, "begin")) {
        emit_begin(compiler, rest);
        compiler->ret_type = Unknown;
      } else
        goto Unmatched;
      break;
    case 'c':
      if (!strcmp(first.str, "cons")) {
        emit_cons(compiler, rest);
        compiler->ret_type = Cons;
      } else if (!strcmp(first.str, "car")) {
        emit_unary(compiler, "movq -1(%rax), %rax", rest);
        compiler->ret_type = Unknown;
      } else if (!strcmp(first.str, "cdr")) {
        emit_unary(compiler, "movq 7(%rax), %rax", rest);
        compiler->ret_type = Unknown;
      } else if (!strcmp(first.str, "caar")) {
        emit_unary(compiler, "movq -1(%rax), %rax", rest);
        emit(compiler, strdup("movq -1(%rax), %rax"));
        compiler->ret_type = Unknown;
      } else if (!strcmp(first.str, "cadr")) {
        emit_unary(compiler, "movq 7(%rax), %rax", rest);
        emit(compiler, strdup("movq -1(%rax), %rax"));
        compiler->ret_type = Unknown;
      } else if (!strcmp(first.str, "cdar")) {
        emit_unary(compiler, "movq -1(%rax), %rax", rest);
        emit(compiler, strdup("movq 7(%rax), %rax"));
        compiler->ret_type = Unknown;
      } else if (!strcmp(first.str, "cddr")) {
        emit_unary(compiler, "movq 7(%rax), %rax", rest);
        emit(compiler, strdup("movq 7(%rax), %rax"));
        compiler->ret_type = Unknown;
      } else
        goto Unmatched;
      break;
    case 'd':
      if (!strcmp(first.str, "define")) {
        emit_define(compiler, rest);
      } else
        goto Unmatched;
      break;
    case 'e':
      if (!strcmp(first.str, "exit")) {
        emit(compiler, strdup("movq $0, %rdi\nmovq $60, %rax\nsyscall"));
        compiler->ret_type = None;
      } else
        goto Unmatched;
      break;
    case 'i':
      if (!strcmp(first.str, "if")) {
        emit_if(compiler, rest);
        compiler->ret_type = Unknown;
      } else
        goto Unmatched;
      break;
    case 's':
      if (!strcmp(first.str, "set!")) {
        emit_set(compiler, rest);
        compiler->ret_type = None;
      } else if (!strcmp(first.str, "set-car!")) {
        emit_carset(compiler, rest);
        compiler->ret_type = None;
      } else if (!strcmp(first.str, "set-cdr!")) {
        emit_cdrset(compiler, rest);
        compiler->ret_type = None;
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
        compiler->ret_type = Unknown;
      } else
        goto Unmatched;
      break;
    case 'n':
      if (!strcmp(first.str, "null?")) {
        emit_quest(compiler, "andl $7, %eax", 0, rest);
        compiler->ret_type = Boolean;
      } else
        goto Unmatched;
      break;
    case 'o':
      if (!strcmp(first.str, "one?")) {
        emit_quest(compiler, 0, 1, rest);
        compiler->ret_type = Boolean;
      } else if (!strcmp(first.str, "or")) {
        emit_binary(compiler, "orq %s, %%rax", rest);
        compiler->ret_type = Boolean;
      } else
        goto Unmatched;
      break;
    case 'l':
      if (!strcmp(first.str, "lambda")) {
        emit_lambda(compiler, 0, rest);
        compiler->ret_type = Lambda;
      } else if (!strcmp(first.str, "let")) {
        emit_let(compiler, rest);
        compiler->ret_type = Unknown;
      } else if (!strcmp(first.str, "let*")) {
        emit_letstar(compiler, rest);
        compiler->ret_type = Unknown;
      } else
        goto Unmatched;
      break;
    case 'v':
      if (!strcmp(first.str, "vector")) {
        emit_vec(compiler, rest);
        compiler->ret_type = Vector;
      } else if (!strcmp(first.str, "vector?")) {
        emit_quest(compiler, "andl $7, %eax", 2, rest);
        compiler->ret_type = Boolean;
      } else if (!strcmp(first.str, "vector-length")) {
        emit_unary(compiler, "movq -2(%rax), %rax", rest);
        compiler->ret_type = Fixnum;
      } else if (!strcmp(first.str, "vector-ref")) {
        emit_vecref(compiler, rest);
        compiler->ret_type = Unknown;
      } else if (!strcmp(first.str, "vector-set!")) {
        emit_vecset(compiler, rest);
        compiler->ret_type = None;
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
    Unmatched:
      emit_ufun(compiler, first.str, rest);
    }
    break;
  case List:
    emit_fun(compiler, first.exprs->arr[0], slice_start_exprs(first.exprs, 1));
    if (compiler->ret_type != Lambda) {
      errc(compiler, ExpectedFunSymb);
    }
    size_t l0 = compiler->label++;
    emit(compiler, strdup("movq %rax, %r13"));
    emit(compiler, strdup("movq -6(%r13), %rax"));
    emit(compiler, size_into_str("cmpq $%zu, %%rax", rest.len));
    emit(compiler, size_into_str("jne L%zu", l0));
    size_t a_count = spill_args(compiler);
    for (size_t i = 0; i < rest.len; i++) {
      emit_load_expr(compiler, rest.arr[i], i);
      compiler->env->arr[i].used = 1;
    }
    for (size_t i = 0; i < rest.len; i++) {
      compiler->env->arr[i].used = 0;
    }
    emit(compiler, strdup("movq 2(%r13), %rax"));
    emit(compiler, strdup("callq *%rax"));
    reorganize_args(compiler, a_count);
    emit(compiler, size_into_str("L%zu:", l0));
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
  case Null:
    emit_movq_imm_var(compiler, tag_nil(), index);
    compiler->env->arr[index].used = Nil;
    break;
  case Num:
    emit_movq_imm_var(compiler, tag_fixnum(expr.num), index);
    compiler->env->arr[index].used = Fixnum;
    break;
  case Chr:
    emit_movq_imm_var(compiler, tag_char(expr.ch), index);
    compiler->env->arr[index].used = Char;
    break;
  case Bool:
    emit_movq_imm_var(compiler, tag_bool(expr.ch), index);
    compiler->env->arr[index].used = Boolean;
    break;
  case Str:
    // WIP
    break;
  case Sym: {
    ssize_t found = find_env(compiler->env, expr.str);
    if (found == -1) {
      if (compiler->free) {
        found = find_strs(compiler->free, expr.str);
        if (found == -1) {
          found = compiler->free->len;
          push_strs(compiler->free, strdup(expr.str));
        }
        emit_movq_regmem_var(compiler, found * 8 + 10, R13, index);
      } else {
        errc(compiler, UndefinedSymb);
      }
    } else {
      emit_movq_var_var(compiler, found, index);
    }
    compiler->env->arr[index].used = compiler->env->arr[found].used;
    if (compiler->env->arr[found].args) {
      compiler->env->arr[index].args =
          clone_exprs(compiler->env->arr[found].args);
    }
    break;
  }
  case List:
    emit_fun(compiler, expr.exprs->arr[0], slice_start_exprs(expr.exprs, 1));
    emit_var_into_str(compiler, "movq %%rax, %s", index);
    compiler->env->arr[index].used = compiler->ret_type;
    if (compiler->ret_args) {
      compiler->env->arr[index].args = clone_exprs(compiler->ret_args);
    }
    break;
  case Vec:
    emit_vec(compiler, slice_start_exprs(expr.exprs, 0));
    emit_var_into_str(compiler, "movq %%rax, %s", index);
    compiler->env->arr[index].used = Vector;
    break;
  case Err:
    errc(compiler, ParserFailure);
    break;
  }
}

void emit_expr(compiler_t *compiler, expr_t expr) {
  switch (expr.type) {
  case Null:
    emit_movq_imm_reg(compiler, tag_nil(), Rax);
    compiler->ret_type = Nil;
    break;
  case Num:
    emit_movq_imm_reg(compiler, tag_fixnum(expr.num), Rax);
    compiler->ret_type = Fixnum;
    break;
  case Chr:
    emit_movq_imm_reg(compiler, tag_char(expr.ch), Rax);
    compiler->ret_type = Char;
    break;
  case Bool:
    emit_movq_imm_reg(compiler, tag_bool(expr.ch), Rax);
    compiler->ret_type = Boolean;
    break;
  case Str:
    // WIP
    break;
  case Sym: {
    ssize_t found = find_env(compiler->env, expr.str);
    if (found == -1) {
      if (compiler->free) {
        found = find_strs(compiler->free, expr.str);
        if (found == -1) {
          found = compiler->free->len;
          push_strs(compiler->free, strdup(expr.str));
        }
        emit(compiler, size_into_str("movq %zu(%%r13), %%rax", found * 8 + 10));
      } else {
        errc(compiler, UndefinedSymb);
      }
    } else {
      emit_movq_var_reg(compiler, found, Rax);
    }
    compiler->ret_type = compiler->env->arr[found].used;
    if (compiler->env->arr[found].args) {
      compiler->ret_args = clone_exprs(compiler->env->arr[found].args);
    }
    break;
  }
  case List:
    emit_fun(compiler, expr.exprs->arr[0], slice_start_exprs(expr.exprs, 1));
    break;
  case Vec:
    emit_vec(compiler, slice_start_exprs(expr.exprs, 0));
    compiler->ret_type = Vector;
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
    if (compiler->env->arr[i].str && compiler->env->arr[i].used >= Cons) {
      compiler->env->arr[i].root_spill = 1;
      emit_var_into_str(compiler, "movq %s, (%%r15)\naddq $8, %%r15", i);
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
      emit_movq_regmem_var(compiler, j * 8, R15, i);
      j++;
    }
  }
}

/// 6 is a magic number indicating when volatile passed arguments end
size_t spill_args(compiler_t *compiler) {
  size_t spilled = 0;
  for (size_t i = 0; i < 6 && i < compiler->env->len; i++) {
    if (compiler->env->arr[i].used && !compiler->env->arr[i].root_spill) {
      compiler->env->arr[i].arg_spill = compiler->env->arr[i].used;
      emit_var_into_str(compiler, "pushq %s", i);
      spilled++;
    }
  }
  return spilled;
}

void reorganize_args(compiler_t *compiler, size_t count) {
  for (size_t i = compiler->env->len, j = 0; j < count && i > 0; i--) {
    if (compiler->env->arr[i - 1].arg_spill) {
      compiler->env->arr[i - 1].used = compiler->env->arr[i - 1].arg_spill;
      compiler->env->arr[i - 1].arg_spill = 0;
      emit_var_into_str(compiler, "popq %s", i - 1);
      j++;
    }
  }
}

void collect(compiler_t *compiler, size_t request) {
  size_t p_count = spill_pointers(compiler);
  size_t a_count = spill_args(compiler);
  emit(compiler,
       size_into_str("movq %%r15, %%rdi\nmovq $%zu, %%rsi\ncallq collect",
                     request));
  reorganize_args(compiler, a_count);
  reorganize_pointers(compiler, p_count);
}

void collect_ret(compiler_t *compiler, size_t extra) {
  size_t p_count = spill_pointers(compiler);
  size_t a_count = spill_args(compiler);
  emit(compiler, strdup("pushq %rax"));
  emit(
      compiler,
      size_into_str(
          "movq %%r15, %%rdi\nleaq %zu(,%rax,2), %%rsi\ncallq collect", extra));
  emit(compiler, strdup("popq %rax"));
  reorganize_args(compiler, a_count);
  reorganize_pointers(compiler, p_count);
}

// NOTE: Currently it uses emit_main/end which are not compatible with newer
// emit_movq_* and friends, but this is an easy update with an enum in compiler
// struct.
void emit_start_end(compiler_t *compiler) {
  emit_main(compiler, strdup("main:"));
  if (compiler->env->len > compiler->env->stack) {
    emit_main(compiler,
              size_into_str("subq $%zu, %%rsp",
                            (compiler->env->len - compiler->env->stack) * 8));
  }
  if (compiler->heap) {
    emit_main(compiler, size_into_str("movq $%zu, %%rdi", compiler->heap_size));
    emit_main(compiler, size_into_str("movq $%zu, %%rsi", compiler->heap_size));
    emit_main(compiler, strdup("callq init_gc"));
    emit_main(compiler, strdup("movq rs_begin(%rip), %r15"));
  }
  if (compiler->env->len > compiler->env->stack) {
    emit_end(compiler,
             size_into_str("addq $%zu, %%rsp",
                           (compiler->env->len - compiler->env->stack) * 8));
  }
  emit_end(compiler, strdup("movq %rax, %rdi"));
  emit_end(compiler, strdup("callq print"));
  if (compiler->heap) {
    emit_end(compiler, strdup("callq cleanup"));
  }
  emit_end(compiler, strdup("movl $0, %eax"));
}

strs_t *compile(compiler_t *compiler, exprs_t *exprs, size_t heap_size,
                const char *src) {
  // Ensure no garbage
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
  if (compiler->end) {
    delete_strs(compiler->end);
    compiler->end = create_strs(2);
  }

  // (Re)Init
  compiler->input = exprs;
  compiler->heap_size = heap_size;
  compiler->src = src;

  // Asm emit
  emit_exprs(compiler);
  emit_start_end(compiler);
  strs_t *fun_p = extract_main_dstrs(compiler->fun);
  const strs_t *ingredients[4] = {fun_p, compiler->main, compiler->body,
                                  compiler->end};
  return union_strs(ingredients, 4);
}
