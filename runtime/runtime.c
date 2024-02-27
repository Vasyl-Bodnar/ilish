/// Generational Copying GC Runtime
/// Also currently implements print as a last program statement
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t *gen0_begin = 0;
size_t *gen0_ptr;
size_t *gen0_tospace;
size_t *gen1_begin;
size_t *gen1_ptr;
size_t *gen1_tospace;
size_t **rs_begin = 0;

/// One time allocation with distributed heaps for both generation.
// HACK: !gen0_begin, !rs_begin may seem like pointless checks, but for some
// reason the executable runs this function at the end of the program, while
// also running it at the start as is the expected behaviour.
// Thus, they are set to 0 at the start of the program, and to 1 when cleanup is
// called which ensures no magic 77KB mallocs when a unit vector program is
// already long complete.
void init_gc(size_t rs_size, size_t heap_size) {
  if (!gen0_begin) {
    gen0_begin = malloc(heap_size);
    gen0_ptr = gen0_begin; // 0.125
    gen0_tospace = gen0_begin + (heap_size >> 3);
    // 0.125
    gen1_begin = gen0_tospace + (heap_size >> 3);
    gen1_ptr = gen1_begin; // 0.375
    gen1_tospace = gen1_begin + (heap_size >> 2) + (heap_size >> 3);
    // 0.375
  }

  if (!rs_begin) {
    rs_begin = malloc(rs_size);
  }
}

void cleanup() {
  if (gen0_tospace > gen0_begin) {
    free(gen0_begin);
    gen0_begin = (size_t *)1;
  } else {
    free(gen0_tospace);
    gen0_tospace = (size_t *)1;
  }
  free(rs_begin);
  rs_begin = (size_t **)1;
}

int exists_root(size_t **rs_ptr, size_t target) {
  for (size_t i = 0; i < (size_t)(rs_ptr - rs_begin); i++) {
    if (target == (size_t)rs_begin[i])
      return 1;
  }
  return 0;
}

void simple_switch(size_t **rs_ptr, size_t **ptr, size_t *tospace) {
  *ptr = tospace;
  for (size_t i = 0;
       i < (size_t)(rs_ptr < rs_begin ? rs_begin - rs_ptr : rs_ptr - rs_begin);
       i++) {
    switch ((size_t)(rs_begin[i]) & 0x7) {
    case 1: // Pair
      memcpy(*ptr, rs_begin[i], sizeof(size_t) * 2);
      *ptr += sizeof(size_t) * 2;
      break;
    case 2: // Vec
      memcpy(*ptr, rs_begin[i],
             sizeof(size_t) + (sizeof(size_t) >> 2) * rs_begin[i][0]);
      *ptr += sizeof(size_t) + (sizeof(size_t) >> 2) * rs_begin[i][0];
      break;
    case 3: // Str
      memcpy(*ptr, rs_begin[i], sizeof(size_t) + (rs_begin[i][0] >> 3));
      *ptr += sizeof(size_t) + (rs_begin[i][0] >> 3);
      break;
    case 5: // Symb
      break;
    case 6: // Lamb
      memcpy(*ptr, rs_begin[i],
             sizeof(size_t) + sizeof(size_t) * rs_begin[i][0]);
      *ptr += sizeof(size_t) + sizeof(size_t) * rs_begin[i][0];
      break;
    default:
      break;
    }
  }
  for (size_t i = 0; i < (size_t)(*ptr - tospace); i += sizeof(size_t)) {
    switch (tospace[i] & 0x7) {
    case 1: // Pair
      if (!exists_root(rs_ptr, tospace[i])) {
        memcpy(*ptr, (size_t *)tospace[i], sizeof(size_t) * 2);
        *ptr += sizeof(size_t) * 2;
      }
      break;
    case 2: // Vec
      if (!exists_root(rs_ptr, tospace[i])) {
        memcpy(*ptr, (size_t *)tospace[i],
               sizeof(size_t) +
                   (sizeof(size_t) >> 2) * ((size_t *)tospace[i])[0]);
        *ptr +=
            sizeof(size_t) + (sizeof(size_t) >> 2) * ((size_t *)tospace[i])[0];
      }
      break;
    case 3: // Str
      if (!exists_root(rs_ptr, tospace[i])) {
        memcpy(*ptr, (size_t *)tospace[i],
               sizeof(size_t) + (((size_t *)tospace[i])[0] >> 3));
        *ptr += sizeof(size_t) + (((size_t *)tospace[i])[0] >> 3);
      }
      break;
    case 5: // Symb
      break;
    case 6: // Lamb
      if (!exists_root(rs_ptr, tospace[i])) {
        memcpy(*ptr, (size_t *)tospace[i],
               sizeof(size_t) + sizeof(size_t) * ((size_t *)tospace[i])[0]);
        *ptr += sizeof(size_t) + sizeof(size_t) * ((size_t *)tospace[i])[0];
      }
      break;
    default:
      break;
    }
  }
}

