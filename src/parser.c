/// Recursive Descent Parser for Scheme R7RS
/// This is not a complete version with a few less important features
/// not being implemented like block comments, string escapes, and
/// full reserve list
///
/// NOTE: There is a number of edge cases, even those noted by R7RS which are
/// currently not handled.
#include "parser.h"
#include "errs.h"
#include "expr.h"
#include "exprs.h"
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define produce_expr(token_type, val_type, value)                              \
  (expr_t) {                                                                   \
    .line = parser->line, .loc = parser->line_loc, .type = token_type,         \
    .val_type = value                                                          \
  }

parser_t *create_parser() {
  parser_t *parser = malloc(sizeof(*parser));
  parser->src = 0;
  parser->line = 0;
  parser->line_loc = 0;
  parser->old_line_loc = 0;
  parser->cursor = 0;
  parser->quote = Normal;
  parser->errs = 0;
  return parser;
}

void delete_parser(parser_t *parser) {
  if (parser->errs) {
    delete_errs(parser->errs);
  }
  free(parser);
}

void err_parser(parser_t *parser, enum err err) {
  push_errs(
      parser->errs,
      (err_t){.line = parser->line, .loc = parser->line_loc, .type = err});
}

int has_err_parser(parser_t *parser) { return parser->errs->len; }

/// NOTE: Advance stalls if it reaches null
void adv_parser(parser_t *parser) {
  if (parser->src[parser->cursor]) {
    parser->cursor++;
    parser->line_loc++;
    if (parser->src[parser->cursor] == '\n') {
      parser->line++;
      parser->old_line_loc = parser->line_loc;
      parser->line_loc = 0;
    }
  }
}

/// NOTE: line_loc "breaks" if too much rewinding is done, so it should
/// preferably be used for one char at most at a time.
/// NOTE: Rewind stalls if it reaches the first character
void rew_parser(parser_t *parser) {
  if (parser->cursor) {
    parser->cursor--;
    parser->line_loc--;
    if (parser->src[parser->cursor] == '\n') {
      parser->line--;
      parser->line_loc = parser->old_line_loc;
    }
  }
}

char get_parser(parser_t *parser) { return parser->src[parser->cursor]; }
char get_next_parser(parser_t *parser) {
  return parser->src[parser->cursor + 1];
}

char eat_parser(parser_t *parser) {
  char ch = parser->src[parser->cursor];
  adv_parser(parser);
  return ch;
}

void skip_whitespace_parser(parser_t *parser) {
  while (isspace(get_parser(parser))) {
    adv_parser(parser);
  }
}

void comment(parser_t *parser) {
  while (get_parser(parser) != '\n') {
    adv_parser(parser);
  }
}

int reserved_next_parser(parser_t *parser) {
  return !isspace(get_next_parser(parser)) && get_next_parser(parser) != '(' &&
         get_next_parser(parser) != ')' && get_next_parser(parser) != '#' &&
         get_next_parser(parser) != '\'' && get_next_parser(parser) != '`' &&
         get_next_parser(parser) != '"' && get_next_parser(parser) != ';' &&
         get_next_parser(parser);
}

int reserved_parser(parser_t *parser) {
  return !isspace(get_parser(parser)) && get_parser(parser) != '(' &&
         get_parser(parser) != ')' && get_parser(parser) != '#' &&
         get_parser(parser) != '\'' && get_parser(parser) != '`' &&
         get_parser(parser) != '"' && get_parser(parser) != ';' &&
         get_parser(parser);
}

void until_reserved_parser(parser_t *parser) {
  while (reserved_parser(parser)) {
    adv_parser(parser);
  }
}

void until_char_parser(parser_t *parser, char ch, enum err err) {
  while (get_parser(parser) != ch) {
    if (!get_parser(parser)) {
      err_parser(parser, err);
      return;
    }
    adv_parser(parser);
  }
}

void expr_until_char_parser(parser_t *parser, exprs_t *output, char ch,
                            enum err err) {
  while (get_parser(parser) != ch) {
    if (!get_parser(parser)) {
      err_parser(parser, err);
      return;
    }
    expr(parser, output);
  }
}

char *slice_parser(parser_t *parser, size_t start) {
  return strndup(parser->src + start, parser->cursor - start);
}

char *slice_ci_parser(parser_t *parser, size_t start) {
  char *str = strndup(parser->src + start, parser->cursor - start);
  for (size_t i = 0; i < parser->cursor - start + 1; i++) {
    str[i] = tolower(str[i]);
  }
  return str;
}

void keyword_parsed(parser_t *parser, exprs_t *output, const char *keyword) {
  exprs_t *quote = create_exprs(2);
  push_exprs(quote, produce_expr(Symb, str, strdup(keyword)));
  push_exprs(quote, output->arr[output->len - 1]);
  output->arr[output->len - 1].type = List;
  output->arr[output->len - 1].exprs = quote;
}

