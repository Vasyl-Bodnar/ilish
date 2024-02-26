#include "free.h"
#include <err.h>
#include <malloc.h>
#include <string.h>

free_t *create_free(size_t cap) {
  free_t *free_v = malloc(sizeof(*free_v));
  free_v->arr = calloc(sizeof(*free_v->arr), cap);
  free_v->len = 0;
  free_v->cap = cap;
  return free_v;
}

void delete_free(free_t *free_v) {
  for (size_t i = 0; i < free_v->len; i++) {
    free(free_v->arr[i].str);
  }
  free(free_v->arr);
  free(free_v);
}

void expand_free(free_t *free_v) {
  if (free_v->len >= free_v->cap) {
    free_v->cap <<= 1;
    free_v->arr = reallocarray(free_v->arr, free_v->cap, sizeof(*free_v->arr));
    if (!free_v->arr) {
      err(1, "Failed to allocate memory for array of free");
    }
  }
}

void push_free(free_t *free_v, char *str, int boxed) {
  expand_free(free_v);
  free_v->arr[free_v->len].str = str;
  free_v->arr[free_v->len].boxed = boxed;
  free_v->len++;
}

void pop_free(free_t *free_v) {
  free_v->len--;
  free(free_v->arr[free_v->len].str);
}

ssize_t find_free(const free_t *free_v, const char *str) {
  for (size_t i = 0; i < free_v->len; i++) {
    if (!strcmp(free_v->arr[i].str, str))
      return i;
  }
  return -1;
}
