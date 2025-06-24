#include <assert.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "block.h"
#include "compat/endian.h"
#include "event.h"
#include "parse.h"
#include "parse_util.h"
#include "util.h"
#include "yaml.h"


typedef enum {
    ASDF_PARSE_ERROR = -1,
    ASDF_PARSE_CONTINUE,
    ASDF_PARSE_EVENT,
} parse_result_t;


/**
 * Helper to consume bytes from the stream and immediately check the stream status
 * for any errors.
 */
#define CONSUME_AND_CHECK(parser, count) \
    do { \
        asdf_stream_consume((parser)->stream, count); \
        if (UNLIKELY(asdf_parser_check_stream(parser) != 0)) { \
            return ASDF_PARSE_ERROR; \
        } \
    } while (0);


/**
 * Like CONSUME_AND_CHECK but returns 1 in case of error instead of ASDF_PARSER_ERROR;
 */
#define CONSUME_AND_CHECK_INT(parser, count) \
    do { \
        asdf_stream_consume((parser)->stream, count); \
        if (UNLIKELY(asdf_parser_check_stream(parser) != 0)) { \
            return 1; \
        } \
    } while (0);


static int parse_version_comment(
    asdf_parser_t *parser, const char *expected, char *out_buf, size_t out_size) {
    size_t expected_len;
    size_t line_len = 0;
    const uint8_t *r = asdf_stream_readline(parser->stream, &line_len);

    if (!r) {
        asdf_parser_set_common_error(parser, ASDF_ERR_UNEXPECTED_EOF);
        return ASDF_PARSE_ERROR;
    }

    expected_len = strlen(expected);

    if (line_len < expected_len || strncmp((const char *)r, expected, expected_len) != 0) {
        asdf_parser_set_common_error(parser, ASDF_ERR_INVALID_ASDF_HEADER);
        return 1;
    }

    size_t to_copy = line_len - expected_len - 1;

    if (to_copy >= out_size)
        to_copy = out_size - 1;

    memcpy(out_buf, r + expected_len, to_copy);
    out_buf[to_copy] = '\0';
    return 0;
}


// TODO: These need to be more flexible for acccepting different ASDF versions
// TODO: Depending on strictness level we can resume even if this fails
static parse_result_t parse_asdf_version(asdf_parser_t *parser, asdf_event_t *event) {
    if (parse_version_comment(
            parser, asdf_version_comment, parser->asdf_version, ASDF_ASDF_VERSION_BUFFER_SIZE) != 0)
        return ASDF_PARSE_ERROR;

    event->type = ASDF_ASDF_VERSION_EVENT;
    event->payload.version = malloc(sizeof(asdf_version_t));
    event->payload.version->version = strdup(parser->asdf_version);
    parser->state = ASDF_PARSER_STATE_STANDARD_VERSION;
    return ASDF_PARSE_EVENT;
}


static parse_result_t parse_standard_version(asdf_parser_t *parser, asdf_event_t *event) {
    if (parse_version_comment(
            parser,
            asdf_standard_comment,
            parser->standard_version,
            ASDF_STANDARD_VERSION_BUFFER_SIZE) != 0)
        return ASDF_PARSE_ERROR;

    event->type = ASDF_STANDARD_VERSION_EVENT;
    event->payload.version = malloc(sizeof(asdf_version_t));
    event->payload.version->version = strdup(parser->standard_version);
    parser->state = ASDF_PARSER_STATE_COMMENT;
    return ASDF_PARSE_EVENT;
}


static parse_result_t parse_comment(asdf_parser_t *parser, asdf_event_t *event) {
    const uint8_t *r = NULL;
    size_t len = 0;

    // Peek the stream to see if we have a comment or blank line
    while ((r = asdf_stream_next(parser->stream, 1, &len))) {
        if (len == 0) {
            // EOF
            parser->state = ASDF_PARSER_STATE_END;
            return ASDF_PARSE_CONTINUE;
        }

        if (r[0] == '\r' || r[0] == '\n') {
            // Blank line, just continue
            CONSUME_AND_CHECK(parser, 1);
            continue;
        }

        if (r[0] == ASDF_COMMENT_CHAR) {
            // Found comment, attempt to read and consume the full line
            r = asdf_stream_readline(parser->stream, &len);

            if (!r) {
                asdf_parser_set_common_error(parser, ASDF_ERR_UNEXPECTED_EOF);
                return 1;
            }

            char *comment = strndup((const char *)r + 1, len - 1);
            comment[strcspn(comment, "\r\n")] = '\0';
            event->type = ASDF_COMMENT_EVENT;
            event->payload.comment = comment;
            // State remains searching for comment events
            return ASDF_PARSE_EVENT;
        }

        break;
    }

    parser->state = ASDF_PARSER_STATE_BLOCK_INDEX;
    return ASDF_PARSE_CONTINUE;
}


