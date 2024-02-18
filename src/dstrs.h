#ifndef DSTRS_H
#define DSTRS_H

#include <sys/types.h>

/// @file dstrs.h
/// @brief Multiple Vectors of char* with a lock on main.
///
/// A modification of `strs` with locks and buffers

/// @brief Lockable "Double" String Vector.
typedef struct dstrs_t {
  struct main {
    char **arr;
    size_t len;
    size_t cap;
  } main;
  struct buf {
    char **arr;
    size_t len;
    size_t cap;
  } *bufs;
  size_t lock;
  size_t lock_cap;
} dstrs_t;

/// @brief Create the `dstrs` object with initial capacity and empty buffers.
/// @param cap Initial capacity.
/// @return `dstrs` object.
dstrs_t *create_dstrs(size_t cap);

/// @brief Frees the `dstrs`, both main and all buf `arr`, and all the inner
/// `char*`.
void delete_dstrs(dstrs_t *dstrs);

/// @brief Push a `char*` to `dstrs`.
///
/// Expands `dstrs` `cap` by factor of 2 if reached.
/// `dstrs` assumes ownership of `str`.
/// @param str The `char*` to be pushed.
void push_dstrs(dstrs_t *dstrs, char *str);

/// @brief Lock main and previous buffers and use the next buffer only
void lock_dstrs(dstrs_t *dstrs);

/// @brief Unlock the upper level
void unlock_dstrs(dstrs_t *dstrs);

/// @brief Combine with and switch to previous.
///
/// Do note, it assumes that you unlocked this level beforehand.
void force_dstrs(dstrs_t *dstrs);

/// @brief Combine everything into the main buffer.
void collapse_dstrs(dstrs_t *dstrs);

/// @brief Extract main into separate `strs` obj.
///
/// Do note, this is a shallow clone
struct strs_t *extract_main_dstrs(dstrs_t *dstrs);

#endif // DSTRS_H
