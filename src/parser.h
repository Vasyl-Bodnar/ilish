#ifndef PARSER_H
#define PARSER_H

#include "errs.h"

typedef struct parser_t {
  struct buffer_t *buffer;
  size_t line;
  size_t line_loc;
  errs_t *errs;
} parser_t;

parser_t *create_parser();

/// Deletes the `errs` and frees the parser
void delete_parser(parser_t *parser);

/// Simple check for whether the parser produced any errors.
/// Should be used as there is no other proper indication of errors.
int has_err_parser(parser_t *parser);

/// Force an error based on location in parser and type enum
struct expr_t err(parser_t *parser, enum err err);

/* int eof(parser_t *parser); */

/* struct expr_t ch(parser_t *parser); */

/* struct expr_t str(parser_t *parser); */

/* struct expr_t list(parser_t *parser); */

/* struct expr_t num(parser_t *parser); */

/// Parse a single expr in a parser
struct expr_t expr(parser_t *parser);

/// `errs` from the previous call are freed.
/// `input` is not modified by parser.
/// Returned list must be deleted.
struct exprs_t *parse(parser_t *parser, struct buffer_t *buffer);

#endif // PARSER_H
