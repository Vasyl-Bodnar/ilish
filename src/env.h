#ifndef ENV_H
#define ENV_H

#include <sys/types.h>

/// @file env.h
/// @brief Environment which keeps track of registers and variables
///
/// Significantly refactored once a while
///
/// This is the manager. It knows what registers compiler has and what is placed
/// in them. It keeps track of the variables sequentially in the order they are
/// initialized (makes it easy for defines).
///
/// NOTE: "Register" here can mean volatile/non-volatile real registers,
/// compiler-reserved registers or even stack assignment. You can tell which it
/// is exactly by "offsets", i.e. index of this begins, previous ends.

enum var_type {
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
  BoxUnknown = 12, // Boxed types below
  BoxFixnum,
  BoxChar,
  BoxUniChar,
  BoxBoolean,
  BoxNil,
};

typedef struct regr {
  ///> Type flag
  enum var_type type;
  ///> Is it a variable flag
  char variable;
  ///> Pointer flag
  char root_spill;
  ///> Is it an argument flag
  char arg_spill;
} regr_t;

typedef struct var {
  ///> Variable name
  char *str;
  ///> Type flag. Note that it is identical to regr's var_type
  enum var_type type;
  ///> Function arguments (0 for non-functions)
  struct exprs_t *args;
  ///> Register reserved in the Register table OR Data Section Constants
  ///> depending on constant flag. -1 is default for unassigned
  ssize_t idx;
  ///> Is it a data section constant flag
  char constant;
  ///> Active flag
  char active;
} var_t;

/// @brief Compiler environment
typedef struct env_t {
  // Variable lookup table
  var_t *arr;
  size_t len;
  size_t cap;

  // Register/Stack lookup table
  regr_t *rarr;
  size_t rlen;
  size_t rcap;
  size_t nonvol_offset;
  size_t reserved_offset;
  size_t stack_offset;
} env_t;

/// @brief Create the `env` object with initial capacity.
/// @param vol_cap The capacity of volatile registers.
/// @param nvol_cap The capacity of non-volatile registers.
/// @param res_cap The capacity of reserved registers.
/// @param stack_cap The capacity of the stack.
/// @param vars_cap The capacity of variables.
/// @returns `env` object.
env_t *create_env(size_t vol_cap, size_t nvol_cap, size_t res_cap,
                  size_t stack_cap, size_t vars_cap);

/// @brief Create the `env` object with initial capacity and fills everything
/// until stack_cap with 0.
/// @param vol_cap The capacity of volatile registers.
/// @param nvol_cap The capacity of non-volatile registers.
/// @param res_cap The capacity of reserved registers.
/// @param stack_cap The capacity of the stack.
/// @param vars_cap The capacity of variables.
/// @returns `env` object.
env_t *create_full_env(size_t vol_cap, size_t nvol_cap, size_t res_cap,
                       size_t stack_cap, size_t vars_cap);

/// @brief Identical to create_full_env, but with constants preinitialized
/// @sa create_full_env
env_t *create_full_from_const_env(env_t *constants, size_t vol_cap,
                                  size_t nvol_cap, size_t res_cap,
                                  size_t stack_cap, size_t const_cap);

/// @brief Frees the `env` and all the inner allocations.
void delete_env(env_t *env);

/// @brief Push a register to `env`.
///
/// Expands by 2 when cap is reached.
/// @param type Whether reg is currently used and its type.
/// @param var Is it a variable
void push_env(env_t *env, enum var_type type, char var);

/// @brief Insert a register into `env`.
///
/// Will forcefully expand to reach target.
/// @param i The location to place it in
/// @param type Whether reg is currently used and its type.
/// @param var Is it a variable
/// @sa push_env
void insert_env(env_t *env, size_t i, enum var_type type, char var);

/// @brief Zeroes the reg at the location.
///
/// Do note, despite being named remove, it in fact zeroes the values.
/// @param i The location to free.
void remove_env(env_t *env, size_t i);

/// @brief Pushes a var to `env`.
///
/// Expands by 2 when cap is reached. `env` assumes ownership of `str`.
/// @param str The `char*` to be pushed, will be owned.
/// @param used Whether var is currently used and its type.
/// @sa push_env
void push_var_env(env_t *env, char *str, enum var_type type, size_t idx,
                  char constant);

/// @brief Pushes a var to `env`.
///
/// Do note that if it is a mutable varialbe, it also clears its register
/// @sa push_var_env
void pop_var_env(env_t *env);

/// @brief Removes a variable from `env` by clearing it with 0s.
///
/// @param i The var index to remove.
/// @sa remove_env
void remove_var_env(env_t *env, size_t i);

/// @brief Linear find in variable table.
/// @param str Non-null char* to find.
/// @return Index if found, -1 otherwise.
/// @sa find_env
ssize_t find_var_env(env_t *env, const char *str);

/// @brief Linear find after n in variable table.
/// @param str Non-null char* to find.
/// @param n Starting value of the search
/// @return Index if found, -1 otherwise.
/// @sa find_var_env
ssize_t find_var_postn_env(env_t *env, const char *str, size_t n);

/// @brief Right->left find
///
/// These versions are particularly useful for variables, as the newer ones
/// are always pushed.
/// @param str Non-null char* to find.
/// @return Index if found, -1 otherwise.
/// @sa find_var_env
ssize_t rfind_var_env(env_t *env, const char *str);

/// @brief Find a provided `char*` var name in activated `env` slot.
///
/// Since constants are initialized at the start of the program,
/// they need to be "activated" when they are actually defined
/// during compiler's tracing.
/// @param str Non-null char* to find.
/// @return Index if found, -1 otherwise.
/// @sa find_env
ssize_t find_active_var_env(env_t *env, const char *str);

/// @brief Right->left find_active
///
/// These versions are particularly useful for variables, as the newer ones
/// are always pushed.
/// @param str Non-null char* to find.
/// @return Index if found, -1 otherwise.
/// @sa find_active_var_env
ssize_t rfind_active_var_env(env_t *env, const char *str);

/// @brief Reserves an unused register
/// @return Index of the register.
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

/// @brief Move data from i register to j and clear i
void move_env(env_t *env, size_t i, size_t j);

/// @brief Takes a register and tries to reassign it after n
/// @param i The register to reassign
/// @param n The point after which to reassign
/// @sa get_unused_env
size_t reassign_postn_env(env_t *env, size_t i, size_t n);

#endif // ENV_H
