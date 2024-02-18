#include "strs.h"
#include <err.h>
#include <malloc.h>
#include <string.h>

strs_t *create_strs(size_t cap) {
  strs_t *strs = malloc(sizeof(*strs));
  strs->arr = calloc(sizeof(*strs->arr), cap);
  strs->len = 0;
  strs->cap = cap;
  return strs;
}

void delete_strs(strs_t *strs) {
  for (size_t i = 0; i < strs->len; i++) {
    free(strs->arr[i]);
  }
  free(strs->arr);
  free(strs);
}

void expand_strs(strs_t *strs) {
  if (strs->len >= strs->cap) {
    strs->cap <<= 1;
    strs->arr = reallocarray(strs->arr, strs->cap, sizeof(*strs->arr));
    if (!strs->arr) {
      err(1, "Failed to allocate memory for array of strs");
    }
  }
}

void push_strs(strs_t *strs, char *str) {
  expand_strs(strs);
  strs->arr[strs->len] = str;
  strs->len++;
}

void pop_strs(strs_t *strs) {
  strs->len--;
  free(strs->arr[strs->len]);
}

void popn_strs(strs_t *strs, size_t n) {
  while (n--) {
    strs->len--;
    free(strs->arr[strs->len]);
  }
}

strs_t *union_strs(const strs_t **strs, size_t len) {
  size_t tot_len = 0;
  for (size_t i = 0; i < len; i++) {
    tot_len += strs[i]->len;
  }
  strs_t *united = create_strs(tot_len);
  united->len = tot_len;
  for (size_t k = 0, i = 0; i < len; i++) {
    for (size_t j = 0; j < strs[i]->len; j++, k++) {
      united->arr[k] = strdup(strs[i]->arr[j]);
    }
  }
  return united;
}

ssize_t find_strs(const strs_t *strs, const char *str) {
  if (!str) {
    for (size_t i = 0; i < strs->len; i++) {
      if (!strs->arr[i])
        return i;
    }
  } else {
    for (size_t i = 0; i < strs->len; i++) {
      if (!strcmp(strs->arr[i], str))
        return i;
    }
  }
  return -1;
}

ssize_t rfind_strs(const strs_t *strs, const char *str) {
  if (!str) {
    for (size_t i = strs->len; i > 0; i--) {
      if (!strs->arr[i])
        return i;
    }
    if (!strs->arr[0])
      return 0;
  } else {
    for (size_t i = strs->len; i > 0; i--) {
      if (!strcmp(strs->arr[i], str))
        return i;
    }
    if (!strcmp(strs->arr[0], str))
      return 0;
  }
  return -1;
}

void print_lined_strs(const strs_t *strs) {
  for (size_t i = 0; i < strs->len; i++) {
    puts(strs->arr[i]);
  }
}