static parse_result_t emit_tree_start_event(asdf_parser_t *parser, asdf_event_t *event) {
    off_t offset = asdf_stream_tell(parser->stream);
    parser->tree.start = offset;
    parser->state = ASDF_PARSER_STATE_TREE;
    event->type = ASDF_TREE_START_EVENT;
    event->payload.tree = calloc(1, sizeof(asdf_tree_info_t));
    event->payload.tree->start = offset;
    return ASDF_PARSE_EVENT;
}


/**
 * This phase of parsing expects *either* an explicit start to the YAML tree indicated
 * by the "%YAML <yaml-version>" directive or the start of a block indicated by the block
 * magic token.
 *
 * This is the the happy case for a well-formed ASDF document.
 *
 * If the happy case fails we then switch to a slower parse mode
 */
static parse_result_t parse_tree_or_block(asdf_parser_t *parser, asdf_event_t *event) {
    size_t len = 0;

    const uint8_t *r = asdf_stream_next(parser->stream, 0, &len);

    if (!r || !len) {
        parser->state = ASDF_PARSER_STATE_END;
        // Should immediately emit the end event, not a block event
        return ASDF_PARSE_CONTINUE;
    }

    // Likeliest case--we encounter a %YAML directive indicating beginning of the YAML tree
    if (LIKELY(is_yaml_directive((const char *)r, len))) {
        return emit_tree_start_event(parser, event);
    }

    // Or may encounter an ASDF block magic esp. in case of an exploded ASDF file
    if (is_block_magic((const char *)r, len)) {
        parser->state = ASDF_PARSER_STATE_BLOCK;
        return ASDF_PARSE_CONTINUE;
    }

    // Not as happy case: might be some padding or other garbage.  Here we might want
    // to issue some consistency warnings, depending, though technically there can be padding.
    // Scan for the beginning of the YAML tree or a block.
    const asdf_parse_token_id_t tokens[] = {
        ASDF_YAML_DIRECTIVE_TOK, ASDF_BLOCK_MAGIC_TOK, ASDF_LAST_TOK};
    asdf_parse_token_id_t match_token = ASDF_LAST_TOK;
    size_t match_offset = 0;

    while (0 == asdf_parser_scan_tokens(parser, tokens, &match_offset, &match_token)) {
        // No expected token found; may be the end of the file or just unparseable garbage
        // TODO: May also encounter a block index, though if we didn't find a valid block
        // up to this point it is either empty or invalid
        switch (match_token) {
        case ASDF_YAML_DIRECTIVE_TOK:
            // Make sure it is actually a valid YAML directive
            // TODO: Technically this should be either at the start of the file or begin
            // with a newline; should be more careful about that.
            if (LIKELY(is_yaml_directive((const char *)r, len))) {
                return emit_tree_start_event(parser, event);
            }
            break;
        case ASDF_BLOCK_MAGIC_TOK:
            parser->state = ASDF_PARSER_STATE_BLOCK;
            return ASDF_PARSE_CONTINUE;
        default:
            /* Shouldn't happen */
            asdf_parser_set_common_error(parser, ASDF_ERR_UNKNOWN_STATE);
            return 1;
        }
    }

    parser->state = ASDF_PARSER_STATE_END;
    return ASDF_PARSE_CONTINUE;
}


/**
 * Helper to clean up and set the ASDF_TREE_END_EVENT in case of finding the end of the tree
 * (or the file)
 */
