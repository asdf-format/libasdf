/**
 * LZ4 decompressor compatible with how the Python asdf library writes LZ4-compressed blocks
 *
 * Format expected in the raw block data:
 *
 *   [ 4-byte big-endian compressed-size ] [ raw LZ4 block bytes ]
 *   [ 4-byte big-endian compressed-size ] [ raw LZ4 block bytes ]
 *   ...
 */

#include <assert.h>
#include <endian.h>
#include <stdbool.h>

#include <lz4.h>

#include "../compat/endian.h"
#include "../error.h"
#include "../file.h"
#include "../log.h"
#include "../util.h"

#include "compression.h"
#include "compressor_registry.h"


#define ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE 4


typedef struct {
    asdf_compressor_status_t status;
    asdf_file_t *file;
    uint8_t *next_in;
    size_t avail_in;

    /** Manage buffer for the LZ4 block header */
    struct {
        uint8_t buf[ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE];
        size_t pos;
    } header;

    /** Manage buffer for the LZ4 blocks */
    struct {
        uint8_t *buf;
        size_t size;
        size_t cap;
        size_t pos;
    } block;
} asdf_compressor_lz4_userdata_t;


static asdf_compressor_userdata_t *asdf_compressor_lz4_init(
    asdf_block_t *block, UNUSED(const void *dest), UNUSED(size_t dest_size)) {
    asdf_compressor_lz4_userdata_t *userdata = NULL;

    userdata = calloc(1, sizeof(asdf_compressor_lz4_userdata_t));

    if (!userdata) {
        ASDF_ERROR_OOM(block->file);
        return NULL;
    }

    userdata->status = ASDF_COMPRESSOR_INITIALIZED;
    userdata->file = block->file;
    userdata->next_in = block->data;
    userdata->avail_in = block->avail_size;
    return userdata;
}


static void asdf_compressor_lz4_destroy(asdf_compressor_userdata_t *userdata) {
    assert(userdata);
    asdf_compressor_lz4_userdata_t *lz4 = userdata;
    free(lz4->block.buf);
    free(lz4);
}


static asdf_compressor_status_t asdf_compressor_lz4_status(asdf_compressor_userdata_t *userdata) {
    assert(userdata);
    asdf_compressor_lz4_userdata_t *lz4 = userdata;
    return lz4->status;
}


/**
 * LZ4 doesn't have a stream interface like zlib and libbz2, so this implements our own similar
 */
static int asdf_compressor_lz4_decomp(
    asdf_compressor_userdata_t *userdata, uint8_t *buf, size_t buf_size) {
    assert(userdata);
    asdf_compressor_lz4_userdata_t *lz4 = userdata;
    lz4->status = ASDF_COMPRESSOR_IN_PROGRESS;

    // Read block length "header" field
    while (lz4->header.pos < ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE && lz4->avail_in > 0) {
        lz4->header.buf[lz4->header.pos++] = *lz4->next_in++;
        lz4->avail_in--;
    }

    if (lz4->header.pos < ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE)
        return 0;

    if (lz4->block.size == 0) {
        uint32_t block_size = 0;
        memcpy(&block_size, lz4->header.buf, ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE);
        lz4->block.size = be32toh(block_size);

        if (lz4->block.size == 0) {
            // Zero-width compressed block encountered--I guess done?
            goto done;
        }

        if (!lz4->block.buf) {
            lz4->block.buf = malloc(lz4->block.size);

            if (!lz4->block.buf) {
                ASDF_ERROR_OOM(lz4->file);
                return -1;
            }

            lz4->block.cap = lz4->block.size;
        } else if (lz4->block.cap < lz4->block.size) {
            uint8_t *orig_block_buf = lz4->block.buf;
            lz4->block.buf = realloc(lz4->block.buf, lz4->block.size);

            if (!lz4->block.buf) {
                free(orig_block_buf);
                ASDF_ERROR_OOM(lz4->file);
                return -1;
            }

            lz4->block.cap = lz4->block.size;
        }

        lz4->block.pos = 0;
    }

    if (lz4->block.pos < lz4->block.size && lz4->avail_in > 0) {
        size_t need = lz4->block.size - lz4->block.pos;
        size_t take = (lz4->avail_in < need) ? lz4->avail_in : need;
        memcpy(lz4->block.buf + lz4->block.pos, lz4->next_in, take);
        lz4->block.pos += take;
        lz4->next_in += take;
        lz4->avail_in -= take;
    }

    if (lz4->block.pos < lz4->block.size)
        // Need more input
        return 0;

    int ret = LZ4_decompress_safe(
        (const char *)lz4->block.buf, (char *)buf, (int)lz4->block.size, (int)buf_size);

    if (ret < 0) {
        ASDF_LOG(lz4->file, ASDF_LOG_ERROR, "LZ4 block decompression failed: %d", ret);
        return ret;
    }

    lz4->block.size = 0;
    lz4->block.pos = 0;
    lz4->header.pos = 0;

    if (lz4->avail_in == 0)
        goto done;

    return 0;
done:
    lz4->status = ASDF_COMPRESSOR_DONE;
    return 0;
}


ASDF_REGISTER_COMPRESSOR(
    lz4,
    asdf_compressor_lz4_init,
    asdf_compressor_lz4_destroy,
    asdf_compressor_lz4_status,
    asdf_compressor_lz4_decomp);
