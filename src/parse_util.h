/**
 * Miscellaneous utility constants and subroutines for parse
 *
 * Split into a separate source module for decluttering and ease of testing
 */

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "block.h"
#include "parse.h"
#include "util.h"


extern ASDF_LOCAL const char *asdf_standard_comment;
extern ASDF_LOCAL const char *asdf_version_comment;

/**
 * The "%YAML 1.1\n" directive specifically expected for valid ASDF
 */
extern ASDF_LOCAL const char *ASDF_YAML_DIRECTIVE;
#define ASDF_YAML_DIRECTIVE_SIZE 9


/**
 * The string "%YAML " indicating start of an arbitrary YAML directive
 */
extern ASDF_LOCAL const char *ASDF_YAML_DIRECTIVE_PREFIX;
#define ASDF_YAML_DIRECTIVE_PREFIX_SIZE 6


/**
 * Token for a valid YAML document end marker, i.e. '\n...'
 * It should also end with a newline but that's excluded from this constant
 * since it could be either \r\n or just \n so we check for that explicitly once
 * this token is found
 */
extern ASDF_LOCAL const char *ASDF_YAML_DOCUMENT_END_MARKER;
#define ASDF_YAML_DOCUMENT_END_MARKER_SIZE 4


/**
 * Enum type defining specific tokens that the parser scans for
 */
typedef enum {
    ASDF_YAML_DIRECTIVE_TOK = 0,
    ASDF_YAML_DOCUMENT_END_TOK,
    ASDF_BLOCK_MAGIC_TOK,
    ASDF_LAST_TOK
} asdf_parse_token_id_t;


typedef struct {
    const uint8_t *tok;
    size_t tok_len;
} asdf_parse_token_t;


extern ASDF_LOCAL const asdf_parse_token_t asdf_parse_tokens[];


/* Internal parse helper functions */
ASDF_LOCAL int asdf_parser_scan_tokens(
    asdf_parser_t *parser, const asdf_parse_token_id_t *token_ids, size_t *match_offset,
    asdf_parse_token_id_t *match_token);


/* Internal error helper functions */
ASDF_LOCAL void asdf_parser_set_oom_error(asdf_parser_t *parser);
ASDF_LOCAL void asdf_parser_set_error(asdf_parser_t *parser, const char *fmt, ...);
ASDF_LOCAL void asdf_parser_set_static_error(asdf_parser_t *parser, const char *error);
ASDF_LOCAL void asdf_parser_set_common_error(asdf_parser_t *parser, asdf_parser_error_code_t code);


/* Additional helper functions */
ASDF_LOCAL bool is_generic_yaml_directive(const char *buf, size_t len);


/* Inline helper functions */
static inline bool is_yaml_1_1_directive(const char *buf, size_t len) {
    if (len < ASDF_YAML_DIRECTIVE_SIZE + 1)
        return false;

    if (strncmp(buf, ASDF_YAML_DIRECTIVE, ASDF_YAML_DIRECTIVE_SIZE) != 0)
        return false;

    if (buf[ASDF_YAML_DIRECTIVE_SIZE] == '\n'
        || (buf[ASDF_YAML_DIRECTIVE_SIZE] == '\r'
        && len >= ASDF_YAML_DIRECTIVE_SIZE + 2 && buf[ASDF_YAML_DIRECTIVE_SIZE + 1] == '\n'))
        return true;

    return false;
}


/**
 * Returns `true` if the given buffer contains a valid %YAML directive line.
 *
 * First checks the happy path for containing exactly ``%YAML 1.1\r?\n`` as required
 * by the ASDF 1.6.0 standard.  Then fall back on accepting any non-standard (but still
 * syntactically valid) ``%YAML`` directive.
 */
static inline bool is_yaml_directive(const char *buf, size_t len) {
    if (is_yaml_1_1_directive(buf, len))
        return true;

    return is_generic_yaml_directive(buf, len);
}


/**
 * Is the given buffer pointing to a line beginning with "\n...\r?\n" (including the preceding
 * newline)
 */
static inline bool is_yaml_document_end_marker(const char *buf, size_t len) {
    return (
        len >= 5
        && strncmp(buf, ASDF_YAML_DOCUMENT_END_MARKER, ASDF_YAML_DOCUMENT_END_MARKER_SIZE) == 0
        && (buf[4] == '\n' || (len >= 6 && buf[4] == '\r' && buf[5] == '\n'))
    );
}
