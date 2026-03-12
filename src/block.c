/**
 * ASDF block functions
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_MD5) && defined(HAVE_MD5_H)
#include <md5.h>
#endif

#include "block.h"
#include "compat/endian.h" // IWYU pragma: keep
#include "compression/compressor_registry.h"
#include "error.h"
#include "stream.h"
#include "util.h"


const unsigned char asdf_block_magic[] = {'\xd3', 'B', 'L', 'K'};

const char asdf_block_index_header[] = "#ASDF BLOCK INDEX";


void asdf_block_info_init(
    size_t index, const void *data, size_t size, asdf_block_info_t *out_block) {
    out_block->index = index;
    // Fill out the header for as much as we know (prior to compression)
    out_block->header = (asdf_block_header_t){
        .header_size = ASDF_BLOCK_HEADER_SIZE,
        .allocated_size = size,
        .used_size = size,
        .data_size = size};
    out_block->header_pos = -1;
    out_block->data_pos = -1;
    out_block->data = data;
}


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
        ASDF_ERROR_COMMON(stream, ASDF_ERR_INVALID_BLOCK_HEADER);
        return false;
    }

    if (avail < 2) {
        ASDF_ERROR_COMMON(stream, ASDF_ERR_UNEXPECTED_EOF);
        return false;
    }

    asdf_block_header_t *header = &out_block->header;
    // NOLINTNEXTLINE(readability-magic-numbers)
    header->header_size = (buf[0] << 8) | buf[1];
    if (header->header_size < ASDF_BLOCK_HEADER_SIZE) {
        ASDF_ERROR_COMMON(stream, ASDF_ERR_INVALID_BLOCK_HEADER);
        return false;
    }

    asdf_stream_consume(stream, FIELD_SIZEOF(asdf_block_header_t, header_size));

    if (UNLIKELY(ASDF_ERROR_GET(stream) != NULL))
        return false;

    buf = asdf_stream_next(stream, header->header_size, &avail);

    if (avail < header->header_size) {
        ASDF_ERROR_COMMON(stream, ASDF_ERR_UNEXPECTED_EOF);
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


#define WRITE_CHECK(stream, data, size) \
    do { \
        size_t n_written = asdf_stream_write((stream), (data), (size)); \
        if (n_written != (size)) { \
            ret = false; \
            goto cleanup; \
        } \
    } while (0)


bool asdf_block_info_write(asdf_stream_t *stream, asdf_block_info_t *block, bool checksum) {
    assert(stream);
    assert(stream->is_writeable);
    assert(block);

    bool ret = true;
    uint8_t *comp_buf = NULL;
    const void *write_data = block->write_data ? block->write_data : block->data;
    size_t write_size = block->write_data ? block->write_data_size : block->header.data_size;
    const asdf_compressor_t *compressor = block->write_compressor;

    /* Compress if a write compressor is set and there is data to compress */
    if (compressor != NULL && write_data != NULL) {
        if (compressor->comp(write_data, write_size, &comp_buf, &write_size) != 0) {
            ret = false;
            goto cleanup;
        }
        write_data = comp_buf;
    }

    block->header_pos = asdf_stream_tell(stream);
    WRITE_CHECK(stream, asdf_block_magic, ASDF_BLOCK_MAGIC_SIZE);

    uint16_t header_size = htobe16(ASDF_BLOCK_HEADER_SIZE);
    WRITE_CHECK(stream, &header_size, sizeof(uint16_t));

    uint32_t flags = htobe32(block->header.flags);
    WRITE_CHECK(stream, &flags, sizeof(uint32_t));

    /* Use compressor name for the compression field when compressing, else preserve
     * whatever was in the block header (e.g. when passing through already-compressed data) */
    char comp_field[ASDF_BLOCK_COMPRESSION_FIELD_SIZE] = {0};
    if (compressor != NULL)
        strncpy(comp_field, compressor->compression, ASDF_BLOCK_COMPRESSION_FIELD_SIZE);
    else
        memcpy(comp_field, block->header.compression, ASDF_BLOCK_COMPRESSION_FIELD_SIZE);
    WRITE_CHECK(stream, comp_field, ASDF_BLOCK_COMPRESSION_FIELD_SIZE);

    // allocated_size -- generally same as used_size, but could be useful to add
    // an option to reserve more size for a block to grow
    uint64_t alloc_size = htobe64(write_size);
    WRITE_CHECK(stream, &alloc_size, sizeof(uint64_t));
    // used_size -- compressed size when compressed, otherwise same as data_size
    WRITE_CHECK(stream, &alloc_size, sizeof(uint64_t));
    // data_size -- always the uncompressed size
    uint64_t data_size = htobe64(block->header.data_size);
    WRITE_CHECK(stream, &data_size, sizeof(uint64_t));

#ifdef HAVE_MD5
    if (checksum) {
        asdf_md5_ctx_t md5_ctx = {0};
        asdf_md5_init(&md5_ctx);
        asdf_md5_update(&md5_ctx, write_data, write_size);
        asdf_md5_final(&md5_ctx, (unsigned char *)&block->header.checksum);
    } else {
        ASDF_LOG(stream, ASDF_LOG_DEBUG, "block checksum calculation disabled by emitter flags");
    }
#else
    (void)checksum;
    ASDF_LOG(
        stream,
        ASDF_LOG_WARN,
        PACKAGE_NAME " was compiled without MD5 support; block "
                     "checksum will not be written");
#endif
    WRITE_CHECK(stream, block->header.checksum, ASDF_BLOCK_CHECKSUM_FIELD_SIZE);

    block->data_pos = asdf_stream_tell(stream);
    WRITE_CHECK(stream, write_data, write_size);

cleanup:
    free(comp_buf);
    if (block->owns_write_data) {
        free((void *)block->write_data);
        block->write_data = NULL;
        block->write_data_size = 0;
        block->owns_write_data = false;
    }
    return ret;
}


int asdf_block_info_compression_set(
    asdf_file_t *file, asdf_block_info_t *block_info, const char *compression) {
    if (UNLIKELY(!file || !block_info))
        return -1;

    const asdf_compressor_t *comp = asdf_compressor_get(file, compression);

    if (!comp) {
        ASDF_ERROR_COMMON(file, ASDF_ERR_UNKNOWN_COMPRESSION, compression);
        return -1;
    }

    block_info->write_compressor = comp;
    return 0;
}


#ifdef HAVE_MD5
#ifdef HAVE_MD5_H
/** libbsd md5.h implementation (only one currently available) */
void asdf_md5_init(asdf_md5_ctx_t *ctx) {
    MD5Init(&ctx->ctx);
}


void asdf_md5_update(asdf_md5_ctx_t *ctx, const void *data, size_t len) {
    MD5Update(&ctx->ctx, data, len);
}


void asdf_md5_final(asdf_md5_ctx_t *ctx, unsigned char digest[16]) {
    MD5Final(digest, &ctx->ctx);
}
#endif
#endif
