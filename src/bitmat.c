#include "bitmat.h"
#include <stdlib.h>

/// NOTE: Currently a simple implementation which is a bit memory wasteful
/// and only supports up to 64 columns
/// This is not a problem given that for argument ordering,
/// it is unlikely to use 64 of them, but preferably limit is significantly
/// increased.
bitmat_t *create_bitmat(size_t nrow, size_t ncol) {
  bitmat_t *bitmat = malloc(sizeof(*bitmat));
  bitmat->arr = calloc(sizeof(*bitmat->arr), nrow);
  bitmat->nrow = nrow;
  bitmat->ncol = ncol;
  return bitmat;
}

void delete_bitmat(bitmat_t *bitmat) {
  free(bitmat->arr);
  free(bitmat);
}

int get_bitmat(bitmat_t *bitmat, size_t row, size_t col) {
  return bitmat->arr[row] & (1 << col);
}

void set_bitmat(bitmat_t *bitmat, size_t row, size_t col, int val) {
  bitmat->arr[row] |= val << col;
}

void flip_bitmat(bitmat_t *bitmat, size_t row, size_t col) {
  bitmat->arr[row] ^= 1 << col;
}

size_t is_ef_pivot_col_bitmat(bitmat_t *bitmat, size_t pivot_col, size_t row) {
  for (size_t i = row + 1; i < bitmat->nrow; i++) {
    if (get_bitmat(bitmat, i, pivot_col)) {
      return i;
    }
  }
  return 0;
}

size_t is_row_unique_bitmat(bitmat_t *bitmat, size_t pivot_row) {
  for (size_t j = pivot_row; j < bitmat->nrow; j++) {
    if (!(bitmat->arr[pivot_row] ^ bitmat->arr[j]) &&
        !(bitmat->arr[pivot_row] != ((size_t)1 << pivot_row))) {
      return j;
    }
  }
  return 0;
}

void swap_row_bitmat(bitmat_t *bitmat, size_t r1, size_t r2) {
  size_t tmp = bitmat->arr[r1];
  bitmat->arr[r1] = bitmat->arr[r2];
  bitmat->arr[r2] = tmp;
}
