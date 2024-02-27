/// Generational Copying GC Runtime
/// Also currently implements print as a last program statement
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MIN(x, y) ((x > y) ? y : x)

char *gen0_begin = 0;
char *gen0_ptr;
char *gen0_tospace;
char *gen1_begin;
char *gen1_ptr;
char *gen1_tospace;
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
    gen0_begin = (char *)1;
  } else {
    free(gen0_tospace);
    gen0_tospace = (char *)1;
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

/// TODO: These are simplified algorithms, they do not track pointers
void copy(size_t **rs_ptr, char **ptr) {
  for (size_t i = 0; i < (size_t)(rs_ptr - rs_begin); i++) {
    switch ((size_t)(rs_begin[i]) & 0x7) {
    case 1: // Pair
      memcpy(*ptr, (void *)(((size_t)rs_begin[i]) - 1), sizeof(size_t) * 2);
      rs_begin[i] = (size_t *)(*ptr + 1);
      *ptr += sizeof(size_t) * 2;
      break;
    case 2: // Vec
      memcpy(*ptr, (void *)(((size_t)rs_begin[i]) - 2),
             sizeof(size_t) + (sizeof(size_t) >> 2) *
                                  ((size_t *)(((size_t)rs_begin[i]) - 2))[0]);
      rs_begin[i] = (size_t *)(*ptr + 2);
      *ptr += sizeof(size_t) + (sizeof(size_t) >> 2) *
                                   ((size_t *)(((size_t)rs_begin[i]) - 2))[0];
      break;
    case 3: // Str
      memcpy(*ptr, (void *)(((size_t)rs_begin[i]) - 3),
             sizeof(size_t) +
                 (((size_t *)(((size_t)rs_begin[i]) - 3))[0] >> 3));
      rs_begin[i] = (size_t *)(*ptr + 3);
      *ptr +=
          sizeof(size_t) + (((size_t *)(((size_t)rs_begin[i]) - 3))[0] >> 3);
      break;
    case 5: // Symb
      break;
      // FIX: The way Lamb is allocated contains no len data anymore
    case 6: // Lamb
      memcpy(*ptr, (void *)(((size_t)rs_begin[i]) - 6),
             sizeof(size_t) + sizeof(size_t)); // * len like in Vec or Str
      rs_begin[i] = (size_t *)(*ptr + 6);
      *ptr += sizeof(size_t) + sizeof(size_t);
      break;
    default:
      break;
    }
  }
}

/// Copying collection
/// NOTE: This version seems to work,
/// but is still not tested as heavily as I would like
void collect(size_t **rs_ptr, size_t request) {
  // 0. Check
  size_t gen0_left = ((gen0_tospace > gen0_begin)
                          ? (gen0_tospace - gen0_ptr)
                          : ((MIN(gen1_begin, gen1_tospace)) - gen0_ptr));
  if (request >= gen0_left) {
    // 1. Copy all objs pointed to in rs to tospace.
    // 2. Navigate tospace and Keep track of copied objects using rs.
    gen0_ptr = gen0_tospace;
    copy(rs_ptr, &gen0_ptr);
    // 3. tospace = fromspace.
    char *tmp = gen0_begin;
    gen0_begin = gen0_tospace;
    gen0_tospace = tmp;
    // NOTE: Current implementation does not account for gen1 holding data
    // pointing to gen0, this would probably require an additional set to hold
    // these generation breaking pointers.
    gen0_left = ((gen0_tospace > gen0_begin)
                     ? (gen0_tospace - gen0_ptr)
                     : ((MIN(gen1_begin, gen1_tospace)) - gen0_ptr));
    if (request >= gen0_left) {
      size_t gen1_left =
          ((gen1_tospace > gen1_begin)
               ? (gen1_tospace - gen1_ptr)
               : ((gen1_begin - gen1_tospace) - (gen1_ptr - gen1_begin)));
      size_t gen0_size =
          ((gen0_tospace > gen0_begin) ? (gen0_tospace - gen0_begin)
                                       : (gen0_begin - gen0_tospace));
      // Copies all gen0 into gen1 and resets gen0
      if ((request + gen0_size) >= gen1_left) {
        // Repeat procedure for gen1
        gen1_ptr = gen1_tospace;
        copy(rs_ptr, &gen1_ptr);
        char *tmp = gen1_begin;
        gen1_begin = gen1_tospace;
        gen1_tospace = tmp;
        size_t gen1_size =
            ((gen1_tospace > gen1_begin) ? (gen1_tospace - gen1_begin)
                                         : (gen1_begin - gen1_tospace));
        if ((request + gen0_size) >= gen1_size) {
          puts("Not Enough Space on the Major Heap! "
               "Please "
               "Allocate a Larger Heap.");
          cleanup();
          exit(1);
        }
      } else {
        // Just copy, there is enough space
        copy(rs_ptr, &gen1_ptr);
      }
      gen0_ptr = gen0_begin;
      if (request >= gen0_size) {
        puts("Not Enough Space on the Minor Heap to Allocate this Object! "
             "Please "
             "Allocate a Larger Heap.");
        cleanup();
        exit(1);
      }
    }
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
