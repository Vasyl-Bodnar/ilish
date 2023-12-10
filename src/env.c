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

void push_env(env_t *env, char *str) {
  expand_env(env);
  env->arr[env->len].str = str;
  env->arr[env->len].used = 0;
  env->len++;
  if (env->len > env->req)
    env->req = env->len;
}

void pop_env(env_t *env) {
  env->len--;
  free(env->arr[env->len].str);
}

void popn_env(env_t *env, size_t n) {
  while (n--) {
    env->len--;
    free(env->arr[env->len].str);
  }
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

void print_lined_env(const env_t *env) {
  for (size_t i = 0; i < env->len; i++) {
    puts(env->arr[i].str);
  }
}
