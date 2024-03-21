#include "compiler.h"
#include "bitmat.h"
#include "dstrs.h"
#include "env.h"
#include "errs.h"
#include "expr.h"
#include "exprs.h"
#include "free.h"
#include "strs.h"
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define args(...) __VA_ARGS__

/// Mallocing and emitting automatically for snprintf with as many args as you
/// want. Compiler emitter owns these mallocs, so they need not be cared about.
/// Preferably abstracted over.
#define mal_sprintf(str, size, name, args)                                     \
  size = snprintf(0, 0, str, args);                                            \
  name = malloc(sizeof(*name) * (size + 1));                                   \
  snprintf(name, size + 1, str, args);
#define emit_mal_sprintf(str, args)                                            \
  int size = snprintf(0, 0, str, args);                                        \
  char *phrase = malloc(sizeof(*phrase) * (size + 1));                         \
  snprintf(phrase, size + 1, str, args);                                       \
  emit(compiler, phrase);

compiler_t *create_compiler() {
  compiler_t *compiler = malloc(sizeof(*compiler));
  compiler->input = 0;
  compiler->line = 0;
  compiler->loc = 0;
  compiler->heap = 0;
  compiler->heap_size = 0;
  compiler->lambda = 0;
  compiler->label = 0;
  compiler->emit = 1;
  compiler->ret_type = None;
  compiler->ret_args = 0;
  compiler->free = 0;
  compiler->env = create_env(8, 3, 3, 0, 2);
  compiler->bss = create_strs(2);
  push_strs(compiler->bss, strdup(".bss"));
  compiler->data = create_strs(2);
  push_strs(compiler->data, strdup(".data"));
  compiler->fun = create_dstrs(4);
  push_dstrs(compiler->fun, strdup(".text\n.global main"));
  compiler->main = create_strs(6);
  compiler->quotes = create_strs(2);
  compiler->body = create_strs(8);
  compiler->end = create_strs(2);
  compiler->emit = Body;
  compiler->errs = create_errs(3);
  compiler->src = 0;
  return compiler;
}

void delete_compiler(compiler_t *compiler) {
  if (compiler->input)
    delete_exprs(compiler->input);
  if (compiler->env)
    delete_env(compiler->env);
  if (compiler->free)
    delete_free(compiler->free);
  if (compiler->bss)
    delete_strs(compiler->bss);
  if (compiler->data)
    delete_strs(compiler->data);
  if (compiler->fun)
    delete_dstrs(compiler->fun);
  if (compiler->body)
    delete_strs(compiler->body);
  if (compiler->quotes)
    delete_strs(compiler->quotes);
  if (compiler->main)
    delete_strs(compiler->main);
  if (compiler->end)
    delete_strs(compiler->end);
  if (compiler->errs)
    delete_errs(compiler->errs);
  free(compiler);
}

/// Emit a single expression
void emit_expr(compiler_t *compiler, expr_t expr);
/// Emit instructions and store at reg/var
void emit_store_expr(compiler_t *compiler, expr_t expr, size_t index,
                     size_t var_index, int use_var);
/// Attempt to emit optimized tail call
void try_emit_tail_call(compiler_t *compiler, const char *name, exprs_t args,
                        expr_t last);
/// GC collect call
void collect(compiler_t *compiler, size_t request);
/// GC collect call from return register + extra
void collect_retq(compiler_t *compiler, size_t extra);
/// GC collect call from return register + extra for bytes like strings
void collect_retb(compiler_t *compiler, size_t extra);
/// Spill function arguments into stack to preserve them
size_t spill_args(compiler_t *compiler);
/// Restore spilled into stack arguments
void reorganize_args(compiler_t *compiler, size_t count);

int has_errc(compiler_t *compiler) { return compiler->errs->len; }

void errc(compiler_t *compiler, enum err err) {
  push_errs(compiler->errs,
            (err_t){.type = err, .line = compiler->line, .loc = compiler->loc});
}

void emit(compiler_t *compiler, char *phrase) {
  switch (compiler->emit) {
  case Bss:
    push_strs(compiler->bss, phrase);
    break;
  case Data:
    push_strs(compiler->data, phrase);
    break;
  case Fun:
    push_dstrs(compiler->fun, phrase);
    break;
  case Main:
    push_strs(compiler->main, phrase);
    break;
  case Quotes:
    push_strs(compiler->quotes, phrase);
    break;
  case Body:
    push_strs(compiler->body, phrase);
    break;
  case End:
    push_strs(compiler->end, phrase);
    break;
  }
}

ssize_t tag_fixnum(ssize_t num) { return num << 2; }
size_t tag_char(size_t ch) { return (ch << 8) | 0x0f; }
size_t tag_unichar(size_t uch) { return (uch << 8) | 0x0f; }
size_t tag_bool(size_t bool) { return (bool << 7) | 0x1f; }
size_t tag_nil() { return 0x2f; }

int is_utf8(const char *str) {
  while (*str++) {
    if (*str & (1 << 8)) {
      return 1;
    }
  }
  return 0;
}

/// NOTE: Despite quad word forms, these are also used for lower forms like
/// double word and byte
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

