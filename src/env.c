#include "env.h"
#include <err.h>
#include <malloc.h>
#include <string.h>

env_t *create_env(size_t vol_cap, size_t nvol_cap, size_t res_cap,
                  size_t stack_cap, size_t const_cap) {
  env_t *env = malloc(sizeof(*env));
  env->arr = calloc(sizeof(*env->arr), const_cap);
  env->len = 0;
  env->cap = const_cap;

  env->rarr =
      calloc(sizeof(*env->arr), vol_cap + nvol_cap + res_cap + stack_cap);
  env->rlen = 0;
  env->rcap = vol_cap + nvol_cap + res_cap + stack_cap;
  env->stack_offset = vol_cap + nvol_cap + res_cap;
  env->reserved_offset = vol_cap + nvol_cap;
  env->nonvol_offset = vol_cap;
  return env;
}

env_t *create_full_env(size_t vol_cap, size_t nvol_cap, size_t res_cap,
                       size_t stack_cap, size_t const_cap) {
  env_t *env = malloc(sizeof(*env));
  env->arr = calloc(sizeof(*env->arr), const_cap);
  env->len = 0;
  env->cap = const_cap;

  env->rarr =
      calloc(sizeof(*env->arr), vol_cap + nvol_cap + res_cap + stack_cap);
  env->rlen = vol_cap + nvol_cap + res_cap + stack_cap;
  env->rcap = vol_cap + nvol_cap + res_cap + stack_cap;
  env->stack_offset = vol_cap + nvol_cap + res_cap;
  env->reserved_offset = vol_cap + nvol_cap;
  env->nonvol_offset = vol_cap;
  return env;
}

env_t *create_full_from_const_env(env_t *constants, size_t vol_cap,
                                  size_t nvol_cap, size_t res_cap,
                                  size_t stack_cap, size_t const_cap) {
  env_t *env = malloc(sizeof(*env));
  env->arr = calloc(sizeof(*env->arr), const_cap);
  env->len = 0;
  env->cap = const_cap;

  env->rarr =
      calloc(sizeof(*env->arr), vol_cap + nvol_cap + res_cap + stack_cap);
  env->rlen = vol_cap + nvol_cap + res_cap + stack_cap;
  env->rcap = vol_cap + nvol_cap + res_cap + stack_cap;
  env->stack_offset = vol_cap + nvol_cap + res_cap;
  env->reserved_offset = vol_cap + nvol_cap;
  env->nonvol_offset = vol_cap;

  for (size_t i = 0; i < constants->len; i++) {
    if (constants->arr[i].active && constants->arr[i].var_type == Constant) {
      push_var_env(env, strdup(constants->arr[i].str),
                   constants->arr[i].val_type, constants->arr[i].idx, 1);
      env->arr[env->len - 1].active = 1;
    }
  }
  return env;
}

void delete_env(env_t *env) {
  for (size_t i = 0; i < env->len; i++) {
    free(env->arr[i].str);
    free(env->arr[i].args);
  }
  free(env->arr);
  free(env->rarr);
  free(env);
}

void push_env(env_t *env, enum val_type type, char var) {
  if (env->rlen >= env->rcap) {
    env->rcap <<= 1;
    env->rarr = reallocarray(env->rarr, env->rcap, sizeof(*env->rarr));
    if (!env->rarr) {
      err(1, "Failed to allocate memory for an array of registers in "
             "env");
    }
  }
  env->rarr[env->rlen].type = type;
  env->rarr[env->rlen].variable = var;
  env->rarr[env->rlen].root_spill = 0;
  env->rarr[env->rlen].arg_spill = 0;
  env->rlen++;
}

void insert_env(env_t *env, size_t i, enum val_type type, char var) {
  while (i > env->rlen) {
    push_env(env, 0, 0);
  }
  env->rarr[env->rlen].type = type;
  env->rarr[env->rlen].variable = var;
  env->rarr[env->rlen].root_spill = 0;
  env->rarr[env->rlen].arg_spill = 0;
  env->rlen++;
}

void remove_env(env_t *env, size_t i) {
  env->rarr[i].type = None;
  env->rarr[i].variable = 0;
  env->rarr[i].root_spill = 0;
  env->rarr[i].arg_spill = 0;
}

