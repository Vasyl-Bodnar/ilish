/// Parses basic sexprs with the following regex-like grammar
/// program := expr+EOF
/// expr := char|num|str|symb|list
/// char := #\.
/// num := ([0-9]+)space*
/// str := "[^"]*"
/// symb := ([^"'\(\)\[\]{}]*)space*
/// list := \((space*)expr+\)
/// space := [\t\n\r\n]
#include "parser.h"
#include "errs.h"
#include "expr.h"
#include "exprs.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

parser_t *create_parser() {
  parser_t *parser = malloc(sizeof(parser_t));
  parser->input = NULL;
  parser->loc = 0;
  parser->line = 1;
  parser->line_loc = 0;
  parser->errs = NULL;
  return parser;
}

void delete_parser(parser_t *parser) {
  if (parser->errs)
    delete_errs(parser->errs);
  free(parser);
}

inline int has_err_parser(parser_t *parser) { return parser->errs->len > 0; }

expr_t err_parser(parser_t *parser, enum err err) {
  push_errs(
      parser->errs,
      (err_t){.type = err, .line = parser->line, .loc = parser->line_loc});
  return (expr_t){.type = Err, .ch = err};
}

inline char get_parser(parser_t *parser) { return parser->input[parser->loc]; }

inline void adv_parser(parser_t *parser) {
  parser->line_loc++;
  parser->loc++;
}

inline void adv_line(parser_t *parser) {
  parser->line++;
  parser->line_loc = 0;
  parser->loc++;
}

inline int eof_parser(parser_t *parser) {
  return !parser->input[parser->loc + 1];
}

int reserved(parser_t *parser) {
  char c = get_parser(parser);
  return c == '(' || c == ')' || c == '"' || c == '[' || c == ']' || c == '{' ||
         c == '}' || c == ';';
}

inline char *str_span(parser_t *parser, size_t stamp) {
  return strndup(parser->input + stamp, parser->loc - stamp);
}

inline span_t span(parser_t *parser, size_t stamp) {
  return (span_t){stamp, parser->loc - stamp};
}

void skip_to_nl(parser_t *parser) {
  while (get_parser(parser) != '\r' && get_parser(parser) != '\n') {
    adv_parser(parser);
  }
}

void skip_space(parser_t *parser) {
  while (get_parser(parser) == ' ' || get_parser(parser) == '\t') {
    adv_parser(parser);
  }
  if (get_parser(parser) == '\n' || get_parser(parser) == '\r') {
    adv_line(parser);
    skip_space(parser);
  }
}

expr_t ch(parser_t *parser) {
  adv_parser(parser);
  if (!eof_parser(parser)) {
    if (get_parser(parser) == '\\') {
      adv_parser(parser);
      char c = get_parser(parser);
      adv_parser(parser);
      return (expr_t){
          .line = parser->line, .loc = parser->line_loc, .type = Char, .ch = c};
    }
  }
  return err_parser(parser, CharUnfinishedEof);
}

expr_t str(parser_t *parser) {
  adv_parser(parser);
  size_t stamp = parser->loc;
  while (get_parser(parser) != '"') {
    if (eof_parser(parser)) {
      return err_parser(parser, StrUnfinishedEof);
    }
    adv_parser(parser);
  }
  expr_t str = {.line = parser->line,
                .loc = parser->line_loc,
                .type = StrSpan,
                .span = span(parser, stamp)};
  adv_parser(parser);
  return str;
}

expr_t symb(parser_t *parser) {
  size_t stamp = parser->loc;
  adv_parser(parser);
  while (!eof_parser(parser) && !isspace(get_parser(parser)) &&
         !reserved(parser)) {
    adv_parser(parser);
  }
  return (expr_t){.line = parser->line,
                  .loc = parser->line_loc,
                  .type = SymbSpan,
                  .span = span(parser, stamp)};
}

expr_t list(parser_t *parser, char end) {
  adv_parser(parser);
  if (get_parser(parser) == end) {
    adv_parser(parser);
    return err_parser(parser, EmptyList);
  }
  exprs_t *exprs = create_exprs(4);
  while (get_parser(parser) != end) {
    if (eof_parser(parser)) {
      delete_exprs(exprs);
      return err_parser(parser, ListUnfinishedEof);
    }
    push_exprs(exprs, expr(parser));
    skip_space(parser);
  }
  adv_parser(parser);
  return (expr_t){.line = parser->line,
                  .loc = parser->line_loc,
                  .type = List,
                  .exprs = exprs};
}

expr_t num(parser_t *parser) {
  size_t stamp = parser->loc;
  while (isdigit(get_parser(parser))) {
    adv_parser(parser);
  }
  if (eof_parser(parser) || isspace(get_parser(parser)) || reserved(parser)) {
    char *tmp = str_span(parser, stamp);
    expr_t num = {.type = Num, .num = strtoll(tmp, NULL, 10)};
    free(tmp);
    return num;
  } else {
    while (!eof_parser(parser) && !isspace(get_parser(parser)) &&
           !reserved(parser)) {
      adv_parser(parser);
    }
    return (expr_t){.line = parser->line,
                    .loc = parser->line_loc,
                    .type = SymbSpan,
                    .span = span(parser, stamp)};
  }
}

expr_t expr(parser_t *parser) {
  switch (get_parser(parser)) {
  case '#':
    return ch(parser);
  case '"':
    return str(parser);
  case '(':
    return list(parser, ')');
  case '[':
    return list(parser, ']');
  case '{':
    return list(parser, '}');
  case '0':
  case '1':
  case '2':
  case '3':
  case '4':
  case '5':
  case '6':
  case '7':
  case '8':
  case '9':
    return num(parser);
  case ';':
    skip_to_nl(parser);
    return expr(parser);
  case '\n':
  case '\r':
    adv_line(parser);
    return expr(parser);
  case ' ':
  case '\t':
    adv_parser(parser);
    return expr(parser);
  case ')':
  case ']':
  case '}':
    adv_parser(parser);
    return err_parser(parser, ListUnmatchedRight);
  case '\0':
    return (expr_t){.type = Err, .ch = Eof};
  default:
    return symb(parser);
  }
}

exprs_t *parse(parser_t *parser, const char *input) {
  if (parser->errs)
    delete_errs(parser->errs);
  parser->input = input;
  parser->loc = 0;
  parser->line = 1;
  parser->line_loc = 0;
  parser->errs = create_errs(3);
  exprs_t *exprs = create_exprs(4);
  while (1) {
    expr_t ex = expr(parser);
    if (ex.type == Err && ex.ch == Eof) {
      break;
    }
    push_exprs(exprs, ex);
    if (eof_parser(parser)) {
      break;
    }
    skip_space(parser);
  }
  return exprs;
}