parse_result_t emit_tree_end_event(asdf_parser_t *parser, asdf_event_t *event) {
    event->type = ASDF_TREE_END_EVENT;
    event->payload.tree = calloc(1, sizeof(asdf_tree_info_t));

    if (!event->payload.tree) {
        asdf_parser_set_oom_error(parser);
        return ASDF_PARSE_ERROR;
    }

    event->payload.tree->start = parser->tree.start;
    event->payload.tree->end = parser->tree.end;
    event->payload.tree->buf = (const char *)parser->tree.buf;

    return ASDF_PARSE_EVENT;
}


/**
 * "Fast" tree parsing
 *
 * Scans for either a YAML document end or an ASDF block magic and sets the offset
 * of the end of the tree (or EOF)
 */
static int parse_tree_fast(asdf_parser_t *parser) {
    const uint8_t *r = NULL;
    bool tree_end_found = false;
    off_t tree_end = 0;
    size_t len = 0;

    const asdf_parse_token_id_t tokens[] = {
        ASDF_YAML_DOCUMENT_END_TOK, ASDF_BLOCK_MAGIC_TOK, ASDF_LAST_TOK};
    asdf_parse_token_id_t match_token = ASDF_LAST_TOK;
    size_t match_offset = 0;

    while (0 == asdf_parser_scan_tokens(parser, tokens, &match_offset, &match_token)) {
        // We got a match for either a document end marker or a block magic
        switch (match_token) {
        case ASDF_YAML_DOCUMENT_END_TOK: {
            r = asdf_stream_next(parser->stream, 0, &len);

            if (LIKELY(is_yaml_document_end_marker((const char *)r, len))) {
                // Read and consume the full line then set the tree end
                // First chomp the leading newline of the document end marker, then consume
                // the line of the marker itself.
                CONSUME_AND_CHECK_INT(parser, 1);
                asdf_stream_readline(parser->stream, &len);
                tree_end = asdf_stream_tell(parser->stream);
                tree_end_found = true;
                break;
            }

            // Looked like a document end marker but unexpected trailing bytes
            // instead of a newline; keep parsing
            continue;
        }
        case ASDF_BLOCK_MAGIC_TOK:
            // Unexpectedly found a block magic before the document end marker
            tree_end = asdf_stream_tell(parser->stream);
            tree_end_found = true;
            break;
        default:
            /* should happen */
            asdf_parser_set_common_error(parser, ASDF_ERR_UNKNOWN_STATE);
            return 1;
        }

        if (tree_end_found)
            break;
    }

    parser->tree.end = tree_end;
    parser->tree.found = true;
    return 0;
}


/**
 * Default libfyaml parser configuration
 *
 * Later we will likely want some ASDF parser configuration, and this could include
 * the ability to pass through low-level YAML parser flags as well.  Could be useful.
 */
static const struct fy_parse_cfg default_fy_parse_cfg = {.flags = FYPCF_QUIET | FYPCF_COLLECT_DIAG};


/**
 * Before transitioning to generating YAML events we need to set up libfyaml
 *
 * There are two possible cases:
 *
 * * We have already buffered the entire YAML tree in memory, so set up libfyaml
 *   to use that memory buffer.
 * * Otherwise set up libfyaml to use the stream.  This is only valid if the stream
 *   is seekable, logic which is enforced elsewhere.
 */
static int initialize_yaml_parser(asdf_parser_t *parser) {
    assert(parser->stream->is_seekable || (parser->tree.found && parser->tree.buf));
    bool buffer_tree = asdf_parser_has_opt(parser, ASDF_PARSER_OPT_BUFFER_TREE);
    int ret = 0;

    if (!(parser->yaml_parser = fy_parser_create(&default_fy_parse_cfg))) {
        asdf_parser_set_common_error(parser, ASDF_ERR_YAML_PARSER_INIT_FAILED);
    }

    if (buffer_tree) {
        ret = fy_parser_set_string(
            parser->yaml_parser, (const char *)parser->tree.buf, parser->tree.size);
    } else {
        ret = parser->stream->fy_parser_set_input(parser->stream, parser->yaml_parser);
    }

    if (0 != ret)
        asdf_parser_set_common_error(parser, ASDF_ERR_YAML_PARSER_INIT_FAILED);

    return ret;
}


