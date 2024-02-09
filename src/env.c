#include "env.h"
#include <err.h>
#include <malloc.h>
#include <string.h>

env_t *create_env(size_t cap) {
  env_t *env = malloc(sizeof(*env));
  env->arr = malloc(sizeof(*env->arr) * cap);
  env->req = 0;
  env->len = 0;
  env->cap = cap;
  return env;
}

void delete_env(env_t *env) {
  for (size_t i = 0; i < env->len; i++) {
    free(env->arr[i].str);
  }
  free(env->arr);
  free(env);
}

void expand_env(env_t *env) {
  if (env->len >= env->cap) {
    env->cap <<= 1;
    env->arr = reallocarray(env->arr, env->cap, sizeof(*env->arr));
    if (!env->arr) {
      err(1, "Failed to allocate memory for array of env");
    }
  }
}

void push_env(env_t *env, char *str, char used, char points) {
  expand_env(env);
  env->arr[env->len].str = str;
  env->arr[env->len].used = used;
  env->arr[env->len].points = points;
  env->arr[env->len].root_spill = 0;
  env->len++;
  if (env->len > env->req)
    env->req = env->len;
}

void push_postn_env(env_t *env, char *str, char used, char points, size_t n) {
  while (env->len < n) {
    push_env(env, 0, 0, 0);
  }
  push_env(env, str, used, points);
}

void push_stack_env(env_t *env, char *str, char used, char points) {
  push_postn_env(env, str, used, points, 6);
}

void pop_env(env_t *env) {
  env->len--;
  free(env->arr[env->len].str);
  env->arr[env->len].str = 0;
  env->arr[env->len].used = 0;
  env->arr[env->len].points = 0;
  env->arr[env->len].root_spill = 0;
}

void popn_env(env_t *env, size_t n) {
  while (n--) {
    pop_env(env);
  }
}

void remove_env(env_t *env, size_t i) {
  free(env->arr[i].str);
  env->arr[i].str = 0;
  env->arr[i].used = 0;
  env->arr[i].points = 0;
  env->arr[i].root_spill = 0;
}

void pop_or_remove_env(env_t *env, size_t i) {
  if (i == env->len - 1) {
    pop_env(env);
  } else {
    remove_env(env, i);
  }
}

ssize_t find_unused_env(env_t *env) {
  for (size_t i = 0; i < env->len; i++) {
    if (!env->arr[i].used)
      return i;
  }
  return -1;
}

ssize_t find_unused_stack_env(env_t *env) {
  for (size_t i = 6; i < env->len; i++) {
    if (!env->arr[i].used)
      return i;
  }
  return -1;
}

ssize_t find_unused_null_env(env_t *env) {
  for (size_t i = 0; i < env->len; i++) {
    if (!env->arr[i].str && !env->arr[i].used)
      return i;
  }
  return -1;
}

ssize_t find_env(env_t *env, const char *str) {
  for (size_t i = 0; i < env->len; i++) {
    if (env->arr[i].str && !strcmp(env->arr[i].str, str))
      return i;
  }
  return -1;
}

ssize_t rfind_env(env_t *env, const char *str) {
  for (size_t i = env->len; i > 0; i--) {
    if (env->arr[i].str && !strcmp(env->arr[i].str, str))
      return i;
  }
  if (env->arr[0].str && !strcmp(env->arr[0].str, str)) {
    return 0;
  } else {
    return -1;
  }
}

size_t get_unused_env(env_t *env) {
  for (size_t i = 0; i < env->len; i++) {
    if (!env->arr[i].used) {
      env->arr[i].used = 1;
      return i;
    }
  }
  push_env(env, 0, 1, 0);
  return env->len - 1;
}

size_t get_unused_postn_env(env_t *env, size_t n) {
  for (size_t i = n; i < env->len; i++) {
    if (!env->arr[i].used) {
      env->arr[i].used = 1;
      return i;
    }
  }
  push_postn_env(env, 0, 1, 0, n);
  return env->len - 1;
}

void print_lined_env(const env_t *env) {
  for (size_t i = 0; i < env->len; i++) {
    puts(env->arr[i].str);
  }
}
