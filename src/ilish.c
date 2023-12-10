#include "buffer.h"
#include "compiler.h"
#include "exprs.h"
#include "parser.h"
#include "strs.h"
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

void compile_line(parser_t *parser, compiler_t *compiler, char *line) {
  buffer_t *buffer = create_buffer(strdup(line));
  exprs_t *exprs = parse(parser, buffer);
  if (has_err_parser(parser)) {
    print_errs(parser->errs);
  } else {
    strs_t *strs = compile(compiler, exprs, line);
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
    buffer_t *buffer = create_buffer(strdup(line));
    exprs_t *exprs = parse(parser, buffer);
    if (has_err_parser(parser)) {
      print_errs(parser->errs);
      printf("\n> ");
    } else {
      strs_t *strs = compile(compiler, exprs, line);
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

void compile_file(parser_t *parser, compiler_t *compiler, FILE *file) {
  buffer_t *buffer = create_file_buffer(file);
  exprs_t *exprs = parse(parser, buffer);
  if (has_err_parser(parser)) {
    print_lined_errs(parser->errs);
  } else {
    //print_exprs(exprs, "");
    if (!compiler)
      puts("yes, what");
    // strs_t *strs = compile(compiler, exprs, buff);
    // if (has_errc(compiler)) {
    //  print_lined_errs(compiler->errs);
    //} else {
    // print_lined_strs(strs);
    //}
    // delete_strs(strs);
  }
  delete_exprs(exprs);
}

// WIP
// void compile_pipe(parser_t *parser, compiler_t *compiler) {
// char buff[1024];
// char *line = NULL;
// size_t size = 0;
// size_t n;
//
// while ((n = fread(buff, 1, sizeof(buff), stdin))) {
// char *new_buff = realloc(line, size + n);
// if (!new_buff) {
// puts("Memory allocation failed");
// free(line);
// return;
// }
// memcpy(new_buff + size, buff, n);
// line = new_buff;
// size += n;
// exprs_t *exprs = parse(parser, line);
// if (has_err_parser(parser)) {
// print_errs(parser->errs);
// } else {
// compile(compiler, exprs, line);
// }
// free(line);
// size = 0;
// }
// }

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
      FILE *file = fopen(argv[2], "r");
      if (file) {
        compile_file(parser, compiler, file);
        fclose(file);
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