void quote_parsed(parser_t *parser, exprs_t *output) {
  switch (parser->quote) {
  case Normal:
    return;
  case Quote:
    if (output->arr[output->len - 1].type != Null) {
      keyword_parsed(parser, output, "quote");
    }
    break;
  case QuasiQuote:
    if (output->arr[output->len - 1].type != Null) {
      keyword_parsed(parser, output, "quasiquote");
    }
    break;
  }
}

void unquote(parser_t *parser, exprs_t *output) {
  adv_parser(parser);
  if (get_parser(parser) == '@') {
    adv_parser(parser);
    if (parser->quote != Normal) {
      expr(parser, output);
      keyword_parsed(parser, output, "unquote-splicing");
    } else {
      err_parser(parser, SpliceOutsideQuote);
    }
  } else {
    if (parser->quote != Normal) {
      expr(parser, output);
      keyword_parsed(parser, output, "unquote");
    } else {
      err_parser(parser, UnquoteOutsideQuote);
    }
  }
}

void list(parser_t *parser, exprs_t *output) {
  adv_parser(parser);
  if (get_parser(parser) != ')') {
    exprs_t *list = create_exprs(1);
    expr_until_char_parser(parser, list, ')', ListUnfinishedEof);
    adv_parser(parser);
    push_exprs(output, produce_expr(List, exprs, list));
  } else {
    adv_parser(parser);
    push_exprs(output, produce_expr(Null, num, 0));
  }
}

void non_empty_list(parser_t *parser, exprs_t *output) {
  adv_parser(parser);
  if (get_parser(parser) != ')') {
    exprs_t *list = create_exprs(1);
    expr_until_char_parser(parser, list, ')', ListUnfinishedEof);
    adv_parser(parser);
    push_exprs(output, produce_expr(List, exprs, list));
  } else {
    err_parser(parser, EmptyList);
  }
}

void string(parser_t *parser, exprs_t *output) {
  adv_parser(parser);
  size_t start = parser->cursor;
  until_char_parser(parser, '"', StrUnfinishedEof);
  char *str = slice_parser(parser, start);
  adv_parser(parser);
  push_exprs(output, produce_expr(Str, str, str));
}

void boolean(parser_t *parser, exprs_t *output, int flag) {
  if (!reserved_next_parser(parser)) {
    adv_parser(parser);
    push_exprs(output, produce_expr(Bool, num, flag));
  } else {
    size_t start = parser->cursor;
    until_reserved_parser(parser);
    char *str = slice_parser(parser, start);
    if (!strncmp(str, flag ? "true" : "false", 5 - flag)) {
      push_exprs(output, produce_expr(Bool, num, flag));
    } else {
      err_parser(parser, ImproperFalse + flag);
    }
    free(str);
  }
}

void character(parser_t *parser, exprs_t *output) {
  adv_parser(parser);
  if (get_parser(parser) == 'x' && isxdigit(get_next_parser(parser))) {
    adv_parser(parser);
    size_t start = parser->cursor;
    while (isxdigit(get_parser(parser))) {
      adv_parser(parser);
    }
    char *strnum = slice_parser(parser, start);
    char *end;
    size_t xnum = strtoll(strnum, &end, 16);
    free(strnum);
    if (xnum > 127) {
      push_exprs(output, produce_expr(UniChr, num, xnum));
    } else {
      push_exprs(output, produce_expr(Chr, ch, xnum));
    }
  } else if (!isalpha(get_next_parser(parser))) {
    push_exprs(output, produce_expr(Chr, ch, get_parser(parser)));
    adv_parser(parser);
  } else {
    size_t start = parser->cursor;
    until_reserved_parser(parser);
    char *name = slice_ci_parser(parser, start);
    switch (name[0]) {
    case 'a':
      if (!strncmp(name, "alarm", 5)) {
        push_exprs(output, produce_expr(Chr, ch, 7));
      } else {
        err_parser(parser, ImproperAlarmName);
      }
      break;
    case 'b':
      if (!strncmp(name, "backspace", 9)) {
        push_exprs(output, produce_expr(Chr, ch, 8));
      } else {
        err_parser(parser, ImproperBackspaceName);
      }
      break;
    case 'd':
      if (!strncmp(name, "delete", 6)) {
        push_exprs(output, produce_expr(Chr, ch, 127));
      } else {
        err_parser(parser, ImproperDeleteName);
      }
      break;
    case 'e':
      if (!strncmp(name, "escape", 6)) {
        push_exprs(output, produce_expr(Chr, ch, 27));
      } else {
        err_parser(parser, ImproperEscapeName);
      }
      break;
    case 'n':
      if (!strncmp(name, "newline", 7)) {
        push_exprs(output, produce_expr(Chr, ch, 10));
      } else if (!strncmp(name, "null", 4)) {
        push_exprs(output, produce_expr(Chr, ch, 0));
      } else {
        err_parser(parser, ImproperNewlineNullName);
      }
      break;
    case 'r':
      if (!strncmp(name, "return", 6)) {
        push_exprs(output, produce_expr(Chr, ch, 13));
      } else {
        err_parser(parser, ImproperReturnName);
      }
      break;
    case 's':
      if (!strncmp(name, "space", 5)) {
        push_exprs(output, produce_expr(Chr, ch, 32));
      } else {
        err_parser(parser, ImproperSpaceName);
      }
      break;
    case 't':
      if (!strncmp(name, "tab", 3)) {
        push_exprs(output, produce_expr(Chr, ch, 9));
      } else {
        err_parser(parser, ImproperTabName);
      }
      break;
    default:
      err_parser(parser, ImproperCharName);
      break;
    }
  }
}

