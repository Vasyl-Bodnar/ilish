#ifndef PARSER_H
#define PARSER_H

#include "errs.h"
#include "exprs.h"

/// @file parser.h
/// @brief String to SExprs parser.
///
/// A capable recursive descent parser with error handling and covering a good
/// amount of important parts of Scheme R7RS.
///
/// This is not a complete version with a few less important features
/// not being implemented like block comments, string escapes, and
/// full reserve list.

/// @brief Quote State
enum quote {
  Normal,
  Quote,
  QuasiQuote,
};

/// @brief Reusable Parser
typedef struct parser_t {
  const char *src;
  size_t line;
  size_t line_loc;
  size_t old_line_loc;
  size_t cursor;
  enum quote quote;
  errs_t *errs;
} parser_t;

/// Create parser with everything set to 0
parser_t *create_parser();
/// Delete parser
void delete_parser(parser_t *parser);
/// Produce an error in the parser
void err_parser(parser_t *parser, enum err err);
/// Did parser produce errors, and how many
int has_err_parser(parser_t *parser);
/// Simple ; comment
void comment(parser_t *parser);
/// Apply quotes, quasiquotes, unquotes, and splices to expressions.
void keyword_parsed(parser_t *parser, exprs_t *output, const char *keyword);
/// Call keyword_parsed accordingly to parser's quote mode
void quote_parsed(parser_t *parser, exprs_t *output);
/// Takes care of #t, #true, #f, #false
void boolean(parser_t *parser, exprs_t *output, int flag);
/// Robust char handler, implemenenting chars, hex, and special names
void character(parser_t *parser, exprs_t *output);
/// Standard vector, essentially identical to list
void vector(parser_t *parser, exprs_t *output);
/// Matches bool, character, or vectors
void bool_or_special(parser_t *parser, exprs_t *output);
/// Handles +1 as number or +x as symbol
void number_or_symbol(parser_t *parser, exprs_t *output);
/// Handles -1 as number or -x as symbol
void number_or_symbol_neg(parser_t *parser, exprs_t *output);
/// Pure whole number
void number(parser_t *parser, exprs_t *output);
/// Produces nil on empty list
void list(parser_t *parser, exprs_t *output);
/// Errs on empty list
void non_empty_list(parser_t *parser, exprs_t *output);
/// Strings in ""
/// NOTE: This is a basic version without \n, \t and the like
void string(parser_t *parser, exprs_t *output);
/// Piped symbol |x| for more special symbol possibilities
/// NOTE: This is a basic version without \n, \t, etc., and hex unicode
void pipe_symbol(parser_t *parser, exprs_t *output);
/// More restricted than piped, good enough for nearly all real cases
void symbol(parser_t *parser, exprs_t *output);
/// Parses expression, return 0/False if encounters 0/EOF, otherwise 1
int expr(parser_t *parser, exprs_t *output);
/// Parse a string using the parser and src
exprs_t *parse(parser_t *parser, const char *src);

#endif // PARSER_H
