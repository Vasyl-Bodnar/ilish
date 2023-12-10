#ifndef ENV_H
#define ENV_H

#include <sys/types.h>

typedef struct env_t {
  struct {
    char *str;
    char used;
  } *arr;
  size_t req;
  size_t len;
  size_t cap;
} env_t;

env_t *create_env(size_t cap);

/// Frees the `env`, `arr`, and all the inner `strings`
void delete_env(env_t *env);

/// Assumes ownership of `str`
void push_env(env_t *env, char *str);

void pop_env(env_t *env);

/// Pop applied multiple times
void popn_env(env_t *env, size_t n);

/// If found, index is returned
/// Otherwise -1 is returned
ssize_t find_env(env_t *env, const char *str);

/// `find_env` from the back, useful for stack simulation
ssize_t rfind_env(env_t *env, const char *str);

/// Like `find_env`, returns -1 if not found otherwise index.
/// Only searches for `NULL` values
ssize_t find_unused_null_env(env_t *env);

void print_lined_env(const env_t *env);

#endif // ENV_H
