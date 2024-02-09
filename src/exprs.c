#include "exprs.h"
#include "expr.h"
#include <err.h>
#include <malloc.h>
#include <string.h>

exprs_t *create_exprs(size_t cap) {
  exprs_t *exprs = malloc(sizeof(*exprs));
  exprs->arr = malloc(sizeof(*exprs->arr) * cap);
  exprs->len = 0;
  exprs->cap = cap;
  return exprs;
}

void delete_exprs(exprs_t *exprs) {
  for (size_t i = 0; i < exprs->len; i++) {
    delete_expr(exprs->arr[i]);
  }
  free(exprs->arr);
  free(exprs);
}

void push_exprs(exprs_t *exprs, expr_t expr) {
  if (exprs->len == exprs->cap) {
    exprs->cap <<= 1;
    exprs->arr = reallocarray(exprs->arr, exprs->cap, sizeof(*exprs->arr));
    if (!exprs->arr) {
      err(1, "Failed to allocate memory for an array of expressions");
    }
  }
  exprs->arr[exprs->len] = expr;
  exprs->len++;
}

exprs_t slice_start_exprs(exprs_t *exprs, size_t start) {
  if (start < exprs->len) {
    return (exprs_t){.arr = exprs->arr + start,
                     .len = exprs->len - start,
                     .cap = exprs->cap - start};
  } else {
    return (exprs_t){.arr = 0, .len = 0, .cap = 0};
  }
}

int find_symb_exprs(exprs_t *exprs, const char *symb, const char *src) {
  for (size_t i = 0; i < exprs->len; i++) {
    switch (exprs->arr[i].type) {
    case Err:
    case Null:
    case Bool:
    case Chr:
    case Num:
    case Str:
    case Vec:
      break;
    case Symb:
      if (!strcmp(exprs->arr[i].str, symb))
        return i;
      else
        break;
    case List:
      if (find_symb_exprs(exprs->arr[i].exprs, symb, src) > -1)
        return i;
      else
        break;
    }
  }
  return -1;
}

void print_exprs(exprs_t *exprs, const char *src) {
  if (exprs->len) {
    for (size_t i = 0; i < exprs->len - 1; i++) {
      print_expr(exprs->arr[i], src);
      printf(" ");
    }
    print_expr(exprs->arr[exprs->len - 1], src);
  }
}
