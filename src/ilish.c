#include "compiler.h"
#include "exprs.h"
#include "parser.h"
#include "strs.h"
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>

const size_t heap_size = 1024;

void compile_line(parser_t *parser, compiler_t *compiler, char *line) {
  exprs_t *exprs = parse(parser, line);
  if (has_err_parser(parser)) {
    print_errs(parser->errs);
  } else {
    strs_t *strs = compile(compiler, exprs, heap_size, line);
    if (has_errc(compiler)) {
      print_errs(compiler->errs);
      puts("");
    } else {
      print_lined_strs(strs);
    }
    delete_strs(strs);
  }
  free(line);
}

void repl(parser_t *parser, compiler_t *compiler) {
  char *line;
  size_t n = 0;
  printf("> ");
  while (getline(&line, &n, stdin)) {
    exprs_t *exprs = parse(parser, strdup(line));
    if (has_err_parser(parser)) {
      print_errs(parser->errs);
      printf("\n> ");
    } else {
      strs_t *strs = compile(compiler, exprs, heap_size, line);
      if (has_errc(compiler)) {
        print_errs(compiler->errs);
        printf("\n> ");
      } else {
        print_lined_strs(strs);
        printf("> ");
      }
      delete_strs(strs);
    }
  }
  free(line);
}

void compile_file(parser_t *parser, compiler_t *compiler, int file) {
  struct stat statbuf;
  fstat(file, &statbuf);
  char *src = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, file, 0);
  exprs_t *exprs = parse(parser, src);
  if (has_err_parser(parser)) {
    print_lined_errs(parser->errs);
  } else {
    strs_t *strs = compile(compiler, exprs, heap_size, src);
    if (has_errc(compiler)) {
      print_lined_errs(compiler->errs);
    } else {
      print_lined_strs(strs);
    }
    delete_strs(strs);
  }
  munmap(src, statbuf.st_size);
}

// TODO: Implement Multiple Files
// This will likely require extra linking processing and might as well implement
// libraries at that point, so this is not top priority at the moment.
int main(int argc, char *argv[]) {
  parser_t *parser = create_parser();
  compiler_t *compiler = create_compiler();
  switch (argc) {
  case 1:
    repl(parser, compiler);
    break;
  case 2:
    if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "--help") ||
        !strcmp(argv[1], "help")) {
      puts("Use -e to compile a passed in string or -f to compile file(s).");
    } else {
      puts("Unknown Argument, See help");
    }
    break;
  case 3:
    if (!strcmp(argv[1], "-f")) {
      int file = open(argv[2], O_RDONLY);
      if (file) {
        compile_file(parser, compiler, file);
      } else {
        puts("Failed to Read a File");
      }
    } else if (!strcmp(argv[1], "-e")) {
      compile_line(parser, compiler, strdup(argv[2]));
    } else {
      puts("Unknown Argument, See help");
    }
    break;
  default:
    if (!strcmp(argv[1], "-f")) {
      for (int i = 2; i < argc; i++) {
        int file = open(argv[i], O_RDONLY);
        if (file) {
          compile_file(parser, compiler, file);
        } else {
          printf("Failed to Read the File: %s\n", argv[i]);
        }
      }
    } else {
      puts("Unknown Argument, See help");
    }
    break;
  }
  delete_parser(parser);
  delete_compiler(compiler);
  return 0;
}
