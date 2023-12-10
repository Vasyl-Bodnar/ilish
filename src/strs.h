#ifndef STRS_H
#define STRS_H

#include <sys/types.h>

typedef struct strs_t {
  char **arr;
  size_t len;
  size_t cap;
} strs_t;

strs_t *create_strs(size_t cap);

/// Frees the `strs`, `arr`, and all the inner `strings`
void delete_strs(strs_t *strs);

/// Assumes ownership of `str`
void push_strs(strs_t *strs, char *str);

/// Frees the str at the end
void pop_strs(strs_t *strs);

/// Pop applied multiple times
void popn_strs(strs_t *strs, size_t n);

/// If found, index is returned
/// Otherwise -1 is returned
ssize_t find_strs(const strs_t *strs, const char *str);

/// `find_strs` from the back
ssize_t rfind_strs(const strs_t *strs, const char *str);

void print_lined_strs(const strs_t *strs);

#endif // STRS_H