static parse_result_t parse_tree(asdf_parser_t *parser, asdf_event_t *event) {
    bool buffer_tree = asdf_parser_has_opt(parser, ASDF_PARSER_OPT_BUFFER_TREE);
    bool emit_yaml_events = asdf_parser_has_opt(parser, ASDF_PARSER_OPT_EMIT_YAML_EVENTS);

    // There is an inherent limitation that if we want to generate YAML events during
    // ASDF parsing, but the input stream is not seekable, we should buffer the entire
    // YAML tree in memory.
    // This is because if we turn parsing over to libfyaml it read the stream past
    // the end of the YAML portion and make it difficult to recover previous portions
    // of the file.
    // There are in principle other more complicated ways to work around it, such as
    // adding a method to push data back onto the buffered stream.  But the
    // ASDF_PARSER_OPT_EMIT_YAML_EVENTS option is primarily just for debugging purposes
    // so for now we don't do anything too crazy.
    buffer_tree |= (emit_yaml_events && !parser->stream->is_seekable);

    if (buffer_tree) {
        // Initialize the tree buffer and set up the stream to capture to it
        parser->tree.buf = malloc(ASDF_PARSER_READ_BUFFER_INIT_SIZE);
        if (!parser->tree.buf) {
            asdf_parser_set_oom_error(parser);
            return ASDF_PARSE_ERROR;
        }
        parser->tree.size = 0;
        asdf_stream_set_capture(
            parser->stream,
            &parser->tree.buf,
            &parser->tree.size,
            ASDF_PARSER_READ_BUFFER_INIT_SIZE);
    }

    if (buffer_tree || !emit_yaml_events) {
        // Go ahead and do "fast" parsing of the tree since we want to buffer it anyways.
        int res = parse_tree_fast(parser);
        // Halt stream capture if it was running
        asdf_stream_set_capture(parser->stream, NULL, NULL, 0);

        if (0 != res)
            return ASDF_PARSE_ERROR;
    }

    // Continue to generating YAML events
    if (emit_yaml_events) {
        if (0 != initialize_yaml_parser(parser))
            return ASDF_PARSE_ERROR;

        parser->state = ASDF_PARSER_STATE_YAML;
        // Even if we have buffered the full tree mark it as not found until YAML parsing completes
        parser->tree.found = false;
        return ASDF_PARSE_CONTINUE;
    }

    // Otherwise emit tree end event and move to padding/block
    parser->state = ASDF_PARSER_STATE_PADDING;
    return emit_tree_end_event(parser, event);
}


static parse_result_t parse_yaml(asdf_parser_t *parser, asdf_event_t *event) {
    bool emit_yaml_events = asdf_parser_has_opt(parser, ASDF_PARSER_OPT_EMIT_YAML_EVENTS);
    bool buffer_tree = asdf_parser_has_opt(parser, ASDF_PARSER_OPT_BUFFER_TREE);

    // Only allowed if the stream is seekable or the YAML is buffered.  Other logic
    // elsewhere should enforce this.
    assert(emit_yaml_events && (buffer_tree || parser->stream->is_seekable));

    if (parser->tree.found) {
        // Already found the end of the YAML tree; just emit the TREE_END event
        parser->state = ASDF_PARSER_STATE_PADDING;
        return emit_tree_end_event(parser, event);
    }

    struct fy_event *yaml = fy_parser_parse(parser->yaml_parser);

    if (fy_parser_get_stream_error(parser->yaml_parser)) {
        asdf_parser_set_common_error(parser, ASDF_ERR_YAML_PARSE_FAILED);
        return ASDF_PARSE_ERROR;
    }

    event->type = ASDF_YAML_EVENT;
    event->payload.yaml = yaml;

    if (!yaml || yaml->type == FYET_STREAM_END) {
        // We have reached the end of the YAML stream, maybe with an error
        // but per TODO above no error handling yet so just move to the next
        // state
        // TODO: What if there are more documents?  Currently not allowed by ASDF
        // but is being discussed for ASDF 2.0.0
        const struct fy_mark *mark = fy_event_end_mark(yaml);
        if (mark) {
            // libfyaml doesn't care about FILE* and considers the the input it's handed
            // to be at position 0 when it starts parsing, so to get the actual tree end
            // relative to the start of the file we add it to the tree start position
            parser->tree.end = parser->tree.start + mark->input_pos;
        } else {
            parser->tree.end = parser->tree.start;
        }

        parser->tree.found = true;
        // Rewind the stream back to the end of the tree, as libfyaml will likely have read
        // past it for its own internal buffering.
        asdf_stream_seek(parser->stream, parser->tree.end, SEEK_SET);
    }

    return ASDF_PARSE_EVENT;
}


