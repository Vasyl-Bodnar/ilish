#ifndef ENV_H
#define ENV_H

#include <sys/types.h>

/// @file env.h
/// @brief Expanded version of `strs` for compiler environment.
///
/// Vector of `char*` much like `strs`.
/// However to work better as variable and stack environment, certain additional
/// functionalities were added and modified:
///
/// - `req`, the maximum length reached to let the compiler know how much stack
/// is required to be allocated.
/// - `used`, an extra flag to denote whether location is currently in use.
/// - `find_env` and `rfind_env` do not allow null inputs.
/// - `find_unused_null_env` replaces above.
///
/// Currently in-process of signficant refactoring.

/// @brief Compiler environment
// (Potentially remove pop and rely on len)
typedef struct env_t {
  struct var {       ///> Variable name, or null for generic stack use
    char *str;       ///> In-use flag
    char used;       ///> Pointer flag
    char points;     ///> Root(stack) flag
    char root_spill; ///> Arg flag
    char arg_spill;
  } *arr; ///> Maximum stack required
  size_t req;
  size_t len;
  size_t cap;
} env_t;

/// @brief Create the `env` object with initial capacity.
/// @param cap The initial capacity.
/// @returns `env` object.
env_t *create_env(size_t cap);

/// @brief Frees the `env`, `arr`, and all the inner `char*`.
void delete_env(env_t *env);

/// @brief Push a var to `env`.
///
/// Expands `env` cap by factor of 2 if reached. `env` assumes ownership of
/// `str`.
/// @param str The `char*` to be pushed.
/// @param used Whether var is currently used.
/// @param points Whether var is a pointer.
void push_env(env_t *env, char *str, char used, char points);

/// @brief Push a var to `env` after n elements.
///
/// If environment is currently below the n, it repeatedly forces
/// environment to expand using `push_env` with dead nulls.
/// @param n The number of elements to ignore
/// @sa push_env push_stack_env
void push_postn_env(env_t *env, char *str, char used, char points, size_t n);

/// @brief Push a var to `env`'s stack.
///
/// A wrapper around postn with 6
/// @sa push_env push_postn_env
void push_stack_env(env_t *env, char *str, char used, char points);

/// @brief Frees and removes the var at the end.
/// @sa popn_env remove_env pop_or_remove_env
void pop_env(env_t *env);

/// @brief `pop` applied multiple times.
/// @param n The amount of times to apply pop.
/// @sa pop_env
void popn_env(env_t *env, size_t n);

/// @brief Frees and zeroes the var at the location if it is bounded.
///
/// Do note, despite being named remove, it does not change length nor any other
/// inner object placement. For length prefer pop_or_remove_env while API is
/// still being transitioned.
/// @param i The location to free.
/// @sa pop_env pop_or_remove_env
void remove_env(env_t *env, size_t i);

/// @brief Conveniance function which calls pop for top variables and remove for
/// everything else.
///
/// The reason for its existance is current API has been changed significantly
/// and requires refactoring. Meanwhile this provides the best of both world at
/// a cost of a branch.
/// @param i The location to free.
void pop_or_remove_env(env_t *env, size_t i);

/// @brief Find a provided `char*` (var name) in `env`.
/// @param str Non-null char* to find.
/// @return Index if found, -1 otherwise.
/// @sa find_strs rfind_env.
ssize_t find_env(env_t *env, const char *str);

/// @brief `find_env` from the back.
/// @param str Non-null char* to find.
/// @return Index if found, -1 otherwise.
/// @sa rfind_strs find_env
ssize_t rfind_env(env_t *env, const char *str);

/// @brief `find_env` for unused vars.
/// @return Index if found, -1 otherwise.
/// @sa find_env
ssize_t find_unused_env(env_t *env);

/// @brief `find_env` for unused stack vars.
/// @return Index if found, -1 otherwise.
/// @sa find_env
ssize_t find_unused_stack_env(env_t *env);

/// @brief `find_env` for unused null values.
/// @return Index if found, -1 otherwise.
/// @sa find_env
ssize_t find_unused_null_env(env_t *env);

/// @brief Finds or creates an unused value and marks it as used.
/// @return Index of the value
/// @sa find_env find_unused_env
size_t get_unused_env(env_t *env);

/// @brief Finds or creates an unused value after (and including) n and marks it
/// as used.
/// @return Index of the value
/// @sa get_unused_env
size_t get_unused_postn_env(env_t *env, size_t n);

/// @brief Prints inner `char*` separated by newlines.
void print_lined_env(const env_t *env);

#endif // ENV_H
