#ifndef BUFFER_H_
#define BUFFER_H_

#include <stddef.h>
#include <stdio.h>

#ifndef BUFFER_SIZE
#define BUFFER_SIZE 1024
#endif 

typedef struct buffer_t {
  FILE *file;
  size_t file_loc;
  size_t loc;
  size_t cap;
  char *input;
} buffer_t;

/// Create a file buffer
buffer_t *create_file_buffer(FILE *file);
/// Create pure text buffer
buffer_t *create_buffer(char *text);
void delete_buffer(buffer_t *buffer);

char get(buffer_t *buffer);
/// Returns 0 if file read is successful, 1 if reached eof (or err), 2 if no file left to read from
int adv(buffer_t *buffer);
/// Advance like `adv`, however at block end instead of overwriting the `input`, expand it with realloc and overwrite the newly allocated bytes
int adv_expand(buffer_t *buffer);
int eof(buffer_t *buffer);
/// BAD IDEA
/// Advance like `adv`, however, return 1 if next adv is block change.
/// Use only for span coordination, costs a potential extra branch for both
/// return and check
// int checked_adv(buffer_t *buffer);
/// BAD IDEA
/// Advances as `adv`, however in case expansion to next block is required, this
/// will try to preserve saved and everything after it. Very important for cases
/// like ...dis | connected..., as spans cannot navigate between blocks
// void adv_preserving(buffer_t *buffer, size_t saved);

#endif // BUFFER_H_