static parse_result_t parse_block(asdf_parser_t *parser, asdf_event_t *event) {
    size_t len = 0;
    const uint8_t *buf = asdf_stream_next(parser->stream, ASDF_BLOCK_MAGIC_SIZE, &len);

    // If is_block_magic we are on the happy path, we are already pointing to the start of a block
    // Otherwise scan for the first block magic we find, if any
    if (!is_block_magic((const char *)buf, len)) {
        const asdf_parse_token_id_t tokens[] = {ASDF_BLOCK_MAGIC_TOK, ASDF_LAST_TOK};
        asdf_parse_token_id_t match_token = ASDF_LAST_TOK;
        size_t match_offset = 0;

        if (0 != asdf_parser_scan_tokens(parser, tokens, &match_offset, &match_token)) {
            // No expected token found; may be the end of the file or just unparseable garbage
            // TODO: May also encounter a block index, though if we didn't find a valid block
            // up to this point it is either empty or invalid
            parser->state = ASDF_PARSER_STATE_END;
            // Should immediately emit the end event, not a block event
            return ASDF_PARSE_CONTINUE;
        }
    }

    asdf_block_info_t *block_info = asdf_block_read_info(parser);

    if (!block_info)
        return ASDF_PARSE_ERROR;

    // Seek to end of the block (hopefully?)
    if (asdf_stream_seek(parser->stream, block_info->header.allocated_size, SEEK_CUR)) {
        // TODO: When reading the file from a stream we will want the option to return a pointer
        // to the start of the block data, possibly with the option to copy it to a buffer.
        // For now it will suffice to skip past it.
        free(block_info);
        asdf_parser_set_static_error(parser, "Failed to seek past block data");
        return ASDF_PARSE_ERROR;
    }
    parser->found_blocks += 1;
    event->type = ASDF_BLOCK_EVENT;
    event->payload.block = block_info;
    return ASDF_PARSE_EVENT;
}


static parse_result_t parse_padding(asdf_parser_t *parser, UNUSED(asdf_event_t *event)) {
    // TODO: For now, we skip padding and transition directly to block parsing
    parser->state = ASDF_PARSER_STATE_BLOCK;
    return ASDF_PARSE_CONTINUE;
}


/* Helper for parse_block_index: Try seeking and if not return an EOF error condition */
#define TRY_SEEK(parser, offset, whence) \
    do { \
        if (UNLIKELY(0 != asdf_stream_seek((parser)->stream, (offset), (whence)))) { \
            asdf_parser_set_common_error((parser), ASDF_ERR_UNEXPECTED_EOF); \
            return ASDF_PARSE_ERROR; \
        } \
    } while (0);


