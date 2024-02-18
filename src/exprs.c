#include "exprs.h"
#include "expr.h"
#include "strs.h"
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
    exprs->cap += exprs->cap & 1 ? (exprs->cap + 1) >> 1 : exprs->cap >> 1;
    exprs->arr = reallocarray(exprs->arr, exprs->cap, sizeof(*exprs->arr));
    if (!exprs->arr) {
      err(1, "Failed to allocate memory for an array of expressions");
    }
  }
  exprs->arr[exprs->len] = expr;
  exprs->len++;
}

exprs_t *clone_exprs(exprs_t *exprs) {
  exprs_t *clone = create_exprs(exprs->cap);
  for (size_t i = 0; i < exprs->len; i++) {
    push_exprs(clone, clone_expr(exprs->arr[i]));
  }
  return clone;
}

exprs_t slice_start_exprs(exprs_t *exprs, size_t start) {
  if (start <= exprs->len) {
    return (exprs_t){.arr = exprs->arr + start,
                     .len = exprs->len - start,
                     .cap = exprs->cap - start};
  } else {
    return (exprs_t){.arr = 0, .len = 0, .cap = 0};
  }
}

exprs_t *slice_start_clone_exprs(exprs_t *exprs, size_t start) {
  if (start <= exprs->len) {
    exprs_t *clone = create_exprs(exprs->cap - start);
    for (size_t i = start; i < exprs->len; i++) {
      push_exprs(clone, clone_expr(exprs->arr[i]));
    }
    return clone;
  } else {
    return create_exprs(0);
  }
}

int find_symb_exprs(exprs_t *exprs, const char *symb) {
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
    case Sym:
      if (!strcmp(exprs->arr[i].str, symb))
        return i;
      else
        break;
    case List:
      if (find_symb_exprs(exprs->arr[i].exprs, symb) > -1)
        return i;
      else
        break;
    }
  }
  return -1;
}

struct strs_t *strs_from_exprs(exprs_t *exprs) {
  strs_t *strs = create_strs(exprs->len);
  for (size_t i = 0; i < exprs->len; i++) {
    push_strs(strs, strdup(exprs->arr[i].str));
  }
  return strs;
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
