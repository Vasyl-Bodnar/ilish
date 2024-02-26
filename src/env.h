#ifndef ENV_H
#define ENV_H

#include <sys/types.h>

/// @file env.h
/// @brief Expanded version of `strs` for compiler environment.
///
/// Vector of `char*` much like `strs`.
/// However to work better as variable and register environment, certain
/// additional functionalities were added and modified:
///
/// - `used`, an extra flag to denote whether location is currently in use.
/// - `find_env` does not allow null inputs.
///
/// Currently in-process of HEAVY refactoring.

enum used_type {
  None = 0,    // Unused/Free
  Unknown = 1, // Either yet, or rely on runtime info
  Fixnum,
  Char,
  UniChar,
  Boolean,
  Nil,
  Cons = 6, // Pointers below
  Vector,
  String,
  UniString,
  Symbol,
  Lambda,
  Box,
};

typedef struct var {
  ///> Variable name, or null for temporary use
  char *str;
  ///> In-use flag
  enum used_type used;
  ///> Function arguments
  struct exprs_t *args;
  ///> Pointer flag
  int root_spill;
  ///> Arg flag
  int arg_spill;
} var_t;

/// @brief Compiler environment
typedef struct env_t {
  var_t *arr;
  size_t len;
  size_t cap;
  ///> Non-Volatile offset
  size_t nvol;
  ///> Reserved offset
  size_t res;
  ///> Stack offset
  size_t stack;
} env_t;

/// @brief Create the `env` object with initial capacity.
/// @param vol_cap The capacity of volatile registers.
/// @param nvol_cap The capacity of non-volatile registers.
/// @param res_cap The capacity of reserved registers.
/// @param stack_cap The capacity of the stack.
/// @returns `env` object.
env_t *create_env(size_t vol_cap, size_t nvol_cap, size_t res_cap,
                  size_t stack_cap);

/// @brief Create the `env` object with initial capacity and fills everything
/// until stack_cap with 0.
/// @param vol_cap The capacity of volatile registers.
/// @param nvol_cap The capacity of non-volatile registers.
/// @param res_cap The capacity of reserved registers.
/// @param stack_cap The capacity of the stack.
/// @returns `env` object.
env_t *create_full_env(size_t vol_cap, size_t nvol_cap, size_t res_cap,
                       size_t stack_cap);

/// @brief Frees the `env`, `arr`, and all the inner `char*`.
void delete_env(env_t *env);

/// @brief Push a var to `env`.
///
/// Expands by 2 when cap is reached. `env` assumes ownership of `str`
/// @param str The `char*` to be pushed, will be owned.
/// @param used Whether var is currently used and its type.
void push_env(env_t *env, char *str, enum used_type used);

/// @brief Insert a var into `env`.
///
/// Will forcefully expand to reach target. `env` assumes ownership of `str`
/// @param i The location to place it in
/// @param str The `char*` to be pushed, will be owned.
/// @param used Whether var is currently used and its type.
/// @sa push_env
void insert_env(env_t *env, size_t i, char *str, enum used_type used);

/// @brief Frees and zeroes the var at the location if it is bounded.
///
/// Do note, despite being named remove, it does not change length nor any other
/// inner object placement.
/// @param i The location to free.
void remove_env(env_t *env, size_t i);

/// @brief Find a provided `char*` var name in `env`.
/// @param str Non-null char* to find.
/// @return Index if found, -1 otherwise.
/// @sa find_strs
ssize_t find_env(env_t *env, const char *str);

/// @brief Finds or creates an unused value across any `env` and marks it as
/// used.
/// @return Index of the value.
/// @sa find_env
size_t get_unused_env(env_t *env);

/// @brief get_unused_env after n
///
/// Do note that this forcibly creates new empty (zeroed) vars to reach n
/// @sa get_unused_env
size_t get_unused_postn_env(env_t *env, size_t n);

/// @brief get_unused_env before n
///
/// If it is impossible, it evicts a non-argument register and marks the first
/// bit of the returned location as 1
/// @sa get_unused_env
size_t get_unused_pren_env(env_t *env, size_t n);

/// @brief Move data from j to i and zero j
void move_env(env_t *env, size_t i, size_t j);

/// @brief Takes a var and tries to reassign it after n
/// @param i The var to reassign
/// @param n The point after which to reassign
/// @sa get_unused_env
size_t reassign_postn_env(env_t *env, size_t i, size_t n);

/// @brief Prints inner `char*` separated by newlines.
void print_lined_env(const env_t *env);

#endif // ENV_H
