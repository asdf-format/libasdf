/**
 * ASDF block functions
 */

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "compat/endian.h"
#include "error.h"
#include "file.h"
#include "log.h"
#include "parse.h"
#include "parse_util.h"
#include "stream.h"
#include "util.h"


const unsigned char asdf_block_magic[] = {'\xd3', 'B', 'L', 'K'};

const char asdf_block_index_header[] = "#ASDF BLOCK INDEX";


/**
 * Parse a block header pointed to by the current stream position
 *
 * On success, allocates an ``asdf_block_info_t`` on the heap, to be freed by the caller.
 */
asdf_block_info_t *asdf_block_read_info(asdf_parser_t *parser) {
    asdf_stream_t *stream = parser->stream;
    size_t header_pos = asdf_stream_tell(stream);
    size_t avail = 0;
    const uint8_t *buf = NULL;

    // TODO: ASDF 2.0.0 proposes adding a checksum to the block header
    // Here we will want to check that as well.
    // In fact we should probably ignore anything that starts with a block
    // magic but then contains garbage.  But we will need some heuristics
    // for what counts as "garbage"
    // Go ahead and allocate storage for the block info
    asdf_block_info_t *block = calloc(1, sizeof(asdf_block_info_t));

    if (!block) {
        ASDF_ERROR_OOM(parser);
        goto error;
    }

    asdf_stream_consume(stream, ASDF_BLOCK_MAGIC_SIZE);

    if (UNLIKELY(ASDF_ERROR_GET(parser) != NULL))
        goto error;

    buf = asdf_stream_next(stream, FIELD_SIZEOF(asdf_block_header_t, header_size), &avail);

    if (!buf) {
        ASDF_ERROR_STATIC(parser, "Failed to seek past block magic");
        goto error;
    }

    if (avail < 2) {
        ASDF_ERROR_STATIC(parser, "Failed to read block header size");
        goto error;
    }

    asdf_block_header_t *header = &block->header;
    // NOLINTNEXTLINE(readability-magic-numbers)
    header->header_size = (buf[0] << 8) | buf[1];
    if (header->header_size < ASDF_BLOCK_HEADER_SIZE) {
        ASDF_ERROR_STATIC(parser, "Invalid block header size");
        goto error;
    }

    asdf_stream_consume(stream, FIELD_SIZEOF(asdf_block_header_t, header_size));

    if (UNLIKELY(ASDF_ERROR_GET(parser) != NULL)) {
        goto error;
    }

    buf = asdf_stream_next(stream, header->header_size, &avail);

    if (avail < header->header_size) {
        ASDF_ERROR_STATIC(parser, "Failed to read full block header");
        goto error;
    }

    // Parse block fields
    uint32_t flags =
        // NOLINTNEXTLINE(readability-magic-numbers)
        (((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3]);
    memcpy(
        header->compression,
        (char *)buf + ASDF_BLOCK_COMPRESSION_OFFSET,
        sizeof(header->compression));

    uint64_t allocated_size = 0;
    uint64_t used_size = 0;
    uint64_t data_size = 0;
    memcpy(&allocated_size, buf + ASDF_BLOCK_ALLOCATED_SIZE_OFFSET, sizeof(allocated_size));
    memcpy(&used_size, buf + ASDF_BLOCK_USED_SIZE_OFFSET, sizeof(used_size));
    memcpy(&data_size, buf + ASDF_BLOCK_DATA_SIZE_OFFSET, sizeof(data_size));

    header->flags = flags;
    header->allocated_size = be64toh(allocated_size);
    header->used_size = be64toh(used_size);
    header->data_size = be64toh(data_size);
    memcpy(header->checksum, buf + ASDF_BLOCK_CHECKSUM_OFFSET, sizeof(header->checksum));

    asdf_stream_consume(stream, header->header_size);

    if (UNLIKELY(ASDF_ERROR_GET(parser) != NULL)) {
        goto error;
    }

    block->header_pos = header_pos;
    block->data_pos = asdf_stream_tell(parser->stream);
    return block;
error:
    free(block);
    return NULL;
}


asdf_block_index_t *asdf_block_index_init(size_t cap) {
    asdf_block_index_t *block_index = malloc(sizeof(asdf_block_index_t));

    if (!block_index)
        return NULL;

    block_index->offsets = malloc(cap * sizeof(off_t));

    if (!block_index->offsets) {
        free(block_index);
        return NULL;
    }

    memset(block_index->offsets, -1, cap);
    block_index->size = 0;
    block_index->cap = cap;
    return block_index;
}


asdf_block_index_t *asdf_block_index_resize(asdf_block_index_t *block_index, size_t cap) {
    if (!block_index)
        return asdf_block_index_init(cap);

    if (cap <= block_index->cap)
        return block_index;

    if (cap > block_index->cap) {
        off_t *new_offsets = realloc(block_index->offsets, cap);

        if (!new_offsets)
            return NULL;

        memset(block_index + block_index->size, -1, cap - block_index->size);
        block_index->offsets = new_offsets;
        block_index->cap = cap;
    }

    return block_index;
}


void asdf_block_index_free(asdf_block_index_t *block_index) {
    if (!block_index)
        return;

    free(block_index->offsets);
    free(block_index);
}
