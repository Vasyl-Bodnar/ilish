#ifndef ENV_H
#define ENV_H

#include <sys/types.h>

/// @file env.h
/// @brief Expanded version of `strs` for compiler environment
///
/// Vector of `char*` much like `strs`.
/// However to work better as variable and stack environment, certain additional
/// functionalities were added and modified:
///
/// - `req`, the maximum length reached to let the compiler know how much stack
/// is required to be allocated.
/// - `used`, an extra flag to denote whether location is currently in use.
/// - `find_env` and `rfind_env` do not allow null inputs
/// - `find_unused_null_env` replaces above

/// @brief Compiler environment
typedef struct env_t {
  struct {     ///> Variable name, or null for generic stack use
    char *str; ///> In-use flag
    int used;
  } *arr; ///> Maximum stack required
  size_t req;
  size_t len;
  size_t cap;
} env_t;

/// @brief Create the `env` object with initial capacity.
/// @param cap The initial capacity.
/// @returns `env` object.
env_t *create_env(size_t cap);

/// @brief Frees the `env`, `arr`, and all the inner `char*`
void delete_env(env_t *env);

/// @brief Push a `char*` to `env`.
///
/// Expands `env` cap by factor of 2 if reached. `env` assumes ownership of
/// `str`. `used` is initialized to 0 (false)
/// @param str The `char*` to be pushed.
void push_env(env_t *env, char *str);

/// @brief Frees and removes the `char*` at the end
void pop_env(env_t *env);

/// @brief `pop` applied multiple times
/// @param n The amount of times to apply pop.
void popn_env(env_t *env, size_t n);

/// @brief Find a provided `char*` in `env`
/// @param str Non-null char* to find
/// @return Index if found, -1 otherwise.
/// @sa find_strs rfind_env
ssize_t find_env(env_t *env, const char *str);

/// @brief `find_env` from the back.
/// @param str Non-null char* to find
/// @return Index if found, -1 otherwise.
/// @sa rfind_strs find_env
ssize_t rfind_env(env_t *env, const char *str);

/// @brief `find_env` for unused null values
/// @return Index if found, -1 otherwise.
/// @sa find_env
ssize_t find_unused_null_env(env_t *env);

/// @brief Prints inner `char*` separated by newlines.
void print_lined_env(const env_t *env);

#endif // ENV_H
