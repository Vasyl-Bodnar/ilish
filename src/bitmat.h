#ifndef BITMAT_H
#define BITMAT_H

#include <sys/types.h>

/// @file bitmat.h
/// @brief Bitmatrix

/// @brief The Bitmatrix
/// TODO: Make ncol scalable beyond 64bit limit of a single size_t.
/// Arrays of arrays are an option.
/// Best to redesign it into even more compact form.
/// I.e. 8x8 being a single integer.
typedef struct bitmat_t {
  size_t *arr; ///> The pointer to numbers
  size_t nrow; ///> The number of rows/numbers of the matrix
  size_t ncol; ///> The number of columns of the matrix
} bitmat_t;

/// @brief Create the `bitmat` object with initial capacity.
/// @param nrow Number of rows
/// @param ncol Number of columns
/// @return `bitmat` object.
bitmat_t *create_bitmat(size_t nrow, size_t ncol);

/// @brief Frees all the numbers and the `bitmat` object
void delete_bitmat(bitmat_t *mat);

int get_bitmat(bitmat_t *bitmat, size_t row, size_t col);

void set_bitmat(bitmat_t *bitmat, size_t row, size_t col, int val);

void flip_bitmat(bitmat_t *bitmat, size_t row, size_t col);

/// @brief Checks if column pivot is in echelon form,
///
/// I.e. everything below it is 0.
/// Do note that for my graph ordering purposes only diagonals need to be
/// checked.
///    a b
/// [a 1 0
/// a 1 0].
/// So despite this not being an echelon form, this can be
/// "echelon form" from perspective of diagonals (like top a going into b).
/// @returns 0 if yes, >0 indicates no and represents the issue row
size_t is_ef_pivot_col_bitmat(bitmat_t *bitmat, size_t pivot_col, size_t row);

/// @brief Checks if there is a duplicate for a given row,
///
/// I.e. unsolvable ordering conflicts.
///      a b
/// [a+b 1 1
/// ab  1 1].
/// This is impossible to solve since a or b will have to be
/// overwritten before they are used.
/// While
/// [1 1
/// 0 1] has a clear solution.
/// This also accounts for a special case
/// [1 0
/// 1 0].
/// Where theory dictates it is a duplicate, for ordering purposes only
/// diagonals matter, so there is not.
/// @returns 0 if yes, >0 there is a duplicate and represents the issue row
size_t is_row_unique_bitmat(bitmat_t *bitmat, size_t pivot_row);

/// @brief Swaps two rows
void swap_row_bitmat(bitmat_t *bitmat, size_t r1, size_t r2);

#endif // BITMAT_H
