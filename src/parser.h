#ifndef PARSER_H
#define PARSER_H

#include "errs.h"

/// @file parser.h
/// @brief String to SExprs parser.
///
/// Parses SExprs to Scheme standard with few exceptions.
/// - Currently [], {} are parsed the same as (), while R7 reserves them for
/// potential future use.
/// - Certain language features like bools are unimplemented, thus not parsed.

/// @brief Reusable parser object
typedef struct parser_t {
  const char *input; ///> String that is being parsed.
  size_t loc;        ///> The location in the input.
  size_t line;       ///> Current file line.
  size_t line_loc;   ///> Current file location.
  errs_t *errs;      ///> Parser errors.
} parser_t;

/// @brief Creates a `parser` object with mostly unintialized fields.
/// @return `parser` object
parser_t *create_parser();

/// @brief Deletes any `errs` and frees the `parser`.
void delete_parser(parser_t *parser);

/// @brief Check for whether the parser produced any errors.
///
/// Should be used as any error will trigger a compiler error.
/// @return 1 if any error exists, 0 otherwise
int has_err_parser(parser_t *parser);

/// @brief Force an error with location in parser and error type.
struct expr_t err(parser_t *parser, enum err err);

// Parse a single expr in a parser.
struct expr_t expr(parser_t *parser);

/// @brief Main parse function.
/// @param parser The reusable parser.
/// @param input The string to parse.
/// @return vector of SExprs.
struct exprs_t *parse(parser_t *parser, const char *input);

#endif // PARSER_H
