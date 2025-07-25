#include <assert.h>
#include <ctype.h>

#include "block.h"
#include "error.h"
#include "event.h"
#include "parse.h"
#include "parse_util.h"
#include "stream.h"

const char *asdf_standard_comment = "#ASDF_STANDARD ";
const char *asdf_version_comment = "#ASDF ";


#define ASDF_YAML_DIRECTIVE_PREFIX "%YAML "
#define ASDF_YAML_DOCUMENT_END_MARKER "\n..."


const char *asdf_yaml_directive_prefix = ASDF_YAML_DIRECTIVE_PREFIX;
const char *asdf_yaml_directive = "%YAML 1.1";
const char *asdf_yaml_document_end_marker = ASDF_YAML_DOCUMENT_END_MARKER;


#define TOKEN(t) ((const uint8_t *)(t))


const asdf_parse_token_t asdf_parse_tokens[] = {
    [ASDF_YAML_DIRECTIVE_TOK] =
        {TOKEN(ASDF_YAML_DIRECTIVE_PREFIX), ASDF_YAML_DIRECTIVE_PREFIX_SIZE},
    [ASDF_YAML_DOCUMENT_END_TOK] =
        {TOKEN(ASDF_YAML_DOCUMENT_END_MARKER), ASDF_YAML_DOCUMENT_END_MARKER_SIZE},
    [ASDF_BLOCK_MAGIC_TOK] = {asdf_block_magic, ASDF_BLOCK_MAGIC_SIZE},
    [ASDF_BLOCK_INDEX_HEADER_TOK] = {
        (const uint8_t *)asdf_block_index_header, ASDF_BLOCK_INDEX_HEADER_SIZE}};


/* Parsing helpers */

/**
 * Wrapper around asdf_stream_scan with an interface that returns a matched token as
 * a value of asdf_parse_token_id_t
 */
int asdf_parser_scan_tokens(
    asdf_parser_t *parser,
    const asdf_parse_token_id_t *token_ids,
    size_t *match_offset,
    asdf_parse_token_id_t *match_token) {
    assert(token_ids);
    asdf_parse_token_id_t used_token_ids[ASDF_LAST_TOK];
    asdf_parse_token_t tokens[ASDF_LAST_TOK] = {0};
    size_t n_tokens = 0;

    while (token_ids[n_tokens] < ASDF_LAST_TOK) {
        used_token_ids[n_tokens] = token_ids[n_tokens];
        tokens[n_tokens] = asdf_parse_tokens[token_ids[n_tokens]];
        n_tokens++;
    }

    if (n_tokens == 0)
        return 1;

    const uint8_t *token_vals[n_tokens];
    size_t token_lens[n_tokens];
    size_t matched_idx = 0;

    for (size_t idx = 0; idx < n_tokens; idx++) {
        token_vals[idx] = tokens[idx].tok;
        token_lens[idx] = tokens[idx].tok_len;
    }

    int ret = asdf_stream_scan(
        parser->stream, token_vals, token_lens, n_tokens, match_offset, &matched_idx);
    if (0 == ret) {
        *match_token = used_token_ids[matched_idx];
    }

    return ret;
}


bool is_generic_yaml_directive(const char *buf, size_t len) {
    if (len < ASDF_YAML_DIRECTIVE_SIZE + 1)
        return false;

    if (strncmp(buf, ASDF_YAML_DIRECTIVE_PREFIX, ASDF_YAML_DIRECTIVE_PREFIX_SIZE) != 0)
        return false;

    const char *version = buf + ASDF_YAML_DIRECTIVE_PREFIX_SIZE;
    const char *end = buf + len;

    // Parse X.Y where X and Y are one or more ASCII digits
    const char *p = version;
    if (!isdigit((unsigned char)*p))
        return false;

    while (isdigit((unsigned char)*p) && p < end)
        ++p;

    if (p < end && *p++ != '.')
        return false;

    if (!isdigit((unsigned char)*p))
        return false;

    while (isdigit((unsigned char)*p) && p < end)
        ++p;

    // Accept optional \r before \n
    if (*p == '\r' && p < end)
        ++p;

    return *p == '\n';
}


/**
 * asdf_event_t allocation helpers
 */
asdf_event_t *asdf_parse_event_alloc(asdf_parser_t *parser) {
    assert(parser);
    struct asdf_event_p *event_p = NULL;

    if (parser->event_freelist) {
        event_p = parser->event_freelist;
        parser->event_freelist = event_p->next;
    } else {
        event_p = malloc(sizeof(*event_p));
        if (!event_p)
            return NULL;
    }

    ZERO_MEMORY(&event_p->event, sizeof(event_p->event));
    parser->current_event_p = event_p;
    return &event_p->event;
}


void asdf_parse_event_recycle(asdf_parser_t *parser, asdf_event_t *event) {
    if (!parser || !event)
        return;

    struct asdf_event_p *event_p =
        (struct asdf_event_p *)((char *)event - offsetof(struct asdf_event_p, event));
    event_p->next = parser->event_freelist;
    parser->event_freelist = event_p;

    if (parser->current_event_p == event_p)
        parser->current_event_p = NULL;
}


void asdf_parse_event_freelist_free(asdf_parser_t *parser) {
    struct asdf_event_p *current = parser->current_event_p;
    free(current);

    struct asdf_event_p *freelist = parser->event_freelist;

    while (freelist) {
        struct asdf_event_p *next = freelist->next;
        // Avoid double-free in case the current head is also on the freelist
        if (freelist != current)
            free(freelist);

        freelist = next;
    }
}
