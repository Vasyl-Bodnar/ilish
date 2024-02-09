#ifndef STRS_H
#define STRS_H

#include <sys/types.h>

/// @file strs.h
/// @brief Vector of char*.

/// @brief Vector of char*.
typedef struct strs_t {
  char **arr; ///> The pointer to char*
  size_t len; ///> The length of the vector
  size_t cap; ///> The capacity of the vector
} strs_t;

/// @brief Create the `strs` object with initial capacity.
/// @param cap Initial capacity.
/// @return `strs` object.
strs_t *create_strs(size_t cap);

/// @brief Frees the `strs`, `arr`, and all the inner `char*`.
void delete_strs(strs_t *strs);

/// @brief Push a `char*` to `strs`.
///
/// Expands `strs` `cap` by factor of 2 if reached.
/// `strs` assumes ownership of `str`.
/// @param str The `char*` to be pushed.
void push_strs(strs_t *strs, char *str);

/// @brief Frees and removes the `char*` at the end.
void pop_strs(strs_t *strs);

/// @brief `pop` applied multiple times.
/// @param n The amount of times to apply pop.
void popn_strs(strs_t *strs, size_t n);

/// @brief Unite multiple `strs` into one.
///
/// All inner `char*` will be cloned into the new `strs`.
/// @param strs The array of `strs` to unite.
/// @param len The amount of strs in the array.
strs_t *union_strs(const strs_t **strs, size_t len);

/// @brief Find a provided `char*` in `strs`
/// @param str `char*` to find in `strs`. May be null.
/// @return Index if found, -1 otherwise.
/// @sa rfind_strs
ssize_t find_strs(const strs_t *strs, const char *str);

/// @brief `find_strs` from the back.
/// @param str `char*` to find in `strs`. May be null.
/// @return Index if found, -1 otherwise.
/// @sa find_strs
ssize_t rfind_strs(const strs_t *strs, const char *str);

/// @brief Prints `strs` separated by newlines.
void print_lined_strs(const strs_t *strs);

#endif // STRS_H
