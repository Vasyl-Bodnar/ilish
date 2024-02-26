#ifndef FREE_H
#define FREE_H

#include <sys/types.h>

/// @file free.h
/// @brief Vector of free variables.

/// @brief Vector of free variables.
typedef struct free_t {
  ///> The pointer to free vars
  ///>
  ///> NOTE: Boxed is still 0 for pointers such as cons and vector,
  ///> it exists purely for boxing of immediates like fixnum
  struct free_var {
    char *str;
    int boxed;
  } *arr;
  ///> The length of the vector
  size_t len;
  ///> The capacity of the vector
  size_t cap;
} free_t;

/// @brief Create the `free` object with initial capacity.
/// @param cap Initial capacity.
/// @return `free` object.
free_t *create_free(size_t cap);

/// @brief Frees the `free`, `arr`, and all the inner `char*`.
void delete_free(free_t *free);

/// @brief Push a `char*` to `free`.
///
/// Expands `free` `cap` by factor of 2 if reached.
/// `free` assumes ownership of `str`.
/// @param str The `char*` to be pushed.
void push_free(free_t *free, char *str, int boxed);

/// @brief Frees and removes the `char*` at the end.
void pop_free(free_t *free);

/// @brief Find a provided `char*` in `free`
/// @param str `char*` to find in `free`. May be null.
/// @return Index if found, -1 otherwise.
/// @sa rfind_free
ssize_t find_free(const free_t *free, const char *str);

#endif // FREE_H
