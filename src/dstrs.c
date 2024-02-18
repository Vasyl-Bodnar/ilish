#include "dstrs.h"
#include "strs.h"
#include <err.h>
#include <malloc.h>

dstrs_t *create_dstrs(size_t cap) {
  dstrs_t *dstrs = malloc(sizeof(*dstrs));
  dstrs->main.arr = malloc(sizeof(*dstrs->main.arr) * cap);
  dstrs->main.len = 0;
  dstrs->main.cap = cap;
  dstrs->bufs = malloc(sizeof(*dstrs->bufs) * 2);
  dstrs->lock = 0;
  dstrs->lock_cap = 2;
  return dstrs;
}

void delete_dstrs(dstrs_t *dstrs) {
  for (size_t i = 0; i < dstrs->main.len; i++) {
    free(dstrs->main.arr[i]);
  }
  free(dstrs->main.arr);
  if (dstrs->bufs) {
    for (size_t i = 0; i < dstrs->lock; i++) {
      for (size_t j = 0; j < dstrs->bufs[i].len; j++) {
        free(dstrs->bufs[i].arr[j]);
      }
      free(dstrs->bufs[i].arr);
    }
    free(dstrs->bufs);
  }
  free(dstrs);
}

void expand_main_dstrs(dstrs_t *dstrs) {
  if (dstrs->main.len >= dstrs->main.cap) {
    dstrs->main.cap <<= 1;
    dstrs->main.arr = reallocarray(dstrs->main.arr, dstrs->main.cap,
                                   sizeof(*dstrs->main.arr));
    if (!dstrs->main.arr) {
      err(1, "Failed to allocate memory for main buffer of dstrs");
    }
  }
}

void expand_buf_dstrs(dstrs_t *dstrs) {
  if (dstrs->bufs[dstrs->lock - 1].len >= dstrs->bufs[dstrs->lock - 1].cap) {
    dstrs->bufs[dstrs->lock - 1].cap <<= 1;
    dstrs->bufs[dstrs->lock - 1].arr = reallocarray(
        dstrs->bufs[dstrs->lock - 1].arr, dstrs->bufs[dstrs->lock - 1].cap,
        sizeof(*dstrs->bufs[dstrs->lock - 1].arr));
    if (!dstrs->bufs[dstrs->lock - 1].arr) {
      err(1, "Failed to allocate memory for secondary buffer of dstrs");
    }
  }
}

void push_dstrs(dstrs_t *dstrs, char *str) {
  if (dstrs->lock) {
    expand_buf_dstrs(dstrs);
    dstrs->bufs[dstrs->lock - 1].arr[dstrs->bufs[dstrs->lock - 1].len] = str;
    dstrs->bufs[dstrs->lock - 1].len++;
  } else {
    expand_main_dstrs(dstrs);
    dstrs->main.arr[dstrs->main.len] = str;
    dstrs->main.len++;
  }
}

void lock_dstrs(dstrs_t *dstrs) {
  dstrs->lock++;
  if (dstrs->lock == dstrs->lock_cap) {
    dstrs->lock_cap <<= 1;
    dstrs->bufs =
        reallocarray(dstrs->bufs, dstrs->lock_cap, sizeof(*dstrs->bufs));
    if (!dstrs->bufs) {
      err(1, "Failed to allocate memory for buffers of dstrs");
    }
  }
  if (!dstrs->bufs[dstrs->lock - 1].arr) {
    dstrs->bufs[dstrs->lock - 1].arr =
        malloc(sizeof(*dstrs->bufs[dstrs->lock - 1].arr) * 2);
    dstrs->bufs[dstrs->lock - 1].len = 0;
    dstrs->bufs[dstrs->lock - 1].cap = 2;
  }
}

inline void unlock_dstrs(dstrs_t *dstrs) {
  if (dstrs->lock) {
    dstrs->lock--;
  }
}

void force_dstrs(dstrs_t *dstrs) {
  if (dstrs->lock) {
    for (size_t i = 0; i < dstrs->bufs[dstrs->lock].len; i++) {
      expand_buf_dstrs(dstrs);
      dstrs->bufs[dstrs->lock - 1].arr[dstrs->bufs[dstrs->lock - 1].len] =
          dstrs->bufs[dstrs->lock].arr[i];
      dstrs->bufs[dstrs->lock - 1].len++;
    }
  } else {
    for (size_t i = 0; i < dstrs->bufs[dstrs->lock].len; i++) {
      expand_main_dstrs(dstrs);
      dstrs->main.arr[dstrs->main.len] = dstrs->bufs[dstrs->lock].arr[i];
      dstrs->main.len++;
    }
  }
  free(dstrs->bufs[dstrs->lock].arr);
  dstrs->bufs[dstrs->lock].arr = 0;
}

void collapse_dstrs(dstrs_t *dstrs) {
  size_t highest = 0;
  while (dstrs->bufs[highest].arr) {
    for (size_t i = 0; i < dstrs->bufs[highest].len; i++) {
      expand_main_dstrs(dstrs);
      dstrs->main.arr[dstrs->main.len] = dstrs->bufs[highest].arr[i];
      dstrs->main.len++;
    }
    free(dstrs->bufs[highest].arr);
    dstrs->bufs[highest].arr = 0;
    highest++;
  }
}

struct strs_t *extract_main_dstrs(dstrs_t *dstrs) {
  strs_t *strs = create_strs(dstrs->main.len);
  for (size_t i = 0; i < dstrs->main.len; i++) {
    push_strs(strs, dstrs->main.arr[i]);
  }
  return strs;
}
