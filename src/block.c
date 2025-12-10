/**
 * ASDF block functions
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "compat/endian.h"
#include "error.h"
#include "stream.h"
#include "util.h"


const unsigned char asdf_block_magic[] = {'\xd3', 'B', 'L', 'K'};

const char asdf_block_index_header[] = "#ASDF BLOCK INDEX";


/**
 * Parse a block header pointed to by the current stream position
 *
 * Assigns the block info into the allocated `asdf_block_info_t *` output,
 * and returns true if a block could be read successfully.
 */
bool asdf_block_info_read(asdf_stream_t *stream, asdf_block_info_t *out_block) {
    off_t header_pos = asdf_stream_tell(stream);
    size_t avail = 0;
    const uint8_t *buf = NULL;

    // TODO: ASDF 2.0.0 proposes adding a checksum to the block header
    // Here we will want to check that as well.
    // In fact we should probably ignore anything that starts with a block
    // magic but then contains garbage.  But we will need some heuristics
    // for what counts as "garbage"
    // Go ahead and allocate storage for the block info
    asdf_stream_consume(stream, ASDF_BLOCK_MAGIC_SIZE);

    if (UNLIKELY(ASDF_ERROR_GET(stream) != NULL))
        return false;

    buf = asdf_stream_next(stream, FIELD_SIZEOF(asdf_block_header_t, header_size), &avail);

    if (!buf) {
        ASDF_ERROR_STATIC(stream, "Failed to seek past block magic");
        return false;
    }

    if (avail < 2) {
        ASDF_ERROR_STATIC(stream, "Failed to read block header size");
        return false;
    }

    asdf_block_header_t *header = &out_block->header;
    // NOLINTNEXTLINE(readability-magic-numbers)
    header->header_size = (buf[0] << 8) | buf[1];
    if (header->header_size < ASDF_BLOCK_HEADER_SIZE) {
        ASDF_ERROR_STATIC(stream, "Invalid block header size");
        return false;
    }

    asdf_stream_consume(stream, FIELD_SIZEOF(asdf_block_header_t, header_size));

    if (UNLIKELY(ASDF_ERROR_GET(stream) != NULL))
        return false;

    buf = asdf_stream_next(stream, header->header_size, &avail);

    if (avail < header->header_size) {
        ASDF_ERROR_STATIC(stream, "Failed to read full block header");
        return false;
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

    if (UNLIKELY(ASDF_ERROR_GET(stream) != NULL))
        return false;

    out_block->header_pos = header_pos;
    out_block->data_pos = asdf_stream_tell(stream);
    return true;
}