const char *reg_to_str(enum reg reg) {
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

const char *regl_to_str(enum reg reg) {
  switch (reg) {
  case Rax:
    return "%eax";
  case Rdi:
    return "%edi";
  case Rsi:
    return "%esi";
  case Rdx:
    return "%edx";
  case Rcx:
    return "%ecx";
  case R8:
    return "%r8l";
  case R9:
    return "%r9l";
  case R10:
    return "%r10l";
  case R11:
    return "%r11l";
  case Rbx:
    return "%ebx";
  case Rbp:
    return "%ebp";
  case R12:
    return "%r12l";
  case R13:
    return "%r13l";
  case R14:
    return "%r14l";
  case R15:
    return "%r15l";
  case Rsp:
    return "%esp";
  default:
    return "FAILL"; // Should not reach here
  }
}

const char *regw_to_str(enum reg reg) {
  switch (reg) {
  case Rax:
    return "%ax";
  case Rdi:
    return "%di";
  case Rsi:
    return "%si";
  case Rdx:
    return "%dx";
  case Rcx:
    return "%cx";
  case R8:
    return "%r8w";
  case R9:
    return "%r9w";
  case R10:
    return "%r10w";
  case R11:
    return "%r11w";
  case Rbx:
    return "%bx";
  case Rbp:
    return "%bp";
  case R12:
    return "%r12w";
  case R13:
    return "%r13w";
  case R14:
    return "%r14w";
  case R15:
    return "%r15w";
  case Rsp:
    return "%sp";
  default:
    return "FAIL"; // Should not reach here
  }
}

const char *regb_to_str(enum reg reg) {
  switch (reg) {
  case Rax:
    return "%al";
  case Rdi:
    return "%dil";
  case Rsi:
    return "%sil";
  case Rdx:
    return "%dl";
  case Rcx:
    return "%cl";
  case R8:
    return "%r8b";
  case R9:
    return "%r9b";
  case R10:
    return "%r10b";
  case R11:
    return "%r11b";
  case Rbx:
    return "%bl";
  case Rbp:
    return "%bpl";
  case R12:
    return "%r12b";
  case R13:
    return "%r13b";
  case R14:
    return "%r14b";
  case R15:
    return "%r15b";
  case Rsp:
    return "%spl";
  default:
    return "FAILB"; // Should not reach here
  }
}

ssize_t calc_stack(ssize_t stack_loc) { return stack_loc * 8 - 8; }

// These, while unpleasant, have some important usages in significant generic
// functions, so for now they will remain.
// HACK: TEMPORARY
void emit_var_str(compiler_t *compiler, const char *str, size_t var) {

  if (var >= compiler->env->stack_offset &&
      compiler->env->stack_offset > compiler->env->reserved_offset) {
    int len;
    char *tmp;
    if (calc_stack(compiler->env->stack_offset - var)) {
      mal_sprintf(
          "%zd(%s)", len, tmp,
          args(calc_stack(compiler->env->stack_offset - var), reg_to_str(Rsp)));
    } else {
      mal_sprintf("(%s)", len, tmp, args(reg_to_str(Rsp)));
    }
    emit_mal_sprintf(str, args(tmp));
    free(tmp);
  } else {
    emit_mal_sprintf(str, args(reg_to_str(var + 1)));
  }
}

// HACK: TEMPORARY
void emit_size_str(compiler_t *compiler, const char *str, size_t num) {
  emit_mal_sprintf(str, args(num));
}

// HACK: TEMPORARY
void emit_str(compiler_t *compiler, const char *str) {
  emit(compiler, strdup(str));
}

void emit_genins_reg(compiler_t *compiler, const char *ins,
                     const char *(*regf)(enum reg), enum reg reg) {
  emit_mal_sprintf("%s %s", args(ins, regf(reg)));
}

// NOTE: mem_reg must always be 64 bit form on x86_64.
// Either redesign this with just a simpler macro system,
// or rethink current system.
void emit_genins_regmem(compiler_t *compiler, const char *ins, size_t add,
                        enum reg mem_reg) {
  if (add) {
    emit_mal_sprintf("%s %zu(%s)", args(ins, add, reg_to_str(mem_reg)));
  } else {
    emit_mal_sprintf("%s (%s)", args(ins, reg_to_str(mem_reg)));
  }
}

void emit_genins_var(compiler_t *compiler, const char *ins,
                     const char *(*regf)(enum reg), size_t var) {
  if (var >= compiler->env->stack_offset &&
      compiler->env->stack_offset > compiler->env->reserved_offset) {
    emit_genins_regmem(compiler, ins,
                       calc_stack(compiler->env->stack_offset - var), Rsp);
  } else {
    emit_genins_reg(compiler, ins, regf, var + 1);
  }
}

void emit_genins_reg_reg(compiler_t *compiler, const char *ins,
                         const char *(*regf)(enum reg), enum reg reg1,
                         enum reg reg2) {
  emit_mal_sprintf("%s %s, %s", args(ins, regf(reg1), regf(reg2)));
}

void emit_genins_regmem_reg(compiler_t *compiler, const char *ins,
                            const char *(*regf)(enum reg), ssize_t add,
                            enum reg mem_reg, enum reg reg) {
  if (add) {
    emit_mal_sprintf("%s %zd(%s), %s",
                     args(ins, add, reg_to_str(mem_reg), regf(reg)));
  } else {
    emit_mal_sprintf("%s (%s), %s", args(ins, reg_to_str(mem_reg), regf(reg)));
  }
}

void emit_genins_reg_regmem(compiler_t *compiler, const char *ins,
                            const char *(*regf)(enum reg), enum reg reg,
                            ssize_t add, enum reg mem_reg) {
  if (add) {
    emit_mal_sprintf("%s %s, %zd(%s)",
                     args(ins, regf(reg), add, reg_to_str(mem_reg)));
  } else {
    emit_mal_sprintf("%s %s, (%s)", args(ins, regf(reg), reg_to_str(mem_reg)));
  }
}

void emit_genins_regmem_regmem(compiler_t *compiler, const char *ins,
                               const char *movins,
                               const char *(*regf)(enum reg), size_t add1,
                               enum reg mem_reg1, size_t add2,
                               enum reg mem_reg2) {
  size_t tmp =
      get_unused_pren_env(compiler->env, compiler->env->reserved_offset);
  if (tmp & 1) {
    size_t new =
        reassign_postn_env(compiler->env, tmp, compiler->env->nonvol_offset);
    emit_genins_reg_reg(compiler, movins, regf, (tmp >> 1) + 1, new + 1);
  }
  if (add1 && add2) {
    emit_mal_sprintf("%s %zu(%s) %s\n%s %s, %zu(%s)",
                     args(movins, add1, reg_to_str(mem_reg1),
                          regf((tmp >> 1) + 1), ins, regf((tmp >> 1) + 1), add2,
                          reg_to_str(mem_reg2)));
  } else if (add1) {
    emit_mal_sprintf("%s %zu(%s) %s\n%s %s, (%s)",
                     args(movins, add1, reg_to_str(mem_reg1),
                          regf((tmp >> 1) + 1), ins, regf((tmp >> 1) + 1),
                          reg_to_str(mem_reg2)));
  } else if (add2) {
    emit_mal_sprintf("%s (%s) %s\n%s %s, %zu(%s)",
                     args(movins, reg_to_str(mem_reg1), regf((tmp >> 1) + 1),
                          ins, regf((tmp >> 1) + 1), add2,
                          reg_to_str(mem_reg2)));
  } else {
    emit_mal_sprintf("%s (%s) %s\n%s %s, (%s)",
                     args(movins, reg_to_str(mem_reg1), regf((tmp >> 1) + 1),
                          ins, regf((tmp >> 1) + 1), reg_to_str(mem_reg2)));
  }
  remove_env(compiler->env, tmp);
}

// void emit_genins_constmem_regmem(compiler_t *compiler, const char *ins,
//                                  const char *movins,
//                                  const char *(*regf)(enum reg),
//                                  size_t const_var, size_t add,
//                                  enum reg mem_reg) {
//   size_t tmp =
//       get_unused_pren_env(compiler->env, compiler->env->reserved_offset);
//   if (tmp & 1) {
//     size_t new =
//         reassign_postn_env(compiler->env, tmp, compiler->env->nonvol_offset);
//     emit_genins_reg_reg(compiler, movins, regf, (tmp >> 1) + 1, new + 1);
//   }
//   if (add) {
//     emit_mal_sprintf("%s (%s) %s\n%s %s, %zu(%s)",
//                      args(movins, compiler->env->arr[const_var].str,
//                           regf((tmp >> 1) + 1), ins, regf((tmp >> 1) + 1),
//                           add, reg_to_str(mem_reg)));
//   } else {
//     emit_mal_sprintf("%s (%s) %s\n%s %s, (%s)",
//                      args(movins, compiler->env->arr[const_var].str,
//                           regf((tmp >> 1) + 1), ins, regf((tmp >> 1) + 1),
//                           reg_to_str(mem_reg)));
//   }
//   remove_env(compiler->env, tmp);
// }
//
// void emit_genins_regmem_constmem(compiler_t *compiler, const char *ins,
//                                  const char *movins,
//                                  const char *(*regf)(enum reg), size_t add,
//                                  enum reg mem_reg, size_t const_var) {
//   size_t tmp =
//       get_unused_pren_env(compiler->env, compiler->env->reserved_offset);
//   if (tmp & 1) {
//     size_t new =
//         reassign_postn_env(compiler->env, tmp, compiler->env->nonvol_offset);
//     emit_genins_reg_reg(compiler, movins, regf, (tmp >> 1) + 1, new + 1);
//   }
//   if (add) {
//     emit_mal_sprintf("%s %zu(%s) %s\n%s %s, (%s)",
//                      args(movins, add, reg_to_str(mem_reg),
//                           regf((tmp >> 1) + 1), ins, regf((tmp >> 1) + 1),
//                           compiler->env->arr[const_var].str));
//   } else {
//     emit_mal_sprintf("%s (%s) %s\n%s %s, (%s)",
//                      args(movins, reg_to_str(mem_reg), regf((tmp >> 1) + 1),
//                           ins, regf((tmp >> 1) + 1),
//                           compiler->env->arr[const_var].str));
//   }
//   remove_env(compiler->env, tmp);
// }

void emit_genins_imm_reg(compiler_t *compiler, const char *ins,
                         const char *(*regf)(enum reg), ssize_t imm,
                         enum reg reg) {
  emit_mal_sprintf("%s $%zd, %s", args(ins, imm, regf(reg)));
}

void emit_genins_reg_var(compiler_t *compiler, const char *ins,
                         const char *(*regf)(enum reg), enum reg reg,
                         size_t var) {
  if (var >= compiler->env->stack_offset) {
    emit_genins_reg_regmem(compiler, ins, regf, reg,
                           calc_stack(compiler->env->stack_offset - var), Rsp);
  } else {
    emit_genins_reg_reg(compiler, ins, regf, reg, var + 1);
  }
}

void emit_genins_imm_regmem(compiler_t *compiler, const char *ins, ssize_t imm,
                            ssize_t add, enum reg mem_reg) {
  if (add) {
    emit_mal_sprintf("%s $%zd, %zd(%s)",
                     args(ins, imm, add, reg_to_str(mem_reg)));
  } else {
    emit_mal_sprintf("%s $%zd, (%s)", args(ins, imm, reg_to_str(mem_reg)));
  }
}

void emit_genins_imm_var(compiler_t *compiler, const char *ins,
                         const char *(*regf)(enum reg), ssize_t imm,
                         size_t var) {
  if (var >= compiler->env->stack_offset &&
      compiler->env->stack_offset > compiler->env->reserved_offset) {
    emit_genins_imm_regmem(compiler, ins, imm,
                           calc_stack(compiler->env->stack_offset - var), Rsp);
  } else {
    emit_genins_imm_reg(compiler, ins, regf, imm, var + 1);
  }
}

void emit_genins_genlabel_imm(compiler_t *compiler, const char *ins,
                              const char *label, size_t idx, ssize_t imm) {
  emit_mal_sprintf("%s %s%zu, %zd", args(ins, label, idx, imm));
}

void emit_genins_genlabel_reg(compiler_t *compiler, const char *ins,
                              const char *label, const char *(*regf)(enum reg),
                              size_t idx, enum reg reg) {
  emit_mal_sprintf("%s $%s%zu, %s", args(ins, label, idx, regf(reg)));
}

void emit_genins_genlabel_regmem(compiler_t *compiler, const char *ins,
                                 const char *label, size_t idx, ssize_t add,
                                 enum reg mem_reg) {
  if (add) {
    emit_mal_sprintf("%s $%s%zd, %zd(%s)",
                     args(ins, label, idx, add, reg_to_str(mem_reg)));
  } else {
    emit_mal_sprintf("%s $%s%zd, (%s)",
                     args(ins, label, idx, reg_to_str(mem_reg)));
  }
}

void emit_genins_genlabel_var(compiler_t *compiler, const char *ins,
                              const char *label, const char *(*regf)(enum reg),
                              size_t idx, size_t var) {
  if (var >= compiler->env->stack_offset &&
      compiler->env->stack_offset > compiler->env->reserved_offset) {
    emit_genins_genlabel_regmem(compiler, ins, label, idx,
                                calc_stack(compiler->env->stack_offset - var),
                                Rsp);
  } else {
    emit_genins_genlabel_reg(compiler, ins, label, regf, idx, var + 1);
  }
}

void emit_genins_var_reg(compiler_t *compiler, const char *ins,
                         const char *(*regf)(enum reg), size_t var,
                         enum reg reg) {
  if (var >= compiler->env->stack_offset) {
    emit_genins_regmem_reg(compiler, ins, regf,
                           calc_stack(compiler->env->stack_offset - var), Rsp,
                           reg);
  } else {
    emit_genins_reg_reg(compiler, ins, regf, var + 1, reg);
  }
}

void emit_genins_var_regmem(compiler_t *compiler, const char *ins,
                            const char *movins, const char *(*regf)(enum reg),
                            size_t var, ssize_t add, enum reg mem_reg) {
  if (var >= compiler->env->stack_offset) {
    emit_genins_regmem_regmem(compiler, ins, movins, regf,
                              calc_stack(compiler->env->stack_offset - var),
                              Rsp, add, mem_reg);
  } else {
    emit_genins_reg_regmem(compiler, ins, regf, var + 1, add, mem_reg);
  }
}

void emit_genins_regmem_var(compiler_t *compiler, const char *ins,
                            const char *movins, const char *(*regf)(enum reg),
                            ssize_t add, enum reg mem_reg, size_t var) {
  if (var >= compiler->env->stack_offset) {
    emit_genins_regmem_regmem(compiler, ins, movins, regf, add, mem_reg,
                              calc_stack(compiler->env->stack_offset - var),
                              Rsp);
  } else {
    emit_genins_regmem_reg(compiler, ins, regf, add, mem_reg, var + 1);
  }
}

void emit_genins_var_var(compiler_t *compiler, const char *ins,
                         const char *movins, const char *(*regf)(enum reg),
                         size_t var1, size_t var2) {
  if (var1 >= compiler->env->stack_offset &&
      var2 >= compiler->env->stack_offset) {
    emit_genins_regmem_regmem(
        compiler, ins, movins, regf,
        calc_stack(compiler->env->stack_offset - var1), Rsp,
        calc_stack(compiler->env->stack_offset - var2), Rsp);
  } else if (var1 >= compiler->env->stack_offset) {
    emit_genins_regmem_reg(compiler, ins, regf,
                           calc_stack(compiler->env->stack_offset - var1), Rsp,
                           var2 + 1);
  } else if (var2 >= compiler->env->stack_offset) {
    emit_genins_reg_regmem(compiler, ins, regf, var1 + 1,
                           calc_stack(compiler->env->stack_offset - var2), Rsp);
  } else {
    emit_genins_reg_reg(compiler, ins, regf, var1 + 1, var2 + 1);
  }
}

void emit_movq_reg_reg(compiler_t *compiler, enum reg reg1, enum reg reg2) {
  if (reg1 != reg2) {
    emit_genins_reg_reg(compiler, "movq", reg_to_str, reg1, reg2);
  }
}

void emit_movq_regmem_reg(compiler_t *compiler, ssize_t add, enum reg mem_reg,
                          enum reg reg) {
  emit_genins_regmem_reg(compiler, "movq", reg_to_str, add, mem_reg, reg);
}

void emit_movq_var_reg(compiler_t *compiler, size_t var, enum reg reg2) {
  emit_genins_var_reg(compiler, "movq", reg_to_str, var, reg2);
}

void emit_movq_const_reg(compiler_t *compiler, size_t var, enum reg reg2) {
  emit_genins_genlabel_reg(compiler, "movq", "const", reg_to_str, var, reg2);
}

void emit_movq_const_regmem(compiler_t *compiler, size_t var, ssize_t add,
                            enum reg mem_reg) {
  emit_genins_genlabel_regmem(compiler, "movq", "const", var, add, mem_reg);
}

void emit_movq_const_var(compiler_t *compiler, size_t var, size_t var2) {
  emit_genins_genlabel_var(compiler, "movq", "const", reg_to_str, var, var2);
}

void emit_movq_reg_regmem(compiler_t *compiler, enum reg reg, ssize_t add,
                          enum reg mem_reg) {
  emit_genins_reg_regmem(compiler, "movq", reg_to_str, reg, add, mem_reg);
}

void emit_movb_reg_regmem(compiler_t *compiler, enum reg reg, ssize_t add,
                          enum reg mem_reg) {
  emit_genins_reg_regmem(compiler, "movb", regb_to_str, reg, add, mem_reg);
}

void emit_movq_reg_var(compiler_t *compiler, enum reg reg, size_t var) {
  emit_genins_reg_var(compiler, "movq", reg_to_str, reg, var);
}

void emit_movq_regmem_regmem(compiler_t *compiler, size_t add1,
                             enum reg mem_reg1, size_t add2,
                             enum reg mem_reg2) {
  if (mem_reg1 != mem_reg2 && add1 != add2) {
    emit_genins_regmem_regmem(compiler, "movq", "movq", reg_to_str, add1,
                              mem_reg1, add2, mem_reg2);
  }
}

void emit_movb_regmem_regmem(compiler_t *compiler, size_t add1,
                             enum reg mem_reg1, size_t add2,
                             enum reg mem_reg2) {
  if (mem_reg1 != mem_reg2 && add1 != add2) {
    emit_genins_regmem_regmem(compiler, "movb", "movb", regb_to_str, add1,
                              mem_reg1, add2, mem_reg2);
  }
}

void emit_movq_var_regmem(compiler_t *compiler, size_t var, ssize_t add,
                          enum reg mem_reg) {
  emit_genins_var_regmem(compiler, "movq", "movq", reg_to_str, var, add,
                         mem_reg);
}

void emit_movb_var_regmem(compiler_t *compiler, size_t var, ssize_t add,
                          enum reg mem_reg) {
  emit_genins_var_regmem(compiler, "movb", "movb", regb_to_str, var, add,
                         mem_reg);
}

void emit_movq_regmem_var(compiler_t *compiler, size_t add, enum reg mem_reg,
                          size_t var) {
  emit_genins_regmem_var(compiler, "movq", "movq", reg_to_str, add, mem_reg,
                         var);
}

void emit_movq_var_var(compiler_t *compiler, size_t var1, size_t var2) {
  emit_genins_var_var(compiler, "movq", "movq", reg_to_str, var1, var2);
}

void emit_movq_imm_reg(compiler_t *compiler, ssize_t imm, enum reg reg) {
  emit_genins_imm_reg(compiler, "movq", reg_to_str, imm, reg);
}

void emit_movq_imm_regmem(compiler_t *compiler, ssize_t imm, ssize_t add,
                          enum reg mem_reg) {
  emit_genins_imm_regmem(compiler, "movq", imm, add, mem_reg);
}

void emit_movq_imm_var(compiler_t *compiler, ssize_t imm, size_t var) {
  emit_genins_imm_var(compiler, "movq", reg_to_str, imm, var);
}

void emit_movq_reg_fullmem(compiler_t *compiler, enum reg reg, ssize_t add,
                           enum reg mem_reg, enum reg add_reg, int mult) {
  emit_mal_sprintf("movq %s, %zd(%s, %s, %d)",
                   args(reg_to_str(reg), add, reg_to_str(mem_reg),
                        reg_to_str(add_reg), mult));
}

void emit_movb_reg_fullmem(compiler_t *compiler, enum reg reg, ssize_t add,
                           enum reg mem_reg, enum reg add_reg, int mult) {
  emit_mal_sprintf("movb %s, %zd(%s, %s, %d)",
                   args(regb_to_str(reg), add, reg_to_str(mem_reg),
                        reg_to_str(add_reg), mult));
}

void emit_movq_regmem_fullmem(compiler_t *compiler, size_t add1,
                              enum reg mem_reg1, ssize_t add2,
                              enum reg mem_reg2, enum reg add_reg, int mult) {
  size_t tmp =
      get_unused_pren_env(compiler->env, compiler->env->reserved_offset);
  if (tmp & 1) {
    size_t new =
        reassign_postn_env(compiler->env, tmp, compiler->env->nonvol_offset);
    emit_movq_reg_reg(compiler, (tmp >> 1) + 1, new + 1);
  }
  if (add1 && add2) {
    emit_mal_sprintf("movq %zu(%s) %s\nmovq %s, %zu(%s, %s, %d)",
                     args(add1, reg_to_str(mem_reg1),
                          reg_to_str((tmp >> 1) + 1),
                          reg_to_str((tmp >> 1) + 1), add2,
                          reg_to_str(mem_reg2), reg_to_str(add_reg), mult));
  } else if (add1) {
    emit_mal_sprintf("movq %zu(%s) %s\nmovq %s, (%s, %s, %d)",
                     args(add1, reg_to_str(mem_reg1),
                          reg_to_str((tmp >> 1) + 1),
                          reg_to_str((tmp >> 1) + 1), reg_to_str(mem_reg2),
                          reg_to_str(add_reg), mult));
  } else if (add2) {
    emit_mal_sprintf("movq (%s) %s\nmovq %s, %zu(%s, %s, %d)",
                     args(reg_to_str(mem_reg1), reg_to_str((tmp >> 1) + 1),
                          reg_to_str((tmp >> 1) + 1), add2,
                          reg_to_str(mem_reg2), reg_to_str(add_reg), mult));
  }
  remove_env(compiler->env, tmp);
}

void emit_movb_regmem_fullmem(compiler_t *compiler, size_t add1,
                              enum reg mem_reg1, ssize_t add2,
                              enum reg mem_reg2, enum reg add_reg, int mult) {
  size_t tmp =
      get_unused_pren_env(compiler->env, compiler->env->reserved_offset);
  if (tmp & 1) {
    size_t new =
        reassign_postn_env(compiler->env, tmp, compiler->env->nonvol_offset);
    emit_movq_reg_reg(compiler, (tmp >> 1) + 1, new + 1);
  }
  if (add1 && add2) {
    emit_mal_sprintf("movb %zu(%s) %s\nmovb %s, %zu(%s, %s, %d)",
                     args(add1, reg_to_str(mem_reg1),
                          regb_to_str((tmp >> 1) + 1),
                          regb_to_str((tmp >> 1) + 1), add2,
                          reg_to_str(mem_reg2), reg_to_str(add_reg), mult));
  } else if (add1) {
    emit_mal_sprintf("movb %zu(%s) %s\nmovb %s, (%s, %s, %d)",
                     args(add1, reg_to_str(mem_reg1),
                          regb_to_str((tmp >> 1) + 1),
                          regb_to_str((tmp >> 1) + 1), reg_to_str(mem_reg2),
                          reg_to_str(add_reg), mult));
  } else if (add2) {
    emit_mal_sprintf("movb (%s) %s\nmovb %s, %zu(%s, %s, %d)",
                     args(reg_to_str(mem_reg1), regb_to_str((tmp >> 1) + 1),
                          regb_to_str((tmp >> 1) + 1), add2,
                          reg_to_str(mem_reg2), reg_to_str(add_reg), mult));
  }
  remove_env(compiler->env, tmp);
}

void emit_movq_var_fullmem(compiler_t *compiler, size_t var, ssize_t add,
                           enum reg mem_reg, enum reg add_reg, int mult) {
  if (var >= compiler->env->stack_offset) {
    emit_movq_regmem_fullmem(compiler,
                             calc_stack(compiler->env->stack_offset - var), Rsp,
                             add, mem_reg, add_reg, mult);
  } else {
    emit_movq_reg_fullmem(compiler, var + 1, add, mem_reg, add_reg, mult);
  }
}

void emit_movb_var_fullmem(compiler_t *compiler, size_t var, ssize_t add,
                           enum reg mem_reg, enum reg add_reg, int mult) {
  if (var >= compiler->env->stack_offset) {
    emit_movb_regmem_fullmem(compiler,
                             calc_stack(compiler->env->stack_offset - var), Rsp,
                             add, mem_reg, add_reg, mult);
  } else {
    emit_movb_reg_fullmem(compiler, var + 1, add, mem_reg, add_reg, mult);
  }
}

void emit_movq_fullmem_reg(compiler_t *compiler, ssize_t add, enum reg mem_reg,
                           enum reg add_reg, int mult, enum reg reg) {
  emit_mal_sprintf("movq %zd(%s, %s, %d), %s",
                   args(add, reg_to_str(mem_reg), reg_to_str(add_reg), mult,
                        reg_to_str(reg)));
}

void emit_movl_fullmem_reg(compiler_t *compiler, ssize_t add, enum reg mem_reg,
                           enum reg add_reg, int mult, enum reg reg) {
  emit_mal_sprintf("movl %zd(%s, %s, %d), %s",
                   args(add, reg_to_str(mem_reg), reg_to_str(add_reg), mult,
                        regl_to_str(reg)));
}

void emit_movw_fullmem_reg(compiler_t *compiler, ssize_t add, enum reg mem_reg,
                           enum reg add_reg, int mult, enum reg reg) {
  emit_mal_sprintf("movw %zd(%s, %s, %d), %s",
                   args(add, reg_to_str(mem_reg), reg_to_str(add_reg), mult,
                        regw_to_str(reg)));
}

void emit_movb_fullmem_reg(compiler_t *compiler, ssize_t add, enum reg mem_reg,
                           enum reg add_reg, int mult, enum reg reg) {
  emit_mal_sprintf("movb %zd(%s, %s, %d), %s",
                   args(add, reg_to_str(mem_reg), reg_to_str(add_reg), mult,
                        regb_to_str(reg)));
}

// TODO: ADD fullmem_regmem
void emit_movb_fullmem_var(compiler_t *compiler, ssize_t add, enum reg mem_reg,
                           enum reg add_reg, int mult, size_t var) {
  emit_mal_sprintf("movb %zd(%s, %s, %d), %s",
                   args(add, reg_to_str(mem_reg), reg_to_str(add_reg), mult,
                        regb_to_str(var + 1)));
}

void emit_leaq_label_reg(compiler_t *compiler, const char *label_name,
                         size_t label_num, enum reg reg) {
  emit_mal_sprintf("leaq %s%zu(%%rip), %s",
                   args(label_name, label_num, reg_to_str(reg)));
}

void emit_leaq_label_regmem(compiler_t *compiler, const char *label_name,
                            size_t label_num, size_t add, enum reg mem_reg) {
  if (add) {
    emit_mal_sprintf("leaq %s%zu(%%rip), %zu(%s)",
                     args(label_name, label_num, add, reg_to_str(mem_reg)));
  } else {
    emit_mal_sprintf("leaq %s%zu(%%rip), (%s)",
                     args(label_name, label_num, reg_to_str(mem_reg)));
  }
}

void emit_leaq_label_var(compiler_t *compiler, const char *label_name,
                         size_t label_num, size_t var) {
  if (var >= compiler->env->stack_offset &&
      compiler->env->stack_offset > compiler->env->reserved_offset) {
    emit_leaq_label_regmem(compiler, label_name, label_num,
                           calc_stack(compiler->env->stack_offset - var), Rsp);
  } else {
    emit_leaq_label_reg(compiler, label_name, label_num, var + 1);
  }
}

void emit_decq_reg(compiler_t *compiler, enum reg reg) {
  emit_genins_reg(compiler, "decq", reg_to_str, reg);
}

void emit_decq_regmem(compiler_t *compiler, size_t add, enum reg mem_reg) {
  emit_genins_regmem(compiler, "decq", add, mem_reg);
}

void emit_decq_var(compiler_t *compiler, size_t var) {
  emit_genins_var(compiler, "decq", reg_to_str, var);
}

void emit_incq_reg(compiler_t *compiler, enum reg reg) {
  emit_genins_reg(compiler, "incq", reg_to_str, reg);
}

void emit_incq_regmem(compiler_t *compiler, size_t add, enum reg mem_reg) {
  emit_genins_regmem(compiler, "incq", add, mem_reg);
}

void emit_incq_var(compiler_t *compiler, size_t var) {
  emit_genins_var(compiler, "incq", reg_to_str, var);
}

void emit_orq_imm_reg(compiler_t *compiler, size_t imm, enum reg reg) {
  emit_genins_imm_reg(compiler, "orq", reg_to_str, imm, reg);
}

void emit_orq_imm_var(compiler_t *compiler, size_t imm, size_t var) {
  emit_genins_imm_var(compiler, "orq", reg_to_str, imm, var);
}

void emit_shlq_imm_reg(compiler_t *compiler, size_t imm, enum reg reg) {
  emit_genins_imm_reg(compiler, "shlq", reg_to_str, imm, reg);
}

void emit_shlb_imm_reg(compiler_t *compiler, size_t imm, enum reg reg) {
  emit_genins_imm_reg(compiler, "shlb", regb_to_str, imm, reg);
}

void emit_shlq_imm_regmem(compiler_t *compiler, size_t imm, size_t add,
                          enum reg mem_reg) {
  emit_genins_imm_regmem(compiler, "shlq", imm, add, mem_reg);
}

void emit_shlb_imm_regmem(compiler_t *compiler, size_t imm, size_t add,
                          enum reg mem_reg) {
  emit_genins_imm_regmem(compiler, "shlb", imm, add, mem_reg);
}

void emit_shlq_imm_var(compiler_t *compiler, size_t imm, size_t var) {
  emit_genins_imm_var(compiler, "shlq", reg_to_str, imm, var);
}

void emit_shlb_imm_var(compiler_t *compiler, size_t imm, size_t var) {
  emit_genins_imm_var(compiler, "shlb", regb_to_str, imm, var);
}

void emit_unary(compiler_t *compiler, const char *action, exprs_t args) {
  if (args.len == 1) {
    emit_expr(compiler, args.arr[0]);
    emit_str(compiler, action);
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

/// Binary with support for variable number of arguments
void emit_binary(compiler_t *compiler, const char *action, exprs_t args) {
  if (args.len >= 2) {
    size_t arg1 = get_unused_env(compiler->env);
    emit_store_expr(compiler, args.arr[1], arg1, 0, 0);
    emit_expr(compiler, args.arr[0]);
    emit_var_str(compiler, action, arg1);
    for (size_t i = 2; i < args.len; i++) {
      emit_store_expr(compiler, args.arr[i], arg1, 0, 0);
      emit_var_str(compiler, action, arg1);
    }
    remove_env(compiler->env, arg1);
  } else {
    errc(compiler, ExpectedBinary);
  }
}

/// TODO: Variable arguments please
void emit_comp(compiler_t *compiler, const char *action, exprs_t args) {
  if (args.len == 2) {
    size_t arg1 = get_unused_env(compiler->env);
    emit_store_expr(compiler, args.arr[1], arg1, 0, 0);
    emit_expr(compiler, args.arr[0]);
    emit_var_str(compiler, action, arg1);
    remove_env(compiler->env, arg1);
  } else {
    errc(compiler, ExpectedBinary);
  }
}

/// Specialized emit_binary for modulo
void emit_mod(compiler_t *compiler, exprs_t args) {
  if (args.len == 2) {
    size_t arg1 = get_unused_env(compiler->env);
    emit_store_expr(compiler, args.arr[1], arg1, 0, 0);
    emit_expr(compiler, args.arr[0]);
    if (compiler->env->arr[2].type) {
      size_t tmp = get_unused_env(compiler->env);
      emit_movq_reg_var(compiler, Rdx, tmp);
      emit_str(compiler, "cqto");
      emit_var_str(compiler, "idivq %s", arg1);
      emit_movq_reg_reg(compiler, Rdx, Rax);
      emit_movq_var_reg(compiler, tmp, Rdx);
      remove_env(compiler->env, tmp);
    } else {
      emit_str(compiler, "cqto");
      emit_var_str(compiler, "idivq %s", arg1);
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
      emit_str(compiler, action);
    }
    emit_size_str(compiler,
                  "cmpq $%zu, %%rax\nmovl $0, %%eax\nsete %%al\nshll "
                  "$7, %%eax\norl $31, %%eax",
                  constant);
  } else {
    errc(compiler, ExpectedUnary);
  }
}

int emit_load_bind(compiler_t *compiler, expr_t bind, size_t index,
                   size_t var_index, size_t use_var) {
  int err_code;
  if (bind.type == List) {
    if (bind.exprs->len == 2) {
      switch (bind.exprs->arr[0].type) {
      case Sym:
        emit_store_expr(compiler, bind.exprs->arr[1], index, var_index,
                        use_var);
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
      size_t reg = get_unused_env(compiler->env);
      push_var_env(compiler->env, strdup(binds->arr[i].exprs->arr[0].str),
                   Unknown, reg, 0);
      compiler->env->rarr[reg].variable = 1;
      if (!emit_load_bind(compiler, binds->arr[i], reg, compiler->env->len - 1,
                          1)) {
        return;
      }
      compiler->env->arr[compiler->env->len - 1].active = 1;
    }
    for (size_t i = 1; i < rest.len; i++) {
      emit_expr(compiler, rest.arr[i]);
    }
    for (size_t i = 0; i < binds->len; i++) {
      pop_var_env(compiler->env);
    }
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
      size_t reg = get_unused_env(compiler->env);
      push_var_env(compiler->env, strdup(binds->arr[i].exprs->arr[0].str),
                   Unknown, reg, 0);
      compiler->env->rarr[reg].variable = 1;
      if (!emit_load_bind(compiler, binds->arr[i], reg, compiler->env->len - 1,
                          1)) {
        return;
      }
    }
    for (size_t i = 0; i < binds->len; i++) {
      compiler->env->arr[compiler->env->len - 1].active = 1;
    }
    for (size_t i = 1; i < rest.len; i++) {
      emit_expr(compiler, rest.arr[i]);
    }
    for (size_t i = 0; i < binds->len; i++) {
      pop_var_env(compiler->env);
    }
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
    emit_size_str(compiler, "cmpq $31, %rax\nje L%zu", l0);
    emit_expr(compiler, rest.arr[1]);
    emit_size_str(compiler, "L%zu:", l0);
    compiler->ret_type = Unknown;
  } else if (rest.len == 3) {
    size_t l0 = compiler->label++;
    size_t l1 = compiler->label++;
    emit_expr(compiler, rest.arr[0]); // Test
    emit_size_str(compiler, "cmpq $31, %rax\nje L%zu", l0);
    emit_expr(compiler, rest.arr[1]);
    emit_size_str(compiler, "jmp L%zu", l1);
    emit_size_str(compiler, "L%zu:", l0);
    emit_expr(compiler, rest.arr[2]);
    emit_size_str(compiler, "L%zu:", l1);
    compiler->ret_type = Unknown;
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
    emit_store_expr(compiler, rest.arr[1], arg1, 0, 0);
    emit_expr(compiler, rest.arr[0]);
    emit_str(compiler, "movq gen0_ptr(%rip), %r14");
    emit_movq_reg_regmem(compiler, Rax, 0, R14);
    emit_movq_var_regmem(compiler, arg1, 8, R14);
    emit_movq_reg_reg(compiler, R14, Rax);
    emit_orq_imm_reg(compiler, 1, Rax);
    emit_str(compiler, "addq $16, gen0_ptr(%rip)");
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
    collect_retq(compiler, 8);
    size_t len = get_unused_env(compiler->env);
    emit_store_expr(compiler, rest.arr[0], len, 0, 0);
    emit_var_str(compiler, "movq gen0_ptr(%%rip), %%r14", len);
    emit_movq_var_regmem(compiler, len, 0, R14);
    emit_movq_reg_reg(compiler, R14, Rax);
    emit_orq_imm_reg(compiler, 2, Rax);
    emit_str(compiler, "movq gen0_ptr(%rip), %r14");
    emit_var_str(compiler, "leaq 8(%%r14,%s,2), %%r14", len);
    emit_str(compiler, "movq %r14, gen0_ptr(%rip)");
    remove_env(compiler->env, len);
    compiler->heap += 8;
  } else if (rest.len == 2) {
    emit_expr(compiler, rest.arr[0]);
    collect_retq(compiler, 8);
    size_t label = compiler->label++;
    size_t len = get_unused_env(compiler->env);
    size_t counter = get_unused_env(compiler->env);
    emit_store_expr(compiler, rest.arr[0], len, 0, 0);
    emit_expr(compiler, rest.arr[1]);
    emit_str(compiler, "movq gen0_ptr(%rip), %r14");
    emit_movq_var_regmem(compiler, len, 0, R14);
    emit_movq_var_var(compiler, len, counter);
    emit_var_str(compiler, "shr $2, %s", counter);
    emit_size_str(compiler, "L%zu:", label);
    emit_movq_reg_fullmem(compiler, Rax, 0, R14, counter + 1, 8);
    emit_decq_var(compiler, counter);
    emit_var_str(compiler, "cmpq $0, %s", counter);
    emit_size_str(compiler, "jne L%zu", label);
    emit_movq_reg_reg(compiler, R14, Rax);
    emit_orq_imm_reg(compiler, 2, Rax);
    emit_str(compiler, "movq gen0_ptr(%rip), %r14");
    emit_var_str(compiler, "leaq 8(%%r14,%s,2), %%r14", len);
    emit_str(compiler, "movq %r14, gen0_ptr(%rip)");
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

void emit_vector(compiler_t *compiler, exprs_t args) {
  exprs_t *arg_len = create_exprs(1);
  push_exprs(arg_len, (expr_t){.type = Num, .num = args.len});
  emit_mkvec(compiler, *arg_len);
  size_t obj = get_unused_env(compiler->env);
  for (size_t i = 0; i < args.len; i++) {
    emit_store_expr(compiler, args.arr[i], obj, 0, 0);
    emit_movq_var_regmem(compiler, obj, 6 + (i << 3), Rax);
  }
  remove_env(compiler->env, obj);
  compiler->ret_type = Vector;
}

void emit_vecref(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 2) {
    size_t loc = get_unused_env(compiler->env);
    emit_store_expr(compiler, rest.arr[1], loc, 0, 0);
    emit_expr(compiler, rest.arr[0]);
    emit_var_str(compiler, "movq 6(%%rax,%s,2), %%rax", loc);
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
    emit_store_expr(compiler, rest.arr[2], obj, 0, 0);
    size_t loc = get_unused_env(compiler->env);
    emit_store_expr(compiler, rest.arr[1], loc, 0, 0);
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

// PERF: Consider the case of a fixnum in the first argument, generates less
// noise
void emit_mkstr(compiler_t *compiler, int utf8, exprs_t rest) {
  if (rest.len == 1) {
    emit_expr(compiler, rest.arr[0]);
    emit_str(compiler, "shrq $2, %rax");
    collect_retb(compiler, 8);
    size_t len = get_unused_env(compiler->env);
    emit_store_expr(compiler, rest.arr[0], len, 0, 0);
    emit_var_str(compiler, "movq gen0_ptr(%%rip), %%r14", len);
    emit_var_str(compiler, "shlq $1, %s", len);
    if (utf8) {
      emit_var_str(compiler, "orq $1, %s", len);
    }
    emit_movq_var_regmem(compiler, len, 0, R14);
    emit_movq_reg_reg(compiler, R14, Rax);
    emit_orq_imm_reg(compiler, 3, Rax);
    emit_str(compiler, "movq gen0_ptr(%rip), %r14");
    emit_var_str(compiler, "leaq 8(%%r14,%s), %%r14", len);
    emit_str(compiler, "movq %r14, gen0_ptr(%rip)");
    remove_env(compiler->env, len);
    compiler->heap += 8;
  } else if (rest.len == 2) {
    emit_expr(compiler, rest.arr[0]);
    emit_str(compiler, "shrq $2, %rax");
    collect_retb(compiler, 8);
    size_t label = compiler->label++;
    size_t len = get_unused_env(compiler->env);
    size_t counter = get_unused_env(compiler->env);
    emit_store_expr(compiler, rest.arr[0], len, 0, 0);
    emit_expr(compiler, rest.arr[1]);
    emit_str(compiler, "movq gen0_ptr(%rip), %r14");
    emit_var_str(compiler, "shlq $1, %s", len);
    if (utf8) {
      emit_var_str(compiler, "orq $1, %s", len);
    }
    emit_movq_var_regmem(compiler, len, 0, R14);
    emit_movq_var_var(compiler, len, counter);
    emit_var_str(compiler, "shr $3, %s", counter);
    emit_size_str(compiler, "L%zu:", label);
    emit_movb_reg_fullmem(compiler, Rax, 0, R14, counter + 1, 1);
    emit_decq_var(compiler, counter);
    emit_var_str(compiler, "cmpq $0, %s", counter);
    emit_size_str(compiler, "jne L%zu", label);
    emit_movq_reg_reg(compiler, R14, Rax);
    emit_orq_imm_reg(compiler, 3, Rax);
    emit_str(compiler, "movq gen0_ptr(%rip), %r14");
    emit_var_str(compiler, "leaq 8(%%r14,%s), %%r14", len);
    emit_str(compiler, "movq %r14, gen0_ptr(%rip)");
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

void emit_string_c(compiler_t *compiler, const char *cstr) {
  exprs_t *arg_len = create_exprs(1);
  push_exprs(arg_len, (expr_t){.type = Num, .num = strlen(cstr)});
  int utf8 = is_utf8(cstr);
  emit_mkstr(compiler, utf8, *arg_len);
  size_t obj = get_unused_env(compiler->env);
  for (size_t i = 0; i < strlen(cstr); i++) {
    emit_movq_imm_var(compiler, cstr[i], obj);
    emit_movb_var_regmem(compiler, obj, 5 + i, Rax);
  }
  remove_env(compiler->env, obj);
  if (utf8) {
    compiler->ret_type = UniString;
  } else {
    compiler->ret_type = String;
  }
}

void emit_string(compiler_t *compiler, exprs_t args) {
  exprs_t *arg_len = create_exprs(1);
  push_exprs(arg_len, (expr_t){.type = Num, .num = args.len});
  emit_mkstr(compiler, 0, *arg_len);
  size_t obj = get_unused_env(compiler->env);
  int utf8 = 0;
  for (size_t i = 0; i < args.len; i++) {
    emit_store_expr(compiler, args.arr[i], obj, 0, 0);
    if (!utf8 && compiler->env->arr[obj].type == UniChar) {
      emit_str(compiler, "orq $1, -3(%rax)");
      utf8 = 1;
    }
    emit_var_str(compiler, "shrq $8, %s", obj);
    emit_movb_var_regmem(compiler, obj, 5 + i, Rax);
  }
  remove_env(compiler->env, obj);
  if (utf8) {
    compiler->ret_type = UniString;
  } else {
    compiler->ret_type = String;
  }
}

void emit_strlen(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 1) {
    emit_expr(compiler, rest.arr[0]);
    if (compiler->ret_type == UniString) {
      size_t obj = get_unused_env(compiler->env);
      size_t len = get_unused_env(compiler->env);
      size_t tmp = get_unused_env(compiler->env);
      size_t l0 = compiler->label++;
      size_t l1 = compiler->label++;
      size_t l2 = compiler->label++;
      emit_movq_reg_var(compiler, Rax, obj);
      emit_movq_regmem_reg(compiler, -3, Rax, Rax);
      emit_movq_reg_var(compiler, Rax, len);
      emit_var_str(compiler, "shrq $3, %s", len);
      emit_movq_imm_reg(compiler, -1, Rax);
      emit_size_str(compiler, "L%zu:", l0);
      emit_str(compiler, "incq %rax");
      emit_size_str(compiler, "L%zu:", l1);
      emit_var_str(compiler, "cmpq $0, %s", len);
      emit_size_str(compiler, "je L%zu", l2);
      emit_decq_var(compiler, len);
      emit_movb_fullmem_var(compiler, 5, obj + 1, len + 1, 1, tmp);
      // NOTE: This looks fun, but it was tested on a pretty old hardware
      emit_shlb_imm_var(compiler, 1, tmp);
      emit_size_str(compiler, "js L%zu", l0);
      emit_size_str(compiler, "jc L%zu", l1);
      emit_size_str(compiler, "jmp L%zu", l0);
      emit_size_str(compiler, "L%zu:", l2);
      emit_str(compiler, "shlq $2, %rax");
      remove_env(compiler->env, tmp);
      remove_env(compiler->env, len);
      remove_env(compiler->env, obj);
      return;
    } else if (compiler->ret_type == String) {
      emit_movq_regmem_reg(compiler, -3, Rax, Rax);
      emit_str(compiler, "shrq $1, %rax");
      return;
    }
    size_t obj = get_unused_env(compiler->env);
    size_t len = get_unused_env(compiler->env);
    size_t count = get_unused_env(compiler->env);
    size_t l0 = compiler->label++;
    size_t l1 = compiler->label++;
    size_t l2 = compiler->label++;
    size_t l3 = compiler->label++;
    size_t l4 = compiler->label++;
    emit_movq_reg_var(compiler, Rax, obj);
    emit_movq_regmem_reg(compiler, -3, Rax, Rax);
    emit_movq_reg_var(compiler, Rax, len);
    emit_str(compiler, "andq $1, %rax\ncmpq $0, %rax");
    emit_size_str(compiler, "je L%zu", l0);
    // >ascii, so do utf8_strlen
    emit_var_str(compiler, "shrq $3, %s", len);
    emit_var_str(compiler, "movq $-1, %s", count);
    emit_size_str(compiler, "L%zu:", l3);
    emit_var_str(compiler, "incq %s", count);
    emit_size_str(compiler, "L%zu:", l2);
    emit_var_str(compiler, "cmpq $0, %s", len);
    emit_size_str(compiler, "je L%zu", l4);
    emit_decq_var(compiler, len);
    emit_movb_fullmem_reg(compiler, 5, obj + 1, len + 1, 1, Rax);
    emit_str(compiler, "shlb $1, %al");
    emit_size_str(compiler, "js L%zu", l3);
    emit_size_str(compiler, "jc L%zu", l2);
    emit_size_str(compiler, "jmp L%zu", l3);
    emit_size_str(compiler, "L%zu:", l4);
    emit_movq_var_reg(compiler, count, Rax);
    emit_str(compiler, "shlq $2, %rax");
    emit_size_str(compiler, "jmp L%zu", l1);
    // ascii, so just returns chars
    emit_size_str(compiler, "L%zu:", l0);
    emit_var_str(compiler, "shrq $1, %s", len);
    emit_movq_var_reg(compiler, len, Rax);
    emit_size_str(compiler, "L%zu:", l1);
    remove_env(compiler->env, count);
    remove_env(compiler->env, len);
    remove_env(compiler->env, obj);
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedUnary);
  }
}

void emit_unistrref(compiler_t *compiler, size_t obj, size_t loc) {
  // Navigate to the code point
  size_t tmp = get_unused_env(compiler->env);
  size_t l0 = compiler->label++;
  size_t l1 = compiler->label++;
  emit_var_str(compiler, "shrq $2, %s", loc);
  emit_movq_imm_reg(compiler, -1, Rax);
  emit_size_str(compiler, "L%zu:", l0);
  emit_incq_reg(compiler, Rax);
  emit_movb_fullmem_var(compiler, 5, obj + 1, Rax, 1, tmp);
  emit_shlb_imm_var(compiler, 1, tmp);
  emit_size_str(compiler, "js L%zu", l1);
  emit_size_str(compiler, "jc L%zu", l0);
  emit_size_str(compiler, "L%zu:", l1);
  emit_decq_var(compiler, loc);
  emit_var_str(compiler, "cmpq $0, %s", loc);
  emit_size_str(compiler, "jge L%zu", l0);
  // Get it and convert to Char
  size_t l2 = compiler->label++;
  size_t l3 = compiler->label++;
  size_t l4 = compiler->label++;
  size_t l5 = compiler->label++;
  emit_movb_fullmem_var(compiler, 5, obj + 1, Rax, 1, tmp);
  emit_shlb_imm_var(compiler, 1, tmp);
  emit_size_str(compiler, "js L%zu", l2);
  emit_movb_fullmem_reg(compiler, 5, obj + 1, Rax, 1, Rax);
  emit_size_str(compiler, "jmp L%zu", l5);
  emit_size_str(compiler, "L%zu:", l2);
  emit_shlb_imm_var(compiler, 1, tmp);
  emit_size_str(compiler, "js L%zu", l3);
  emit_movw_fullmem_reg(compiler, 5, obj + 1, Rax, 1, Rax);
  emit_size_str(compiler, "jmp L%zu", l5);
  emit_size_str(compiler, "L%zu:", l3);
  emit_shlb_imm_var(compiler, 1, tmp);
  emit_size_str(compiler, "js L%zu", l4);
  emit_movl_fullmem_reg(compiler, 5, obj + 1, Rax, 1, Rax);
  emit_genins_imm_reg(compiler, "andl", regl_to_str, 0x00ffffff, Rax);
  emit_size_str(compiler, "jmp L%zu", l5);
  emit_size_str(compiler, "L%zu:", l4);
  emit_movl_fullmem_reg(compiler, 5, obj + 1, Rax, 1, Rax);
  emit_size_str(compiler, "L%zu:", l5);
  remove_env(compiler->env, tmp);
}

void emit_strref(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 2) {
    size_t obj = get_unused_env(compiler->env);
    size_t loc = get_unused_env(compiler->env);
    emit_store_expr(compiler, rest.arr[0], obj, 0, 0);
    emit_store_expr(compiler, rest.arr[1], loc, 0, 0);
    if (compiler->env->arr[obj].type == UniString) {
      emit_unistrref(compiler, obj, loc);
      emit_shlq_imm_reg(compiler, 8, Rax);
      emit_orq_imm_reg(compiler, 15, Rax);
      remove_env(compiler->env, loc);
      remove_env(compiler->env, obj);
      compiler->ret_type = UniChar;
      return;
    } else if (compiler->env->arr[obj].type == String) {
      emit_var_str(compiler, "shrq $2, %s", loc);
      emit_str(compiler, "xorl %eax, %eax");
      emit_movb_fullmem_reg(compiler, 5, obj + 1, loc + 1, 1, Rax);
      emit_shlq_imm_reg(compiler, 8, Rax);
      emit_orq_imm_reg(compiler, 15, Rax);
      remove_env(compiler->env, loc);
      remove_env(compiler->env, obj);
      compiler->ret_type = Char;
      return;
    }
    size_t l0 = compiler->label++;
    size_t l1 = compiler->label++;
    emit_movq_regmem_reg(compiler, -3, Rax, Rax);
    emit_str(compiler, "andq $1, %rax\ncmpq $0, %rax");
    emit_size_str(compiler, "je L%zu", l0);
    emit_unistrref(compiler, obj, loc);
    emit_size_str(compiler, "jmp L%zu", l1);
    emit_size_str(compiler, "L%zu:", l0);
    emit_var_str(compiler, "shrq $2, %s", loc);
    emit_str(compiler, "xorl %eax, %eax");
    emit_movb_fullmem_reg(compiler, 5, obj + 1, loc + 1, 1, Rax);
    emit_size_str(compiler, "L%zu:", l1);
    emit_shlq_imm_reg(compiler, 8, Rax);
    emit_orq_imm_reg(compiler, 15, Rax);
    remove_env(compiler->env, loc);
    remove_env(compiler->env, obj);
    compiler->ret_type = UniChar;
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedBinary);
  }
}

// TODO: THIS IS SIMPLIFIED | HEAVY WIP
void emit_strset(compiler_t *compiler, exprs_t rest) {
  // NOTE: Current plan is to reallocate string if it does not fit
  if (rest.len == 3) {
    if (compiler->ret_type == UniString) {
      // TODO: Write it
      return;
    } else if (compiler->ret_type == String) {
      size_t obj = get_unused_env(compiler->env);
      emit_store_expr(compiler, rest.arr[2], obj, 0, 0);
      // TODO: Implement reallocation
      if (compiler->env->arr[obj].type == UniChar) {
        compiler->line = rest.arr[0].line;
        compiler->loc = rest.arr[0].loc;
        errc(compiler, ExpectedNonUniChar);
        return;
      }
      size_t loc = get_unused_env(compiler->env);
      emit_store_expr(compiler, rest.arr[1], loc, 0, 0);
      emit_expr(compiler, rest.arr[0]);
      emit_var_str(compiler, "shrq $2, %s", loc);
      emit_movq_var_fullmem(compiler, obj, 5, Rax, loc + 1, 1);
      remove_env(compiler->env, obj);
      remove_env(compiler->env, loc);
      return;
    }
    // Branch to either uni or str
    // TODO: Write it
  } else {
    compiler->line = rest.arr[0].line;
    compiler->loc = rest.arr[0].loc;
    errc(compiler, ExpectedTrinary);
  }
}

void emit_cdrset(compiler_t *compiler, exprs_t rest) {
  if (rest.len == 2) {
    size_t obj = get_unused_env(compiler->env);
    emit_store_expr(compiler, rest.arr[1], obj, 0, 0);
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
    emit_store_expr(compiler, rest.arr[1], obj, 0, 0);
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
    ssize_t found = rfind_active_var_env(compiler->env, rest.arr[0].str);
    if (found == -1) {
      if (compiler->free) {
        found = find_free(compiler->free, rest.arr[0].str);
        if (found == -1) {
          found = compiler->free->len;
          push_free(compiler->free, strdup(rest.arr[0].str), 1);
        } else {
          compiler->free->arr[found].boxed = 1;
        }
        size_t tmp = get_unused_env(compiler->env);
        emit_movq_regmem_var(compiler, found * 8 + 10, R13, tmp);
        emit_movq_reg_regmem(compiler, Rax, 0, tmp + 1);
        remove_env(compiler->env, tmp);
      } else {
        errc(compiler, UndefinedSymb);
      }
    } else {
      emit_movq_reg_var(compiler, Rax, compiler->env->arr[found].idx);
    }
    compiler->env->rarr[compiler->env->arr[found].idx].type =
        compiler->ret_type;
    compiler->env->arr[found].type = compiler->ret_type;
    if (compiler->ret_args) {
      compiler->env->arr[found].args = clone_exprs(compiler->ret_args);
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
    emit_store_expr(compiler, rest.arr[order[i]], order[i], 0, 0);
  }
  free(order);
  delete_bitmat(bitmat);
}

void emit_box(compiler_t *compiler, size_t var) {
  collect(compiler, 8);
  emit_str(compiler, "movq gen0_ptr(%rip), %r14");
  emit_movq_var_regmem(compiler, var, 0, R14);
  emit_movq_reg_var(compiler, R14, var);
  emit_str(compiler, "addq $8, gen0_ptr(%rip)");
  compiler->heap += 8;
}

void emit_closure(compiler_t *compiler, size_t lamb, size_t arity,
                  size_t *free_vars, size_t free_len) {
  size_t boxes = 0;
  for (size_t i = 0; i < free_len; i++) {
    if (free_vars[i] & 1) {
      boxes++;
    }
  }
  collect(compiler, free_len * 8 + 16 + boxes * 8);
  emit_str(compiler, "movq gen0_ptr(%rip), %r14");
  for (size_t i = 0, j = 0; i < free_len; i++) {
    if (free_vars[i] & 1) {
      emit_movq_var_regmem(compiler, compiler->env->arr[free_vars[i] >> 1].idx,
                           j * 8, R14);
      emit_movq_reg_var(compiler, R14,
                        compiler->env->arr[free_vars[i] >> 1].idx);
      j++;
    }
  }
  emit_size_str(compiler, "addq $%zu, gen0_ptr(%rip)", boxes * 8);
  emit_size_str(compiler, "movq gen0_ptr(%%rip), %%r14\nmovq $%zu, (%%r14)",
                arity);
  size_t tmp = get_unused_env(compiler->env);
  emit_leaq_label_var(compiler, "lambda", lamb, tmp);
  emit_movq_var_regmem(compiler, tmp, 8, R14);
  remove_env(compiler->env, tmp);
  for (size_t i = 0; i < free_len; i++) {
    emit_movq_var_regmem(compiler, compiler->env->arr[free_vars[i] >> 1].idx,
                         i * 8 + 16, R14);
  }
  emit_str(compiler, "movq %r14, %rax\norq $6, %rax");
  emit_size_str(compiler, "addq $%zu, gen0_ptr(%rip)", free_len * 8 + 16);
  compiler->heap += free_len * 8 + 16 + boxes * 8;
}

void emit_tail_call(compiler_t *compiler, exprs_t args, exprs_t rest) {
  solve_call_order(compiler, args, rest);
  if (compiler->env->len > compiler->env->stack_offset + 1) {
    emit_size_str(compiler, "addq $%zu, %%rsp",
                  (compiler->env->len - compiler->env->stack_offset) * 8);
  }
  emit_str(compiler, "movq 2(%r13), %rax");
  emit_str(compiler, "jmp *%rax");
}

void try_emit_tail_if(compiler_t *compiler, const char *name, exprs_t args,
                      exprs_t rest) {
  if (rest.len == 2) {
    size_t l0 = compiler->label++;
    emit_expr(compiler, rest.arr[0]); // Test
    emit_size_str(compiler, "cmpq $31, %rax\nje L%zu", l0);
    try_emit_tail_call(compiler, name, args, rest.arr[1]);
    emit_size_str(compiler, "L%zu:", l0);
  } else if (rest.len == 3) {
    size_t l0 = compiler->label++;
    size_t l1 = compiler->label++;
    emit_expr(compiler, rest.arr[0]); // Test
    emit_size_str(compiler, "cmpq $31, %rax\nje L%zu", l0);
    try_emit_tail_call(compiler, name, args, rest.arr[1]);
    emit_size_str(compiler, "jmp L%zu", l1);
    emit_size_str(compiler, "L%zu:", l0);
    try_emit_tail_call(compiler, name, args, rest.arr[2]);
    emit_size_str(compiler, "L%zu:", l1);
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
        // Also try stuff like last of `let`, `begin`, etc.
      } else if (!strcmp(last.exprs->arr[0].str, "if")) {
        try_emit_tail_if(compiler, name, args,
                         slice_start_exprs(last.exprs, 1));
        return;
      }
    }
  }
  emit_expr(compiler, last);
}

// NOTE: Potentially move this logic to emit_constants, by marking what is
// free and supposed to be boxed
void find_and_fill_boxes(compiler_t *compiler, exprs_t rest) {
  exprs_t *all_sets = find_all_symb_exprs(&rest, "set!");
  if (!all_sets) {
    return;
  }
  for (size_t i = 0; i < all_sets->len; i++) {
    char *free_var = all_sets->arr[i].exprs->arr[1].str;
    ssize_t found = find_active_var_env(compiler->env, free_var);
    if (found == -1) {
      found = find_free(compiler->free, free_var);
      if (found == -1) {
        found = compiler->free->len;
        push_free(compiler->free, strdup(free_var), 1);
      } else {
        compiler->free->arr[found].boxed = 1;
      }
    }
  }
  delete_exprs(all_sets);
}

// NOTE: Consider rewriting
void emit_lambda(compiler_t *compiler, const char *name, exprs_t rest) {
  int err_code = ExpectedAtLeastBinary;
  if (rest.len > 1) {
    if (rest.arr[0].type == List) {
      env_t *saved_env = compiler->env;
      free_t *saved_free = compiler->free;
      enum emit saved_emit = compiler->emit;
      compiler->env = create_full_from_const_env(saved_env, 8, 3, 3, 0, 4);
      compiler->free = create_free(4);
      if (name) {
        ssize_t recurse = find_active_var_env(saved_env, name);
        if (recurse != -1) {
          push_var_env(compiler->env, strdup(name), Lambda, -1, 0);
          compiler->env->arr[compiler->env->len - 1].active = 1;
          compiler->env->arr[compiler->env->len - 1].args =
              clone_exprs(saved_env->arr[recurse].args);
        }
      }
      for (size_t i = 0; i < rest.arr[0].exprs->len; i++) {
        if (rest.arr[0].exprs->arr[i].type == Sym) {
          size_t reg = get_unused_env(compiler->env);
          compiler->env->rarr[reg].variable = 1;
          compiler->env->rarr[reg].type = Unknown;
          push_var_env(compiler->env, strdup(rest.arr[0].exprs->arr[i].str),
                       Unknown, reg, 0);
          compiler->env->arr[compiler->env->len - 1].active = 1;
        } else {
          err_code = ExpectedSymb;
          goto LambErr;
        }
      }
      // HACK: Locks are heavily and sickly abused by current
      // implementation, think of a better way to buffer
      lock_dstrs(compiler->fun);
      compiler->emit = Fun;
      emit_size_str(compiler, "lambda%zu:", compiler->lambda);
      size_t lamb = compiler->lambda;
      compiler->lambda++;
      lock_dstrs(compiler->fun);
      find_and_fill_boxes(compiler, rest);
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
      if (compiler->env->len > compiler->env->stack_offset) {
        emit_size_str(compiler, "addq $%zu, %%rsp",
                      (compiler->env->len - compiler->env->stack_offset) * 8);
      }
      emit_str(compiler, "retq");
      unlock_dstrs(compiler->fun);
      if (compiler->env->len > compiler->env->stack_offset + 1) {
        emit_size_str(compiler, "subq $%zu, %%rsp",
                      (compiler->env->len - compiler->env->stack_offset) * 8);
      }
      unlock_dstrs(compiler->fun);
      delete_env(compiler->env);
      compiler->env = saved_env;
      size_t free_len = compiler->free->len;
      size_t *free_vars = calloc(sizeof(*free_vars), free_len);
      for (size_t i = 0; i < free_len; i++) {
        ssize_t found =
            rfind_active_var_env(compiler->env, compiler->free->arr[i].str);
        if (found == -1) {
          err_code = UndefinedSymb;
          goto LambErr;
        } else {
          if (compiler->free->arr[i].boxed) {
            compiler->env->arr[found].type = BoxUnknown;
            free_vars[i] = (found << 1) | 1;
          } else {
            free_vars[i] = (found << 1);
          }
        }
      }
      delete_free(compiler->free);
      compiler->free = saved_free;
      compiler->emit = saved_emit;
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

void emit_var_define(compiler_t *compiler, exprs_t rest, ssize_t postn) {
  ssize_t found = find_var_postn_env(compiler->env, rest.arr[0].str, postn);
  if (found == -1) {
    errc(compiler, UndefinedSymb);
  }
  if (compiler->env->arr[found].constant) {
    if (!compiler->env->arr[found].active) {
      compiler->env->arr[found].active = 1;
    } else {
      emit_var_define(compiler, rest, found);
    }
  } else {
    if (compiler->env->arr[found].active) {
      if (compiler->env->arr[found].idx != -1) {
        remove_env(compiler->env, compiler->env->arr[found].idx);
      }
    } else {
      compiler->env->arr[found].active = 1;
    }
    compiler->env->arr[found].constant = 0;

    size_t reg = get_unused_postn_env(compiler->env, 6);
    compiler->env->arr[found].idx = reg;
    emit_load_bind(compiler, (expr_t){.type = List, .exprs = &rest}, reg, found,
                   1);
    compiler->env->rarr[reg].variable = 1;
  }
}

void emit_define(compiler_t *compiler, exprs_t rest) {
  if (rest.arr[0].type == List) {
    if (rest.len >= 2) {
      ssize_t found =
          find_var_env(compiler->env, rest.arr[0].exprs->arr[0].str);
      if (found == -1) {
        errc(compiler, UndefinedSymb);
      }
      if (compiler->env->arr[found].active) {
        if (compiler->env->arr[found].idx != -1) {
          remove_env(compiler->env, compiler->env->arr[found].idx);
        }
      } else {
        compiler->env->arr[found].active = 1;
      }

      exprs_t *tmp = create_exprs(2);
      exprs_t slice = slice_start_exprs(rest.arr[0].exprs, 1);
      push_exprs(tmp, (expr_t){.type = List, .exprs = &slice});
      for (size_t i = 1; i < rest.len; i++) {
        push_exprs(tmp, clone_expr(rest.arr[i]));
      }
      emit_lambda(compiler, rest.arr[0].exprs->arr[0].str, *tmp);
      compiler->ret_type = Lambda;
      free(tmp);

      size_t reg = get_unused_postn_env(compiler->env, 6);
      emit_movq_reg_var(compiler, Rax, reg);
      compiler->env->rarr[reg].type = Lambda;
      compiler->env->rarr[reg].variable = 1;
      compiler->env->arr[found].idx = reg;
    } else {
      compiler->line = rest.arr[0].line;
      compiler->loc = rest.arr[0].loc;
      errc(compiler, ExpectedAtLeastBinary);
    }
  } else {
    if (rest.len == 2) {
      emit_var_define(compiler, rest, -1);
    } else {
      compiler->line = rest.arr[0].line;
      compiler->loc = rest.arr[0].loc;
      errc(compiler, ExpectedBinary);
    }
  }
}

void emit_ufun(compiler_t *compiler, const char *str, exprs_t rest) {
  ssize_t found = rfind_active_var_env(compiler->env, str);
  if (found == -1 && compiler->free) {
    // NOTE: This is the worst case. Gates have fallen.
    // We cannot tell if we are looking at a lamb or a number.
    found = find_free(compiler->free, str);
    if (found == -1) {
      found = compiler->free->len;
      push_free(compiler->free, strdup(str), 0);
    }
    // if (found + 1 != R13) {
    //  emit_movq_var_reg(compiler, found, R13);
    //}
    size_t l0 = compiler->label++;
    emit_size_str(compiler, "movq %zu(%%r13), %%rax", found * 8 + 10);
    emit_str(compiler, "pushq %r13\nmovq %rax, %r13");
    emit_str(compiler, "movq -6(%r13), %rax");
    emit_size_str(compiler, "cmpq $%zu, %%rax", rest.len);
    emit_size_str(compiler, "jne L%zu", l0);
    size_t a_count = spill_args(compiler);
    // TODO: Try to order this if possible at all, since otherwise (f (n-1) n)
    // will always cause unexpected behaviour.
    // Worst case have to reassign and save all variables to guarantee proper
    // result.
    for (size_t i = 0; i < rest.len; i++) {
      size_t new = reassign_postn_env(compiler->env, i, rest.len);
      emit_movq_reg_reg(compiler, i + 1, new + 1);
      emit_store_expr(compiler, rest.arr[i], i, 0, 0);
    }
    emit_str(compiler, "movq 2(%r13), %rax");
    emit_str(compiler, "callq *%rax");
    reorganize_args(compiler, a_count);
    emit_size_str(compiler, "L%zu:", l0);
    emit_str(compiler, "popq %r13");
  } else if (compiler->env->arr[found].type == Lambda) {
    if (compiler->env->arr[found].args->len == rest.len) {
      if (compiler->env->arr[found].idx != -1 &&
          compiler->env->arr[found].idx + 1 != R13) {
        emit_movq_var_reg(compiler, compiler->env->arr[found].idx, R13);
      }
      size_t a_count = spill_args(compiler);
      solve_call_order(compiler, *compiler->env->arr[found].args, rest);
      emit_str(compiler, "movq 2(%r13), %rax");
      emit_str(compiler, "callq *%rax");
      reorganize_args(compiler, a_count);
    } else {
      errc(compiler, ExpectedNoArg + compiler->env->arr[found].args->len);
    }
  } else if (compiler->free) {
    emit_str(compiler, "pushq %r13");
    emit_movq_var_reg(compiler, compiler->env->arr[found].idx, R13);
    size_t a_count = spill_args(compiler);
    solve_call_order(compiler, *compiler->env->arr[found].args, rest);
    emit_str(compiler, "movq 2(%r13), %rax");
    emit_str(compiler, "callq *%rax");
    reorganize_args(compiler, a_count);
    emit_str(compiler, "popq %r13");
  } else {
    errc(compiler, UnmatchedFun);
  }
}

void emit_function(compiler_t *compiler, expr_t first, exprs_t rest) {
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
        // Deal with the Devil
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
        emit_comp(compiler,
                  "cmpq %s, %%rax\nmovl $0, %%eax\nsete %%al\nshll $7, "
                  "%%eax\norl $31, %%eax",
                  rest);
        compiler->ret_type = Boolean;
      } else
        goto Unmatched;
      break;
    case '>':
      if (!strcmp(first.str, ">")) {
        emit_comp(compiler,
                  "cmpq %s, %%rax\nmovl $0, %%eax\nsetg %%al\nshll $7, "
                  "%%eax\norl $31, %%eax",
                  rest);
        compiler->ret_type = Boolean;
      } else if (!strcmp(first.str, ">=")) {
        emit_comp(compiler,
                  "cmpq %s, %%rax\nmovl $0, %%eax\nsetge %%al\nshll $7, "
                  "%%eax\norl $31, %%eax",
                  rest);
        compiler->ret_type = Boolean;
      } else
        goto Unmatched;
      break;
    case '<':
      if (!strcmp(first.str, "<")) {
        emit_comp(compiler,
                  "cmpq %s, %%rax\nmovl $0, %%eax\nsetl %%al\nshll $7, "
                  "%%eax\norl $31, %%eax",
                  rest);
        compiler->ret_type = Boolean;
      } else if (!strcmp(first.str, "<=")) {
        emit_comp(compiler,
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
        emit_str(compiler, "movq -1(%rax), %rax");
        compiler->ret_type = Unknown;
      } else if (!strcmp(first.str, "cadr")) {
        emit_unary(compiler, "movq 7(%rax), %rax", rest);
        emit_str(compiler, "movq -1(%rax), %rax");
        compiler->ret_type = Unknown;
      } else if (!strcmp(first.str, "cdar")) {
        emit_unary(compiler, "movq -1(%rax), %rax", rest);
        emit_str(compiler, "movq 7(%rax), %rax");
        compiler->ret_type = Unknown;
      } else if (!strcmp(first.str, "cddr")) {
        emit_unary(compiler, "movq 7(%rax), %rax", rest);
        emit_str(compiler, "movq 7(%rax), %rax");
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
        emit_str(compiler, "movq $0, %rdi\nmovq $60, %rax\nsyscall");
        compiler->ret_type = None;
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
        compiler->ret_type = None;
      } else if (!strcmp(first.str, "set-car!")) {
        emit_carset(compiler, rest);
        compiler->ret_type = None;
      } else if (!strcmp(first.str, "set-cdr!")) {
        emit_cdrset(compiler, rest);
        compiler->ret_type = None;
      } else if (!strcmp(first.str, "string")) {
        emit_string(compiler, rest);
        compiler->ret_type = Vector;
      } else if (!strcmp(first.str, "string?")) {
        emit_quest(compiler, "andl $7, %eax", 3, rest);
        compiler->ret_type = Boolean;
      } else if (!strcmp(first.str, "string-length")) {
        emit_strlen(compiler, rest);
        compiler->ret_type = Fixnum;
      } else if (!strcmp(first.str, "string-ref")) {
        emit_strref(compiler, rest);
      } else if (!strcmp(first.str, "string-set!")) {
        emit_strset(compiler, rest);
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
      } else if (!strcmp(first.str, "make-string")) {
        emit_mkstr(compiler, 0, rest);
        compiler->ret_type = Unknown;
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
        emit_vector(compiler, rest);
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
    emit_function(compiler, first.exprs->arr[0],
                  slice_start_exprs(first.exprs, 1));
    if (compiler->ret_type != Lambda) {
      errc(compiler, ExpectedFunSymb);
    }
    size_t l0 = compiler->label++;
    emit_str(compiler, "movq %rax, %r13");
    emit_str(compiler, "movq -6(%r13), %rax");
    emit_size_str(compiler, "cmpq $%zu, %%rax", rest.len);
    emit_size_str(compiler, "jne L%zu", l0);
    size_t a_count = spill_args(compiler);
    for (size_t i = 0; i < rest.len; i++) {
      emit_store_expr(compiler, rest.arr[i], i, 0, 0);
      compiler->env->arr[i].type = Unknown;
    }
    for (size_t i = 0; i < rest.len; i++) {
      compiler->env->arr[i].type = None;
    }
    emit_str(compiler, "movq 2(%r13), %rax");
    emit_str(compiler, "callq *%rax");
    reorganize_args(compiler, a_count);
    emit_size_str(compiler, "L%zu:", l0);
    break;
  default:
    errc(compiler, ExpectedFunSymb);
    break;
  }
}

void emit_store_symb_expr(compiler_t *compiler, const char *symb, size_t index,
                          size_t var_index, int use_var) {
  ssize_t found = rfind_active_var_env(compiler->env, symb);
  if (found == -1) {
    if (compiler->free) {
      found = find_free(compiler->free, symb);
      if (found == -1) {
        found = compiler->free->len;
        push_free(compiler->free, strdup(symb), 0);
      }
      emit_movq_regmem_var(compiler, found * 8 + 10, R13, index);
      compiler->env->rarr[index].type = compiler->env->arr[found].type;
      if (compiler->free->arr[found].boxed) {
        emit_movq_regmem_var(compiler, 0, index + 1, index);
        compiler->env->rarr[index].type = Unknown;
      }
    } else {
      errc(compiler, UndefinedSymb);
    }
  } else {
    if (compiler->env->arr[found].type >= BoxUnknown) {
      // Potentially Err properly here
      // if (compiler->env->arr[found].constant) {
      // errc(compiler, ExpectedConstant);
      //}
      emit_movq_regmem_var(compiler, 0, compiler->env->arr[found].idx, index);
      compiler->env->rarr[index].type =
          compiler->env->arr[found].type - BoxUnknown;
    } else {
      if (compiler->env->arr[found].constant) {
        emit_movq_const_var(compiler, compiler->env->arr[found].idx, index);
      } else {
        emit_movq_var_var(compiler, compiler->env->arr[found].idx, index);
        compiler->env->rarr[index].type = compiler->env->arr[found].type;
      }
    }
  }
  if (use_var && compiler->env->arr[found].type == Lambda &&
      compiler->env->arr[found].args) {
    compiler->env->arr[var_index].args =
        clone_exprs(compiler->env->arr[found].args);
  }
}

void emit_store_expr(compiler_t *compiler, expr_t expr, size_t index,
                     size_t var_index, int use_var) {
  compiler->line = expr.line;
  compiler->loc = expr.loc;
  switch (expr.type) {
  case Null:
    emit_movq_imm_var(compiler, tag_nil(), index);
    compiler->env->rarr[index].type = Nil;
    break;
  case Num:
    emit_movq_imm_var(compiler, tag_fixnum(expr.num), index);
    compiler->env->rarr[index].type = Fixnum;
    break;
  case Chr:
    emit_movq_imm_var(compiler, tag_char(expr.ch), index);
    compiler->env->rarr[index].type = Char;
    break;
  case UniChr:
    emit_movq_imm_var(compiler, tag_unichar(expr.uch), index);
    compiler->env->rarr[index].type = UniChar;
    break;
  case Bool:
    emit_movq_imm_var(compiler, tag_bool(expr.ch), index);
    compiler->env->rarr[index].type = Boolean;
    break;
  case Str:
    emit_string_c(compiler, expr.str);
    emit_var_str(compiler, "movq %%rax, %s", index);
    compiler->env->rarr[index].type = compiler->ret_type;
    break;
  case Sym:
    emit_store_symb_expr(compiler, expr.str, index, var_index, use_var);
    break;
  case List:
    emit_function(compiler, expr.exprs->arr[0],
                  slice_start_exprs(expr.exprs, 1));
    emit_var_str(compiler, "movq %%rax, %s", index);
    compiler->env->rarr[index].type = compiler->ret_type;
    if (use_var && compiler->ret_args) {
      compiler->env->arr[var_index].args = clone_exprs(compiler->ret_args);
    }
    break;
  case Vec:
    emit_vector(compiler, slice_start_exprs(expr.exprs, 0));
    emit_var_str(compiler, "movq %%rax, %s", index);
    compiler->env->rarr[index].type = compiler->ret_type;
    break;
  case Err:
    errc(compiler, ParserFailure);
    break;
  }
  if (use_var) {
    compiler->env->arr[var_index].type = compiler->env->rarr[index].type;
  }
}

void emit_symb_ret(compiler_t *compiler, const char *symb) {
  ssize_t found = rfind_active_var_env(compiler->env, symb);
  if (found == -1) {
    if (compiler->free) {
      found = find_free(compiler->free, symb);
      if (found == -1) {
        found = compiler->free->len;
        push_free(compiler->free, strdup(symb), 0);
      }
      emit_movq_regmem_reg(compiler, found * 8 + 10, R13, Rax);
      compiler->ret_type = Unknown; // compiler->env->arr[found].type;
      if (compiler->free->arr[found].boxed) {
        emit_movq_regmem_reg(compiler, 0, Rax, Rax);
        compiler->ret_type = Unknown;
      }
    } else {
      errc(compiler, UndefinedSymb);
    }
  } else {
    if (compiler->env->arr[found].type >= BoxUnknown) {
      // Potentially Err properly here
      // if (compiler->env->arr[found].constant) {
      // errc(compiler, ExpectedConstant);
      //}
      emit_movq_regmem_reg(compiler, 0, compiler->env->arr[found].idx + 1, Rax);
      compiler->ret_type = compiler->env->arr[found].type - BoxUnknown;
    } else {
      if (compiler->env->arr[found].constant) {
        emit_movq_const_reg(compiler, compiler->env->arr[found].idx, Rax);
      } else {
        emit_movq_var_reg(compiler, compiler->env->arr[found].idx, Rax);
        compiler->ret_type = compiler->env->arr[found].type;
      }
    }
  }
  if (compiler->env->arr[found].type == Lambda &&
      compiler->env->arr[found].args) {
    compiler->ret_args = clone_exprs(compiler->env->arr[found].args);
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
  case UniChr:
    emit_movq_imm_reg(compiler, tag_unichar(expr.uch), Rax);
    compiler->ret_type = UniChar;
    break;
  case Bool:
    emit_movq_imm_reg(compiler, tag_bool(expr.ch), Rax);
    compiler->ret_type = Boolean;
    break;
  case Str:
    emit_string_c(compiler, expr.str);
    break;
  case Sym:
    emit_symb_ret(compiler, expr.str);
    break;
  case List:
    emit_function(compiler, expr.exprs->arr[0],
                  slice_start_exprs(expr.exprs, 1));
    break;
  case Vec:
    emit_vector(compiler, slice_start_exprs(expr.exprs, 0));
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

/// Moves pointers into the root stack for the gc
size_t spill_pointers(compiler_t *compiler) {
  size_t count = 0;
  for (size_t i = 0; i < compiler->env->rlen; i++) {
    if (compiler->env->rarr[i].type >= Cons) {
      compiler->env->rarr[i].root_spill = 1;
      emit_movq_var_regmem(compiler, i, 0, R15);
      emit_str(compiler, "addq $8, %r15");
      count++;
    }
  }
  return count;
}

void reorganize_pointers(compiler_t *compiler, size_t count) {
  emit_str(compiler, "movq rs_begin(%rip), %r15");
  for (size_t i = 0, j = 0; i < compiler->env->rlen && j < count; i++) {
    if (compiler->env->rarr[i].root_spill) {
      compiler->env->rarr[i].root_spill = 0;
      emit_movq_regmem_var(compiler, j * 8, R15, i);
      j++;
    }
  }
}

/// NOTE: 6 is a magic number indicating when volatile passed arguments end
size_t spill_args(compiler_t *compiler) {
  size_t spilled = 0;
  for (size_t i = 0; i < 6 && i < compiler->env->rlen; i++) {
    if (compiler->env->rarr[i].type && !compiler->env->rarr[i].root_spill) {
      compiler->env->rarr[i].arg_spill = compiler->env->rarr[i].type;
      emit_var_str(compiler, "pushq %s", i);
      spilled++;
    }
  }
  return spilled;
}

void reorganize_args(compiler_t *compiler, size_t count) {
  for (size_t i = compiler->env->rlen, j = 0; j < count && i > 0; i--) {
    if (compiler->env->rarr[i - 1].arg_spill) {
      compiler->env->rarr[i - 1].type = compiler->env->rarr[i - 1].arg_spill;
      compiler->env->rarr[i - 1].arg_spill = 0;
      emit_var_str(compiler, "popq %s", i - 1);
      j++;
    }
  }
}

void collect(compiler_t *compiler, size_t request) {
  size_t p_count = spill_pointers(compiler);
  size_t a_count = spill_args(compiler);
  emit_size_str(compiler, "movq %%r15, %%rdi\nmovq $%zu, %%rsi\ncallq collect",
                request);
  reorganize_args(compiler, a_count);
  reorganize_pointers(compiler, p_count);
}

void collect_retq(compiler_t *compiler, size_t extra) {
  size_t p_count = spill_pointers(compiler);
  size_t a_count = spill_args(compiler);
  emit_size_str(compiler,
                "movq %%r15, %%rdi\nleaq %zu(,%rax,2), %%rsi\ncallq collect",
                extra);
  reorganize_args(compiler, a_count);
  reorganize_pointers(compiler, p_count);
}

void collect_retb(compiler_t *compiler, size_t extra) {
  size_t p_count = spill_pointers(compiler);
  size_t a_count = spill_args(compiler);
  emit_size_str(compiler,
                "movq %%r15, %%rdi\nleaq %zu(%rax), %%rsi\ncallq collect",
                extra);
  reorganize_args(compiler, a_count);
  reorganize_pointers(compiler, p_count);
}

void emit_start_end(compiler_t *compiler) {
  enum emit saved_emit = compiler->emit;
  compiler->emit = Main;
  emit_str(compiler, "main:");
  if (compiler->env->len > compiler->env->stack_offset) {
    emit_genins_imm_reg(compiler, "subq", reg_to_str,
                        (compiler->env->len - compiler->env->stack_offset) * 8,
                        Rsp);
  }
  if (compiler->heap) {
    emit_movq_imm_reg(compiler, compiler->heap_size, Rdi);
    emit_movq_imm_reg(compiler, compiler->heap_size, Rsi);
    emit_str(compiler, "callq init_gc");
    emit_str(compiler, "movq rs_begin(%rip), %r15");
  }
  compiler->emit = End;
  if (compiler->env->len > compiler->env->stack_offset) {
    emit_genins_imm_reg(compiler, "addq", reg_to_str,
                        (compiler->env->len - compiler->env->stack_offset) * 8,
                        Rsp);
  }
  emit_str(compiler, "movq %rax, %rdi");
  emit_str(compiler, "callq print");
  if (compiler->heap) {
    emit_str(compiler, "callq cleanup");
  }
  emit_str(compiler, "xorl %eax, %eax");
  compiler->emit = saved_emit;
}

void try_calc_var_type(int *result, expr_t expr) {
  switch (expr.type) {
  case Null: {
    result[0] = tag_nil();
    result[1] = Nil;
    break;
  }
  case Num: {
    result[0] = tag_fixnum(expr.num);
    result[1] = Fixnum;
    break;
  }
  case Chr: {
    result[0] = tag_char(expr.ch);
    result[1] = Char;
    break;
  }
  case UniChr: {
    result[0] = tag_unichar(expr.uch);
    result[1] = UniChar;
    break;
  }
  case Bool: {
    result[0] = tag_bool(expr.ch);
    result[1] = Boolean;
    break;
  }
  default:
    result[0] = 0;
    result[1] = Unknown;
  }
}

// TODO: Potetially do extra checks for sets in functions for boxing.
// This would require more var_t fields for all the new information gained.
// NOTE: This can be heavily expanded to include support for things like
// `let` as well.
// NOTE: Scheme does not actually have such a thing as defining constants.
// This is purely an optimization for cases like (define pi 3) to not take
// up a register or stack needlessly. However, it is essential for quotes,
// as they have to be unique.
void emit_constants(compiler_t *compiler) {
  // HACK: The most unoptimal way to do this.
  // The defines can be optimized to only check non-functions.
  // Should be a single pass into a single alloc not three passes and allocs.
  // Each pass is over the !entire! source code!.
  // This also clones everything, when we only need references
  exprs_t *all_defs = find_all_symb_exprs(compiler->input, "define");
  exprs_t *all_sets = find_all_symb_exprs(compiler->input, "set!");
  exprs_t *all_quotes = find_all_symb_exprs(compiler->input, "quote");

  // TODO: Error Rigor Needed
  if (all_defs) {
    compiler->emit = Data;
    size_t const_index = 0;
    int try_result[2] = {0, 0};
    if (all_sets) {
      for (size_t i = 0; i < all_defs->len; i++) {
        if (all_defs->arr[i].exprs->arr[1].type == Sym) {
          size_t len = strlen(all_defs->arr[i].exprs->arr[1].str);
          for (size_t j = 0; j < all_sets->len; j++) {
            if (all_sets->arr[j].exprs->arr[1].type == Sym) {
              if (!strncmp(all_sets->arr[j].exprs->arr[1].str,
                           all_defs->arr[i].exprs->arr[1].str, len)) {
                goto Mutable;
              }
            }
          }
          try_calc_var_type(try_result, all_defs->arr[i].exprs->arr[2]);
          if (try_result[1] != Unknown) {
            emit_genins_genlabel_imm(compiler, ".equ", "const", const_index,
                                     try_result[0]);
            push_var_env(compiler->env,
                         strdup(all_defs->arr[i].exprs->arr[1].str),
                         try_result[1], const_index, 1);
            const_index++;
          } else {
            // Consider vectors, lists, etc. mutable
            push_var_env(compiler->env,
                         strdup(all_defs->arr[i].exprs->arr[1].str), Unknown,
                         -1, 0);
          }
          continue;
        Mutable:
          push_var_env(compiler->env,
                       strdup(all_defs->arr[i].exprs->arr[1].str), Unknown, -1,
                       0);
        } else {
          // Function
          push_var_env(compiler->env,
                       strdup(all_defs->arr[i].exprs->arr[1].exprs->arr[0].str),
                       Lambda, -1, 0);
          compiler->env->arr[compiler->env->len - 1].args =
              slice_start_clone_exprs(all_defs->arr[i].exprs->arr[1].exprs, 1);
        }
      }
      delete_exprs(all_sets);
    } else {
      for (size_t i = 0; i < all_defs->len; i++) {
        if (all_defs->arr[i].exprs->arr[1].type == Sym) {
          try_calc_var_type(try_result, all_defs->arr[i].exprs->arr[2]);
          if (try_result[1] != Unknown) {
            emit_genins_genlabel_imm(compiler, ".equ", "const", const_index,
                                     try_result[0]);
            push_var_env(compiler->env,
                         strdup(all_defs->arr[i].exprs->arr[1].str),
                         try_result[1], const_index, 1);
            const_index++;
          } else {
            // Consider vectors, lists, etc. mutable
            push_var_env(compiler->env,
                         strdup(all_defs->arr[i].exprs->arr[1].str), Unknown,
                         -1, 0);
          }
        } else {
          // Function
          push_var_env(compiler->env,
                       strdup(all_defs->arr[i].exprs->arr[1].exprs->arr[0].str),
                       Lambda, -1, 0);
          exprs_t *args =
              slice_start_clone_exprs(all_defs->arr[i].exprs->arr[1].exprs, 1);
          if (args) {
            compiler->env->arr[compiler->env->len - 1].args = args;
          }
        }
      }
    }
    delete_exprs(all_defs);
  }

  // FANCY ALGO
  if (all_quotes)
    delete_exprs(all_quotes);
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

  // First Pass: PreCompute Constants/Quotes
  emit_constants(compiler);

  compiler->emit = Body;
  // Second/Final Pass: Trace and Emit Asm
  emit_exprs(compiler);
  emit_start_end(compiler);

  // Unite all the assembly code lines
  strs_t *fun_p = extract_main_dstrs(compiler->fun);
  const strs_t *ingredients[7] = {
      compiler->bss,    compiler->data, fun_p,        compiler->main,
      compiler->quotes, compiler->body, compiler->end};
  return union_clone_strs(ingredients, 7);
}