void vector(parser_t *parser, exprs_t *output) {
  adv_parser(parser);
  if (get_parser(parser) != ')') {
    exprs_t *vec = create_exprs(1);
    expr_until_char_parser(parser, vec, ')', VecUnfinishedEof);
    adv_parser(parser);
    push_exprs(output, produce_expr(Vec, exprs, vec));
  } else {
    exprs_t *vec = create_exprs(0);
    adv_parser(parser);
    push_exprs(output, produce_expr(Vec, exprs, vec));
  }
}

void bool_or_special(parser_t *parser, exprs_t *output) {
  adv_parser(parser);
  switch (get_parser(parser)) {
  case 't':
    boolean(parser, output, 1);
    break;
  case 'f':
    boolean(parser, output, 0);
    break;
  case '\\':
    character(parser, output);
    break;
  case '(':
    vector(parser, output);
    break;
  default:
    err_parser(parser, ImproperSpecial);
    break;
  }
}

void number_or_symbol(parser_t *parser, exprs_t *output) {
  if (!isdigit(get_next_parser(parser))) {
    symbol(parser, output);
  } else {
    adv_parser(parser);
    number(parser, output);
  }
}

void number_or_symbol_neg(parser_t *parser, exprs_t *output) {
  if (!isdigit(get_next_parser(parser))) {
    symbol(parser, output);
  } else {
    adv_parser(parser);
    number(parser, output);
    output->arr[output->len - 1].num *= -1;
  }
}

void number(parser_t *parser, exprs_t *output) {
  size_t start = parser->cursor;
  while (isdigit(get_parser(parser))) {
    adv_parser(parser);
  }
  char *strnum = slice_parser(parser, start);
  char *end;
  ssize_t num = strtoll(strnum, &end, 10);
  free(strnum);
  push_exprs(output, produce_expr(Num, num, num));
}

void pipe_symbol(parser_t *parser, exprs_t *output) {
  adv_parser(parser);
  size_t start = parser->cursor;
  until_char_parser(parser, '|', SymbUnfinishedEof);
  char *symb = slice_parser(parser, start);
  adv_parser(parser);
  push_exprs(output, produce_expr(Symb, str, symb));
}

void symbol(parser_t *parser, exprs_t *output) {
  size_t start = parser->cursor;
  until_reserved_parser(parser);
  char *symb = slice_parser(parser, start);
  push_exprs(output, produce_expr(Symb, str, symb));
}

int expr(parser_t *parser, exprs_t *output) {
  skip_whitespace_parser(parser);
  switch (get_parser(parser)) {
  case 0:
    return 0;
  case ';':
    comment(parser);
    break;
  case '\'': {
    adv_parser(parser);
    enum quote qsaved = parser->quote;
    parser->quote = Quote;
    expr(parser, output);
    quote_parsed(parser, output);
    parser->quote = qsaved;
    break;
  }
  case '`': {
    adv_parser(parser);
    enum quote qqsaved = parser->quote;
    parser->quote = QuasiQuote;
    expr(parser, output);
    quote_parsed(parser, output);
    parser->quote = qqsaved;
    break;
  }
  case ',':
    unquote(parser, output);
    break;
  case '#':
    bool_or_special(parser, output);
    break;
  case '(':
    if (parser->quote == Normal) {
      non_empty_list(parser, output);
    } else {
      list(parser, output);
    }
    break;
  case ')':
    err_parser(parser, ListUnmatchedRight);
    break;
  case '"':
    string(parser, output);
    break;
  case '+':
    number_or_symbol(parser, output);
    break;
  case '-':
    number_or_symbol_neg(parser, output);
    break;
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
    number(parser, output);
    break;
  case '|':
    pipe_symbol(parser, output);
    break;
  default:
    symbol(parser, output);
    break;
  }
  return 1;
}

exprs_t *parse(parser_t *parser, const char *src) {
  if (parser->errs) {
    delete_errs(parser->errs);
  }
  parser->errs = create_errs(2);
  parser->src = src;
  parser->line = 0;
  parser->line_loc = 0;
  parser->cursor = 0;

  exprs_t *output = create_exprs(4);
  // expr returns 0 only when encountering 0/EOF, so this is a practical expr*
  while (expr(parser, output))
    ;
  return output;
}