// TODO: Per @braingram they have already encountered some ASDF files in the wild (catalogs?)
// that have as many as 10000 blocks, so this might not even search backwards far enough.
// Have to take a look at how other libraries are handling this.
// TODO: Split this up a bit once it's working
static parse_result_t parse_block_index(asdf_parser_t *parser, asdf_event_t *event) {
    parse_result_t res = ASDF_PARSE_CONTINUE;

    if (UNLIKELY(!parser->stream->is_seekable)) {
        // Stream is not seekable so we can't rely on the block index
        goto next_state;
    }

    asdf_stream_t *stream = parser->stream;
    off_t cur_offset = asdf_stream_tell(stream);

    // Basically seek to the end of the file on page boundardies, then seek for the block index
    // header
    long page_size = sysconf(_SC_PAGESIZE);

    if (UNLIKELY(page_size <= 0)) {
        // Something bizarre happened here, skip block index parsing
        goto next_state;
    }

    TRY_SEEK(parser, 0, SEEK_END);

    off_t file_size = asdf_stream_tell(stream);

    if (UNLIKELY(file_size < 0)) {
        asdf_parser_set_common_error(parser, ASDF_ERR_UNEXPECTED_EOF);
        return ASDF_PARSE_ERROR;
    }

    off_t aligned_offset =
        (file_size > page_size) ? (file_size - page_size - (file_size % page_size)) : 0;
    struct fy_document *doc = NULL;


    TRY_SEEK(parser, aligned_offset, SEEK_SET);

    const asdf_parse_token_id_t tokens[] = {ASDF_BLOCK_INDEX_HEADER_TOK, ASDF_LAST_TOK};
    asdf_parse_token_id_t match_token = ASDF_LAST_TOK;
    size_t match_offset = 0;

    while (0 == asdf_parser_scan_tokens(parser, tokens, &match_offset, &match_token)) {
        size_t avail = 0;
        const uint8_t *buf = asdf_stream_next(stream, ASDF_BLOCK_INDEX_HEADER_SIZE + 1, &avail);

        if (buf == NULL || avail < ASDF_BLOCK_INDEX_HEADER_SIZE + 1) {
            goto cleanup;
        }

        if (ends_with_newline((const char *)buf, avail, ASDF_BLOCK_INDEX_HEADER_SIZE))
            break;
    }

    if (match_token != ASDF_BLOCK_INDEX_HEADER_TOK)
        goto cleanup;

    // Block index found (assuming it's valid YAML)
    off_t block_index_offset = asdf_stream_tell(stream);
    off_t block_index_len = file_size - block_index_offset;

    // Ensure the full block index is available to the stream
    size_t avail = 0;
    const uint8_t *buf = asdf_stream_next(stream, block_index_len, &avail);

    if (UNLIKELY(!buf)) {
        // TODO: (#5) Not necessarily an unrecoverable error but should produce a log message
        asdf_parser_set_common_error(parser, ASDF_ERR_UNEXPECTED_EOF);
        return ASDF_PARSE_ERROR;
    }

    off_t *offsets = NULL;
    asdf_block_index_t *block_index = NULL;

    // Try to read the block index document
    doc = fy_document_build_from_string(NULL, (const char *)buf, block_index_len);
    struct fy_node *root = fy_document_root(doc);

    if (!doc || !root || !fy_node_is_sequence(root)) {
        // Invalid / corrupt block index
        goto cleanup_on_error;
    }

    size_t count = fy_node_sequence_item_count(root);
    offsets = calloc(count, sizeof(off_t));
    block_index = malloc(sizeof(asdf_block_index_t));

    if (UNLIKELY(!offsets || !block_index)) {
        asdf_parser_set_oom_error(parser);
        return ASDF_PARSE_ERROR;
    }

    struct fy_node *item = NULL;
    int idx = 0;
    void *iter = NULL;
    while ((item = fy_node_sequence_iterate(root, &iter))) {
        if (UNLIKELY(1 != fy_node_scanf(item, "/ %ld", &offsets[idx++]))) {
            goto cleanup_on_error;
        }
    }

    block_index->offsets = offsets;
    block_index->size = count;
    block_index->cap = count;
    parser->block_index = block_index;
    event->type = ASDF_BLOCK_INDEX_EVENT;
    event->payload.block_index = block_index;
    res = ASDF_PARSE_EVENT;
    goto cleanup;

cleanup_on_error:
    free(offsets);
    free(block_index);
cleanup:
    fy_document_destroy(doc);
    TRY_SEEK(parser, cur_offset, SEEK_SET);
next_state:
    parser->state = ASDF_PARSER_STATE_TREE_OR_BLOCK;
    return res;
}