/// Copying collection
/// NOTE: It is heavy WIP and is mostly untested, it is far too likely that this
/// implementation simply does not work and is no different from just an
/// unstable arena allocator with a free at the end of the program
void collect(size_t **rs_ptr, size_t request) {
  // 0. Check
  size_t gen0_difference =
      ((gen0_tospace > gen0_ptr) ? gen0_tospace - gen0_ptr
                                 : gen0_ptr - gen0_tospace);
  if (request < gen0_difference) {
    return;
  }
  // 1. Copy all objs pointed to in rs to tospace.
  // 2. Navigate tospace and Keep track of copied objects using rs.
  simple_switch(rs_ptr, &gen0_ptr, gen0_tospace);
  // 3. tospace = fromspace.
  size_t *tmp = gen0_begin;
  gen0_begin = gen0_tospace;
  gen0_tospace = tmp;
  // 4. request not satisfied? gen1!
  // NOTE: Current implementation does not account for gen1 holding data
  // pointing to gen1, this would probably require an additional set to hold
  // these generation breaking pointers.
  gen0_difference = ((gen0_tospace > gen0_ptr) ? gen0_tospace - gen0_ptr
                                               : gen0_ptr - gen0_tospace);
  if (request > gen0_difference) {
    size_t gen1_difference =
        ((gen1_tospace > gen1_ptr) ? gen1_tospace - gen1_ptr
                                   : gen1_ptr - gen1_tospace);
    if ((request + gen0_difference) > gen1_difference) {
      simple_switch(rs_ptr, &gen1_ptr, gen1_tospace);
      size_t *tmp = gen1_begin;
      gen1_begin = gen1_tospace;
      gen1_tospace = tmp;
    }
    simple_switch(rs_ptr, &gen0_ptr, gen1_begin);
    gen0_ptr = (gen0_tospace > gen0_begin) ? gen0_begin : gen0_tospace;
  }
}

void print(size_t val) {
  if (val == 31) { // Bool
    printf("#f");
  } else if (val == 159) { // Bool
    printf("#t");
  } else if (val == 47) { // Nil
    printf("()");
  } else if ((val & 0x0f) == 15) { // Char
    printf("#\\x%zx", val >> 8);
  } else if ((val & 7) == 1) { // Cons
    printf("(");
    print(*(size_t *)(val - 1));
    while (((*(size_t *)(val + 7)) & 3) == 1) {
      printf(" ");
      val = *(size_t *)(val + 7);
      print(*(size_t *)(val - 1));
    }
    if ((*(size_t *)(val + 7)) != 47) {
      printf(" . ");
      print(*(size_t *)(val + 7));
    }
    printf(")");
  } else if ((val & 7) == 2) { // Vec
    printf("#(");
    for (size_t len = (*(size_t *)(val - 2)) >> 2, i = 0; i < len; i++) {
      print(*(size_t *)(val + 6 + (8 * i)));
      if (i != len - 1) {
        printf(" ");
      }
    }
    printf(")");
  } else if ((val & 7) == 3) { // String
    printf("\"");
    for (size_t len = (*(size_t *)(val - 3)) >> 3, i = 0; i < len; i++) {
      printf("%c", (char)*(size_t *)(val + 5 + i));
    }
    printf("\"");
  } else if ((val & 7) == 6) { // Lambda
    printf("<Lambda>(ref=0x%zx, arity=%zu)", val + 2, *((size_t *)(val - 6)));
  } else if (!(val & 3)) { // Fixnum
    printf("%zd", ((ssize_t)val >> 2));
  }
}
