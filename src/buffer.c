#include "buffer.h"
#include "malloc.h"
#include <err.h>
#include <stdio.h>

buffer_t *create_file_buffer(FILE *file) {
  buffer_t *buffer = malloc(sizeof(*buffer));
  buffer->file = file;
  buffer->file_loc = 0;
  buffer->loc = 0;
  buffer->cap = BUFFER_SIZE;
  buffer->input = malloc(sizeof(*buffer->input) * BUFFER_SIZE);
  size_t n = fread(buffer->input, 1, BUFFER_SIZE, buffer->file);
  if (n < BUFFER_SIZE)
    buffer->input[n - 1] = '\0';
  return buffer;
}

buffer_t *create_buffer(char *text) {
  buffer_t *buffer = malloc(sizeof(*buffer));
  buffer->file = NULL;
  buffer->file_loc = 0;
  buffer->loc = 0;
  buffer->input = text;
  return buffer;
}

void delete_buffer(buffer_t *buffer) {
  free(buffer->input);
  free(buffer);
}

inline char get(buffer_t *buffer) { return buffer->input[buffer->loc]; }

int adv(buffer_t *buffer) {
  buffer->loc++;
  buffer->file_loc++;
  if (buffer->loc == buffer->cap) {
    if (!buffer->file) {
      return 2;
    }
    buffer->loc = 0;
    size_t n = fread(buffer->input, 1, buffer->cap, buffer->file);
    if (n < buffer->cap) {
      buffer->input[n - 1] = '\0';
      return 1;
    }
  }
  return 0;
}

int adv_expand(buffer_t *buffer) {
  buffer->loc++;
  buffer->file_loc++;
  if (buffer->loc == buffer->cap) {
    if (!buffer->file) {
      return 2;
    }
    buffer->input =
        reallocarray(buffer->input, buffer->cap << 1, sizeof(*buffer->input));
    if (!buffer->input) {
      err(1, "Failed to allocate memory for the file buffer");
    }
    size_t n = fread(buffer->input + buffer->loc, 1, buffer->cap, buffer->file);
    if (n < buffer->cap) {
      buffer->input[n - 1] = '\0';
      return 1;
    }
    buffer->cap <<= 1;
  }
  return 0;
}

inline int eof(buffer_t *buffer) {
  return (!buffer->file || feof(buffer->file)) && !buffer->input[buffer->loc + 1];
}

// void adv_preserving(buffer_t *buffer, size_t saved) {
//   buffer->loc++;
//   buffer->file_loc++;
//   if (buffer->loc == SIZE && saved) {
//     for (size_t i = 0; i < SIZE - saved; i++) {
//       buffer->loc = SIZE - saved;
//       char tmp = buffer->input[i];
//       buffer->input[i] = buffer->input[saved + i];
//       buffer->input[saved + i] = tmp;
//     }
//     buffer->input[SIZE - saved] = '\0';
//     size_t n = fread(buffer->input + SIZE - saved, 1, saved, buffer->file);
//     if (n < saved)
//       buffer->input[n - 1] = '\0';
//   }
// }
