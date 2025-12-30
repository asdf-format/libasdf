/**
 * Miscellaneous utility constants and subroutines for parse
 *
 * Split into a separate source module for decluttering and ease of testing
 */

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "parser.h"
#include "util.h"
#include "yaml.h"


extern ASDF_LOCAL const char *asdf_standard_comment;
extern ASDF_LOCAL const char *asdf_version_comment;

/** The default ASDF format version to use when writing new files */
extern ASDF_LOCAL const char *asdf_version_default;

/** The default ASDF standard version to use when writing new files */
extern ASDF_LOCAL const char *asdf_standard_default;


/**
 * Enum type defining specific tokens that the parser scans for
 */
typedef enum {
    ASDF_YAML_DIRECTIVE_TOK = 0,
    ASDF_YAML_DOCUMENT_END_TOK,
    ASDF_BLOCK_MAGIC_TOK,
    ASDF_BLOCK_INDEX_HEADER_TOK,
    ASDF_LAST_TOK
} asdf_parse_token_id_t;


typedef struct {
    const uint8_t *tok;
    size_t tok_len;
} asdf_parse_token_t;


extern ASDF_LOCAL const asdf_parse_token_t asdf_parse_tokens[];


/* Internal parse helper functions */
ASDF_LOCAL int asdf_parser_scan_tokens(
    asdf_parser_t *parser,
    const asdf_parse_token_id_t *token_ids,
    size_t *match_offset,
    asdf_parse_token_id_t *match_token);


/* Additional helper functions */
ASDF_LOCAL bool is_generic_yaml_directive(const char *buf, size_t len);


/* Inline helper functions */
static inline bool ends_with_newline(const char *buf, size_t buf_size, size_t len) {
    if (UNLIKELY(len == 0 || buf_size == 0 || buf_size < len + 1))
        return false;

    return buf[len] == '\n' || (buf[len] == '\r' && buf_size >= len + 2 && buf[len + 1] == '\n');
}


static inline bool is_string_with_newline(
    const char *buf, size_t buf_size, const char *str, size_t len) {
    if (buf_size < len + 1)
        return false;

    if (buf == NULL)
        return false;

    if (0 != strncmp(buf, str, len))
        return false;

    return ends_with_newline(buf, buf_size, len);
}


static inline bool is_yaml_1_1_directive(const char *buf, size_t len) {
    return is_string_with_newline(buf, len, asdf_yaml_directive, ASDF_YAML_DIRECTIVE_SIZE);
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
    return is_string_with_newline(
        buf, len, asdf_yaml_document_end_marker, ASDF_YAML_DOCUMENT_END_MARKER_SIZE);
}

asdf_event_t *asdf_parse_event_alloc(asdf_parser_t *parser);
void asdf_parse_event_recycle(asdf_parser_t *parser, asdf_event_t *event);
void asdf_parse_event_freelist_free(asdf_parser_t *parser);
