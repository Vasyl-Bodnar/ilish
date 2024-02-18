#include "env.h"
#include <err.h>
#include <malloc.h>
#include <string.h>

env_t *create_env(size_t vol_cap, size_t nvol_cap, size_t res_cap,
                  size_t stack_cap) {
  env_t *env = malloc(sizeof(*env));
  env->arr =
      calloc(sizeof(*env->arr), vol_cap + nvol_cap + res_cap + stack_cap);
  env->len = 0;
  env->cap = vol_cap + nvol_cap + res_cap + stack_cap;
  env->stack = vol_cap + nvol_cap + res_cap;
  env->res = vol_cap + nvol_cap;
  env->nvol = vol_cap;
  return env;
}

env_t *create_full_env(size_t vol_cap, size_t nvol_cap, size_t res_cap,
                       size_t stack_cap) {
  env_t *env = malloc(sizeof(*env));
  env->arr =
      calloc(sizeof(*env->arr), vol_cap + nvol_cap + res_cap + stack_cap);
  env->len = 0;
  env->cap = vol_cap + nvol_cap + res_cap + stack_cap;
  env->stack = vol_cap + nvol_cap + res_cap;
  env->res = vol_cap + nvol_cap;
  env->nvol = vol_cap;
  while (env->len < env->stack) {
    push_env(env, 0, 0);
  }
  return env;
}

void delete_env(env_t *env) {
  for (size_t i = 0; i < env->len; i++) {
    free(env->arr[i].str);
    free(env->arr[i].args);
  }
  free(env->arr);
  free(env);
}

void push_env(env_t *env, char *str, enum used_type used) {
  if (env->len >= env->cap) {
    env->cap <<= 1;
    env->arr = reallocarray(env->arr, env->cap, sizeof(*env->arr));
    if (!env->arr) {
      err(1, "Failed to allocate memory for an array of"
             "env");
    }
  }
  env->arr[env->len].str = str;
  env->arr[env->len].used = used;
  env->arr[env->len].args = 0;
  env->arr[env->len].root_spill = 0;
  env->arr[env->len].arg_spill = 0;
  env->len++;
}

void insert_env(env_t *env, size_t i, char *str, enum used_type used) {
  while (i > env->len) {
    push_env(env, 0, 0);
  }
  env->arr[i].str = str;
  env->arr[i].used = used;
  env->arr[i].args = 0;
  env->arr[i].root_spill = 0;
  env->arr[i].arg_spill = 0;
  env->len++;
}

void remove_env(env_t *env, size_t i) {
  free(env->arr[i].str);
  env->arr[i].str = 0;
  env->arr[i].used = 0;
  env->arr[i].args = 0;
  env->arr[i].root_spill = 0;
  env->arr[i].arg_spill = 0;
}

ssize_t find_env(env_t *env, const char *str) {
  for (size_t i = 0; i < env->len; i++) {
    if (env->arr[i].str && !strcmp(env->arr[i].str, str))
      return i;
  }
  return -1;
}

size_t get_unused_env(env_t *env) {
  for (size_t i = 0; i < env->len; i++) {
    if (!env->arr[i].used && ((i < env->res) || (i > env->stack))) {
      env->arr[i].used = 1;
      return i;
    }
  }
  push_env(env, 0, 1);
  return env->len - 1;
}

size_t get_unused_postn_env(env_t *env, size_t n) {
  while (n > env->len) {
    push_env(env, 0, 0);
  }
  for (size_t i = n; i < env->len; i++) {
    if (!env->arr[i].used && ((i < env->res) || (i > env->stack))) {
      env->arr[i].used = 1;
      return i;
    }
  }
  push_env(env, 0, 1);
  return env->len - 1;
}

size_t get_unused_pren_env(env_t *env, size_t n) {
  while (n > env->len) {
    push_env(env, 0, 0);
  }
  for (size_t i = 0; i < env->res && i < env->len; i++) {
    if (!env->arr[i].used) {
      env->arr[i].used = 1;
      return i << 1;
    }
  }
  return (env->nvol << 1) | 1;
}

void move_env(env_t *env, size_t i, size_t j) {
  env->arr[i].args = env->arr[j].args;
  env->arr[i].str = env->arr[j].str;
  env->arr[i].used = env->arr[j].used;
  env->arr[i].arg_spill = env->arr[j].arg_spill;
  env->arr[i].root_spill = env->arr[j].root_spill;
  env->arr[j].args = 0;
  env->arr[j].str = 0;
  env->arr[j].used = 0;
  env->arr[j].arg_spill = 0;
  env->arr[j].root_spill = 0;
}

size_t reassign_postn_env(env_t *env, size_t i, size_t n) {
  size_t new = get_unused_postn_env(env, n);
  move_env(env, new, i);
  return new;
}

void print_lined_env(const env_t *env) {
  for (size_t i = 0; i < env->len; i++) {
    if (env->arr[i].str) {
      puts(env->arr[i].str);
    }
  }
}
