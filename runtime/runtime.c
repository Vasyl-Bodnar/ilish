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
    gen0_begin = malloc(heap_size);               // 0.125
    gen0_ptr = gen0_begin;                        //
    gen0_tospace = gen0_begin + (heap_size >> 3); // +0.125

    gen1_begin = gen0_begin + (heap_size >> 3);                      // 0.375
    gen1_ptr = gen1_begin;                                           //
    gen1_tospace = gen1_begin + (heap_size >> 2) + (heap_size >> 3); // +0.375
  }

  if (!rs_begin) {
    rs_begin = malloc(rs_size);
  }
}

int exists_root(size_t **rs_ptr, size_t target) {
  for (size_t i = 0; i < (size_t)(rs_ptr - rs_begin); i++) {
    if (target == (size_t)rs_begin[i])
      return 1;
  }
  return 0;
}

/// Copying collection (Single gen for now)
void collect(size_t **rs_ptr, size_t request) {
  // 0. CHECK
  if (gen0_tospace > gen0_begin
          ? ((size_t)gen0_ptr) + request < (size_t)gen0_tospace
          : (size_t)gen0_ptr - (size_t)gen0_begin + request >
                (size_t)(gen0_begin - gen0_tospace)) {
    return;
  }
  // 1. Copy all objs pointed to in rs to tospace.
  gen0_ptr = gen0_tospace;
  for (size_t i = 0;
       i < (size_t)(rs_ptr < rs_begin ? rs_begin - rs_ptr : rs_ptr - rs_begin);
       i++) {
    switch ((size_t)(rs_begin[i]) & 0x7) {
    case 1: // Pair
      memcpy(gen0_ptr, rs_begin[i], sizeof(size_t) * 2);
      gen0_ptr += sizeof(size_t) * 2;
      break;
    case 2: // Vec
      memcpy(gen0_ptr, rs_begin[i],
             sizeof(size_t) + (sizeof(size_t) >> 2) * rs_begin[i][0]);
      gen0_ptr += sizeof(size_t) + (sizeof(size_t) >> 2) * rs_begin[i][0];
      break;
    case 3: // Str
      memcpy(gen0_ptr, rs_begin[i], sizeof(size_t) + (rs_begin[i][0] >> 3));
      gen0_ptr += sizeof(size_t) + (rs_begin[i][0] >> 3);
      break;
    case 5: // Symb
      break;
    case 6: // Lamb
      memcpy(gen0_ptr, rs_begin[i],
             sizeof(size_t) + sizeof(size_t) * rs_begin[i][0]);
      gen0_ptr += sizeof(size_t) + sizeof(size_t) * rs_begin[i][0];
      break;
    default:
      break;
    }
  }
  // TODO 2. Navigate tospace and Keep track of copied objects using rs.
  for (size_t i = 0; i < (size_t)(gen0_ptr - gen0_tospace);
       i += sizeof(size_t)) {
    switch (gen0_tospace[i] & 0x7) {
    case 1: // Pair
      if (!exists_root(rs_ptr, gen0_tospace[i])) {
        memcpy(gen0_ptr, (size_t *)gen0_tospace[i], sizeof(size_t) * 2);
        gen0_ptr += sizeof(size_t) * 2;
      }
      break;
    case 2: // Vec
      if (!exists_root(rs_ptr, gen0_tospace[i])) {
        memcpy(gen0_ptr, (size_t *)gen0_tospace[i],
               sizeof(size_t) +
                   (sizeof(size_t) >> 2) * ((size_t *)gen0_tospace[i])[0]);
        gen0_ptr += sizeof(size_t) +
                    (sizeof(size_t) >> 2) * ((size_t *)gen0_tospace[i])[0];
      }
      break;
    case 3: // Str
      if (!exists_root(rs_ptr, gen0_tospace[i])) {
        memcpy(gen0_ptr, (size_t *)gen0_tospace[i],
               sizeof(size_t) + (((size_t *)gen0_tospace[i])[0] >> 3));
        gen0_ptr += sizeof(size_t) + (((size_t *)gen0_tospace[i])[0] >> 3);
      }
      break;
    case 5: // Symb
      break;
    case 6: // Lamb
      if (!exists_root(rs_ptr, gen0_tospace[i])) {
        memcpy(gen0_ptr, (size_t *)gen0_tospace[i],
               sizeof(size_t) +
                   sizeof(size_t) * ((size_t *)gen0_tospace[i])[0]);
        gen0_ptr +=
            sizeof(size_t) + sizeof(size_t) * ((size_t *)gen0_tospace[i])[0];
      }
      break;
    default:
      break;
    }
  }
  // TODO 3. tospace = fromspace.
  size_t *tmp = gen0_begin;
  gen0_begin = gen0_tospace;
  gen0_tospace = tmp;
  // TODO 4. req satisfied? no? GEN1 you GO TO!
  if (request > (size_t)(gen0_tospace < gen0_ptr ? gen0_tospace - gen0_ptr
                                                 : gen0_ptr - gen0_tospace)) {
    if (gen0_tospace > gen0_begin) {
      free(gen0_begin);
    } else {
      free(gen0_tospace);
    }
    free(rs_begin);
    exit(1);
  }
  //  TODO 5. gen1 anyone?
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