asdf_event_t *asdf_parser_parse(asdf_parser_t *parser) {
    assert(parser);
    assert(parser->stream);

    asdf_event_t *event = asdf_parse_event_alloc(parser);

    parse_result_t res = ASDF_PARSE_CONTINUE;

    while (ASDF_PARSE_CONTINUE == res) {
        switch (parser->state) {
        case ASDF_PARSER_STATE_ASDF_VERSION:
            res = parse_asdf_version(parser, event);
            break;
        case ASDF_PARSER_STATE_STANDARD_VERSION:
            res = parse_standard_version(parser, event);
            break;
        case ASDF_PARSER_STATE_COMMENT:
            res = parse_comment(parser, event);
            break;
        case ASDF_PARSER_STATE_TREE_OR_BLOCK:
            res = parse_tree_or_block(parser, event);
            break;
        case ASDF_PARSER_STATE_TREE:
            res = parse_tree(parser, event);
            break;
        case ASDF_PARSER_STATE_YAML:
            res = parse_yaml(parser, event);
            break;
        case ASDF_PARSER_STATE_BLOCK:
            res = parse_block(parser, event);
            break;
        case ASDF_PARSER_STATE_PADDING:
            res = parse_padding(parser, event);
            break;
        case ASDF_PARSER_STATE_BLOCK_INDEX:
            res = parse_block_index(parser, event);
            break;
        case ASDF_PARSER_STATE_END:
            if (!parser->done) {
                // Produce a valid ASDF_END_EVENT and return 0 (success)
                // but on subsequent calls produce no more valid events and return 1 (error)
                parser->done = true;
                event->type = ASDF_END_EVENT;
                return event;
            }
            return NULL;
        case ASDF_PARSER_STATE_ERROR:
            return NULL;
        default:
            asdf_parser_set_common_error(parser, ASDF_ERR_UNKNOWN_STATE);
            return NULL;
        }
    }

    return res == ASDF_PARSE_EVENT ? event : NULL;
}


/**
 * Default libasdf parser configuration
 */
static const asdf_parser_cfg_t default_asdf_parser_cfg = {.flags = 0};


asdf_parser_t *asdf_parser_create(asdf_parser_cfg_t *config) {
    asdf_parser_t *parser = calloc(1, sizeof(asdf_parser_t));

    if (!parser)
        return parser;

    parser->config = config ? config : &default_asdf_parser_cfg;
    parser->state = ASDF_PARSER_STATE_INITIAL;
    parser->error = NULL;
    parser->error_type = ASDF_ERROR_NONE;
    parser->done = false;
    ZERO_MEMORY(parser->asdf_version, sizeof(parser->asdf_version));
    ZERO_MEMORY(parser->standard_version, sizeof(parser->standard_version));
    return parser;
}


int asdf_parser_set_input_file(asdf_parser_t *parser, const char *filename) {
    assert(parser);
    parser->stream = asdf_stream_from_file(filename);

    if (!parser->stream) {
        // TODO: Better error handling for file opening errors
        // For now just use this generic error
        asdf_parser_set_common_error(parser, ASDF_ERR_STREAM_INIT_FAILED);
        return 1;
    }
    parser->state = ASDF_PARSER_STATE_ASDF_VERSION;
    return 0;
}


int asdf_parser_set_input_fp(asdf_parser_t *parser, FILE *file, const char *filename) {
    assert(parser);
    parser->stream = asdf_stream_from_fp(file, filename);

    if (!parser->stream) {
        // TODO: Better error handling for file opening errors
        // For now just use this generic error
        asdf_parser_set_common_error(parser, ASDF_ERR_STREAM_INIT_FAILED);
        return 1;
    }
    parser->state = ASDF_PARSER_STATE_ASDF_VERSION;
    return 0;
}


int asdf_parser_set_input_mem(asdf_parser_t *parser, const void *buf, size_t size) {
    assert(parser);
    parser->stream = asdf_stream_from_memory(buf, size);

    if (!parser->stream) {
        // TODO: Better error handling for file opening errors
        // For now just use this generic error
        asdf_parser_set_common_error(parser, ASDF_ERR_STREAM_INIT_FAILED);
        return 1;
    }
    parser->state = ASDF_PARSER_STATE_ASDF_VERSION;
    return 0;
}


bool asdf_parser_has_error(const asdf_parser_t *parser) {
    return parser->error != NULL;
}


const char *asdf_parser_get_error(const asdf_parser_t *parser) {
    return parser->error ? parser->error : "";
}


void asdf_parser_destroy(asdf_parser_t *parser) {
    if (!parser)
        return;

    fy_parser_destroy(parser->yaml_parser);

    if (parser->error_type == ASDF_ERROR_HEAP)
        free((void *)parser->error);

    if (parser->stream)
        parser->stream->close(parser->stream);

    free(parser->tree.buf);
    asdf_parse_event_freelist_free(parser);

    if (parser->block_index) {
        free(parser->block_index->offsets);
        free(parser->block_index);
    }

    free(parser);
}