void push_var_env(env_t *env, char *str, enum val_type val_type, size_t idx,
                  enum var_type var_type) {
  if (env->len >= env->cap) {
    env->cap <<= 1;
    env->arr = reallocarray(env->arr, env->cap, sizeof(*env->arr));
    if (!env->arr) {
      err(1, "Failed to allocate memory for an array of variables in env");
    }
  }
  env->arr[env->len].str = str;
  env->arr[env->len].val_type = val_type;
  env->arr[env->len].var_type = var_type;
  env->arr[env->len].idx = idx;
  env->arr[env->len].active = 0;
  env->arr[env->len].args = 0;
  env->len++;
}

void pop_var_env(env_t *env) {
  env->len--;
  free(env->arr[env->len].str);
  if (env->arr[env->len].var_type != Constant && env->arr[env->len].idx != -1) {
    remove_env(env, env->arr[env->len].idx);
  }
}

void remove_var_env(env_t *env, size_t i) {
  free(env->arr[i].str);
  env->arr[i].str = 0;
  env->arr[i].val_type = None;
  env->arr[i].var_type = 0;
  env->arr[i].active = 0;
  env->arr[i].args = 0;
  env->arr[i].idx = -1;
}

ssize_t find_var_env(env_t *env, const char *str) {
  for (size_t i = 0; i < env->len; i++) {
    if (!strcmp(env->arr[i].str, str))
      return i;
  }
  return -1;
}

ssize_t find_var_postn_env(env_t *env, const char *str, size_t n) {
  for (size_t i = n + 1; i < env->len; i++) {
    if (!strcmp(env->arr[i].str, str))
      return i;
  }
  return -1;
}

ssize_t rfind_var_env(env_t *env, const char *str) {
  for (size_t i = env->len; i > 0; i--) {
    if (!strcmp(env->arr[i - 1].str, str))
      return i - 1;
  }
  return -1;
}

ssize_t find_active_var_env(env_t *env, const char *str) {
  for (size_t i = 0; i < env->len; i++) {
    if (env->arr[i].active && !strcmp(env->arr[i].str, str))
      return i;
  }
  return -1;
}

ssize_t rfind_active_var_env(env_t *env, const char *str) {
  for (size_t i = env->len; i > 0; i--) {
    if (env->arr[i - 1].active && !strcmp(env->arr[i - 1].str, str))
      return i - 1;
  }
  return -1;
}

size_t get_unused_env(env_t *env) {
  for (size_t i = 0; i < env->rlen; i++) {
    if (!env->rarr[i].type &&
        ((i < env->reserved_offset) || (i > env->stack_offset))) {
      env->rarr[i].type = Unknown;
      env->rarr[i].variable = 0;
      return i;
    }
  }
  push_env(env, Unknown, 0);
  return env->rlen - 1;
}

size_t get_unused_postn_env(env_t *env, size_t n) {
  while (n > env->rlen) {
    push_env(env, 0, 0);
  }
  for (size_t i = n; i < env->rlen; i++) {
    if (!env->rarr[i].type &&
        ((i < env->reserved_offset) || (i > env->stack_offset))) {
      env->rarr[i].type = Unknown;
      env->rarr[i].variable = 0;
      return i;
    }
  }
  push_env(env, Unknown, 0);
  return env->rlen - 1;
}

size_t get_unused_pren_env(env_t *env, size_t n) {
  while (n > env->rlen) {
    push_env(env, 0, 0);
  }
  for (size_t i = 0; i < env->reserved_offset && i < env->rlen; i++) {
    if (!env->rarr[i].type) {
      env->rarr[i].type = Unknown;
      env->rarr[i].variable = 0;
      return i << 1;
    }
  }
  return (env->nonvol_offset << 1) | 1;
}

size_t reassign_postn_env(env_t *env, size_t i, size_t n) {
  size_t new = get_unused_postn_env(env, n);
  for (size_t j = 0; j < env->len; j++) {
    if (env->arr[j].active && env->arr[j].var_type != Constant &&
        env->arr[j].idx == (ssize_t)i) {
      env->arr[j].idx = new;
    }
  }
  env->rarr[new].type = env->rarr[i].type;
  env->rarr[new].variable = env->rarr[i].variable;
  env->rarr[new].arg_spill = env->rarr[i].arg_spill;
  env->rarr[new].root_spill = env->rarr[i].root_spill;
  remove_env(env, i);
  return new;
}
