#include "parser.h"
#include "errs.h"
#include "expr.h"
#include "exprs.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

parser_t *create_parser() {
  parser_t *parser = malloc(sizeof(parser_t));
  parser->input = 0;
  parser->loc = 0;
  parser->line = 1;
  parser->line_loc = 0;
  parser->errs = 0;
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

inline void retr_parser(parser_t *parser) {
  parser->line_loc--;
  parser->loc--;
}

inline void adv_parser(parser_t *parser) {
  parser->line_loc++;
  parser->loc++;
}

inline void adv_line(parser_t *parser) {
  parser->line++;
  parser->line_loc = 0;
  parser->loc++;
}

inline int cur_eof_parser(parser_t *parser) {
  return !parser->input[parser->loc];
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

void skip_to_nl(parser_t *parser) {
  while (get_parser(parser) != '\n' && get_parser(parser) != '\r') {
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

expr_t vec(parser_t *parser) {
  if (get_parser(parser) == ')') {
    adv_parser(parser);
    return (expr_t){.line = parser->line,
                    .loc = parser->line_loc,
                    .type = Vec,
                    .exprs = create_exprs(0)};
  }
  exprs_t *exprs = create_exprs(1);
  while (get_parser(parser) != ')') {
    if (eof_parser(parser)) {
      delete_exprs(exprs);
      return err_parser(parser, VecUnfinishedEof);
    }
    push_exprs(exprs, expr(parser));
    skip_space(parser);
  }
  adv_parser(parser);
  return (expr_t){.line = parser->line,
                  .loc = parser->line_loc,
                  .type = Vec,
                  .exprs = exprs};
}

expr_t special(parser_t *parser) {
  adv_parser(parser);
  switch (get_parser(parser)) {
  case '\\':
    adv_parser(parser);
    if (eof_parser(parser)) {
      return err_parser(parser, CharUnfinishedEof);
    }
    char c = get_parser(parser);
    adv_parser(parser);
    return (expr_t){
        .line = parser->line, .loc = parser->line_loc, .type = Chr, .ch = c};
  case 't':
    adv_parser(parser);
    return (expr_t){
        .line = parser->line, .loc = parser->line_loc, .type = Bool, .ch = 1};
  case 'f':
    adv_parser(parser);
    return (expr_t){
        .line = parser->line, .loc = parser->line_loc, .type = Bool, .ch = 0};
  case '(':
    adv_parser(parser);
    return vec(parser);
  case ')':
    adv_parser(parser);
    return err_parser(parser, ListUnmatchedRight);
  default:
    return err_parser(parser, SpecialUnfinishedEof);
  }
}

expr_t quote(parser_t *parser) {
  adv_parser(parser);
  if (get_parser(parser) == '(') {
    adv_parser(parser);
    if (get_parser(parser) == ')') {
      adv_parser(parser);
      return (expr_t){
          .line = parser->line, .loc = parser->line_loc, .type = Null, .ch = 0};
    }
  }
  return err_parser(parser, QuoteUnfinishedEof);
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
                .type = Str,
                .str = str_span(parser, stamp)};
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
                  .type = Sym,
                  .str = str_span(parser, stamp)};
}

expr_t lambda(parser_t *parser) {
  exprs_t *exprs = create_exprs(2);
  if (eof_parser(parser)) {
    delete_exprs(exprs);
    return err_parser(parser, ListUnfinishedEof);
  }
  push_exprs(exprs, (expr_t){.line = parser->line,
                             .loc = parser->line_loc,
                             .type = Sym,
                             .str = strdup("lambda")});
  adv_parser(parser);
  exprs_t *args = create_exprs(1);
  if (get_parser(parser) == ')') {
    adv_parser(parser);
    push_exprs(exprs, (expr_t){.line = parser->line,
                               .loc = parser->line_loc,
                               .type = List,
                               .exprs = args});
  } else {
    while (get_parser(parser) != ')') {
      if (eof_parser(parser)) {
        delete_exprs(exprs);
        return err_parser(parser, ListUnfinishedEof);
      }
      push_exprs(args, expr(parser));
      skip_space(parser);
    }
    adv_parser(parser);
    push_exprs(exprs, (expr_t){.line = parser->line,
                               .loc = parser->line_loc,
                               .type = List,
                               .exprs = args});
  }
  while (get_parser(parser) != ')') {
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

expr_t list(parser_t *parser) {
  adv_parser(parser);
  if (get_parser(parser) == ')') {
    adv_parser(parser);
    return err_parser(parser, EmptyList);
  }
  exprs_t *exprs = create_exprs(1);
  if (eof_parser(parser)) {
    delete_exprs(exprs);
    return err_parser(parser, ListUnfinishedEof);
  }
  expr_t name = expr(parser);
  skip_space(parser);
  if (name.type == Sym && !strcmp(name.str, "lambda")) {
    return lambda(parser);
  }
  push_exprs(exprs, name);
  while (get_parser(parser) != ')') {
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
  if (get_parser(parser) == '-') {
    adv_parser(parser);
    if (!isdigit(get_parser(parser))) {
      retr_parser(parser);
    }
  }
  while (isdigit(get_parser(parser))) {
    adv_parser(parser);
  }
  if (cur_eof_parser(parser) || isspace(get_parser(parser)) ||
      reserved(parser)) {
    char *tmp = str_span(parser, stamp);
    expr_t num = {.type = Num, .num = strtoll(tmp, 0, 10)};
    free(tmp);
    return num;
  } else {
    while (!eof_parser(parser) && !isspace(get_parser(parser)) &&
           !reserved(parser)) {
      adv_parser(parser);
    }
    return (expr_t){.line = parser->line,
                    .loc = parser->line_loc,
                    .type = Sym,
                    .str = str_span(parser, stamp)};
  }
}

expr_t expr(parser_t *parser) {
  switch (get_parser(parser)) {
  case '#':
    return special(parser);
  case '\'':
    return quote(parser);
  case '"':
    return str(parser);
  case '(':
    return list(parser);
  case '-':
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
