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

const size_t heap_size = 128;

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
    // compiler->loc = 0;
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

// BROKEN, technically completely unnecessery features
void compile_pipe(parser_t *parser, compiler_t *compiler) {
  char *buff = mmap(0, 1024, PROT_READ, MAP_SHARED, (size_t)stdin, 0);
  exprs_t *exprs = parse(parser, buff);
  if (has_err_parser(parser)) {
    print_lined_errs(parser->errs);
  } else {
    strs_t *strs = compile(compiler, exprs, heap_size, buff);
    if (has_errc(compiler)) {
      print_lined_errs(compiler->errs);
    } else {
      print_lined_strs(strs);
    }
    delete_strs(strs);
  }
  munmap(buff, 1024);
}

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
      puts("Help is relative");
    } else if (!strcmp(argv[1], "-p")) {
      // compile_pipe(parser, compiler);
    } else {
      puts("Unknown Argument");
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
      puts("Unknown Argument");
    }
    break;
  default:
    puts("Too Many Argument");
  }
  delete_parser(parser);
  delete_compiler(compiler);
  return 0;
}
