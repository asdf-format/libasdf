/**
 * LZ4 decompressor compatible with how the Python asdf library writes LZ4-compressed blocks
 *
 * Format expected in the raw block data:
 *
 *   [ 4B big-endian compressed-size ] [ 4B little-endian decompressed-size] [ raw LZ4 block ]
 *   [ 4B big-endian compressed-size ] [ 4B little-endian decompressed-size] [ raw LZ4 block ]
 *   ...
 *
 * Why is this so wonky?  The Python LZ4 library has its own ad-hoc scheme that
 * prepends each compressed block with its decompressed size as a 32-bit
 * integer stored little-endian.  This is a feature enabled by default in the
 * library and also used in the Python asdf library.  Its use is somewhat
 * implicit in the design, and though technically it can be disabled by users
 * of the library, doing so renders the files effectively unreadable in a round
 * trip through Python asdf.  So the default assumption is that this block
 * header is always included; see discussion on
 * https://github.com/asdf-format/asdf/issues/1574#issuecomment-3585588065
 *
 * Furthermore, the Python asdf prepends to this its own ad-hoc header
 * containing the *compressed* size of the block, again as a 32-bit integer
 * but in network (big-endian) byte order for some reason.  An additional
 * subtlety is this size includes the decompressed-size header output by
 * python-lz4 (it just takes `len(lz4.block.decompress(....)))`).
 *
 * The Python library also divides the decompressed data into equal-sized
 * chunks, with a default size of 4MB.  It's implicit, but not enforced, that
 * the chunk size is thus small enough to fit in a 32-bit signed integer.
 */

#include <assert.h>
#include <stdbool.h>

#include <lz4.h>

#include "../compat/endian.h"
#include "../error.h"
#include "../file.h"
#include "../log.h"
#include "../util.h"

#include "compression.h"
#include "compressor_registry.h"


#define ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE 8


typedef struct {
    asdf_compressor_info_t info;
    asdf_file_t *file;
    uint8_t *data;
    size_t data_size;
    size_t pos;
    size_t progress;

    /** Manage buffer for the LZ4 block header */
    struct {
        uint8_t buf[ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE];
        size_t pos;
        int32_t block_size;
        int32_t decomp_block_size;
    } header;

    /**
     * Intermediate buffer for decompressed blocks
     *
     * These are always the same size so no need for a separate capacity (except for the last
     * block which may be smaller)
     *
     * Later will maybe have a collection of these since for some cases, like tile reading,
     * it will be useful to have multiple fully decompressed but partially read blocks in memory
     * at once.
     */
    struct {
        uint8_t *buf;
        size_t size;
        size_t pos;
    } block;
} asdf_compressor_lz4_userdata_t;


/**
 * Read one block header from the current position of the input
 *
 * If the header.pos is already at ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE, then
 * the current header is considered already read; reset the position to 0 to
 * re-read from the current input position.
 */
static int asdf_compressor_lz4_read_header(asdf_compressor_lz4_userdata_t *lz4) {
    assert(lz4);
    assert(lz4->data);

    if (lz4->header.pos == ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE)
        return 0;

    while (lz4->header.pos < ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE && lz4->pos < lz4->data_size)
        lz4->header.buf[lz4->header.pos++] = *(lz4->data + lz4->pos++);

    if (lz4->header.pos < ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE)
        return ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE - lz4->header.pos;

    int32_t block_size = 0;
    memcpy(&block_size, lz4->header.buf, sizeof(int32_t));

    // Exclude the decompressed-size header from python-lz4 in the block_size
    lz4->header.block_size = be32toh(block_size) - sizeof(int32_t);

    int32_t decomp_block_size = 0;
    memcpy(&decomp_block_size, lz4->header.buf + sizeof(int32_t), sizeof(int32_t));
    lz4->header.decomp_block_size = le32toh(decomp_block_size);
    return 0;
}


static asdf_compressor_userdata_t *asdf_compressor_lz4_init(
    asdf_block_t *block, UNUSED(const void *dest), UNUSED(size_t dest_size)) {
    asdf_compressor_lz4_userdata_t *userdata = NULL;

    userdata = calloc(1, sizeof(asdf_compressor_lz4_userdata_t));

    if (!userdata) {
        ASDF_ERROR_OOM(block->file);
        return NULL;
    }

    userdata->info.status = ASDF_COMPRESSOR_INITIALIZED;
    userdata->file = block->file;
    userdata->data = block->data;
    userdata->data_size = block->avail_size;
    userdata->pos = 0;
    userdata->progress = 0;

    // Try to read the first block header; if not enough bytes are found
    // return NULL and indicate an error
    if (asdf_compressor_lz4_read_header(userdata) != 0) {
        ASDF_LOG(
            block->file,
            ASDF_LOG_ERROR,
            "could not read first LZ4 block header of compressed block; decompression not "
            "possible");
        free(userdata);
        return NULL;
    }

    // Set the optimal chunk size based on the decompressed size of the blocks
    // (which is the same for every block except possibly the last)
    // By default, currently, Python asdf sets this to 4MB so already a
    // multiple of the system page size on most systems, which is ideal.
    // However, the main compression handler will always round this to the
    // nearest page size, so in theory it may be necessary to decode multiple
    // blocks to fill one multi-page decompression chunk
    userdata->info.optimal_chunk_size = userdata->header.decomp_block_size;
    return userdata;
}


static void asdf_compressor_lz4_destroy(asdf_compressor_userdata_t *userdata) {
    assert(userdata);
    asdf_compressor_lz4_userdata_t *lz4 = userdata;
    free(lz4->block.buf);
    free(lz4);
}


static const asdf_compressor_info_t *asdf_compressor_lz4_info(
    asdf_compressor_userdata_t *userdata) {
    assert(userdata);
    asdf_compressor_lz4_userdata_t *lz4 = userdata;
    return &lz4->info;
}


static int asdf_compresser_lz4_read_block(asdf_compressor_lz4_userdata_t *lz4) {
    if (asdf_compressor_lz4_read_header(lz4) != 0)
        return 0;

    if (lz4->header.block_size <= 0) {
        // Zero-width compressed block encountered--I guess done?
        ASDF_LOG(
            lz4->file, ASDF_LOG_ERROR, "zero-width LZ4 block encountered; aborting decompression");
        return -1;
    }

    if (lz4->header.decomp_block_size < 0) {
        // Zero-width compressed block encountered--I guess done?
        ASDF_LOG(
            lz4->file,
            ASDF_LOG_ERROR,
            "invalid decompressed size LZ4 block encountered (%d); aborting decompression",
            lz4->header.decomp_block_size);
        return -1;
    }

    // Allocate buffer for decompressed block
    // This is always the same size through the entire compressed block
    // except possibly for the last LZ4 block which may be smaller but
    // never larger
    if (!lz4->block.buf) {
        lz4->block.buf = malloc(lz4->header.decomp_block_size);

        if (!lz4->block.buf) {
            ASDF_ERROR_OOM(lz4->file);
            return -1;
        }
    }

    lz4->block.size = lz4->header.decomp_block_size;
    lz4->block.pos = 0;

    int ret = LZ4_decompress_safe(
        (const char *)lz4->data + lz4->pos,
        (char *)lz4->block.buf,
        (int)lz4->header.block_size,
        (int)lz4->block.size);

    if (ret < 0) {
        ASDF_LOG(lz4->file, ASDF_LOG_ERROR, "LZ4 block decompression failed: %d", ret);
        return ret;
    }

    lz4->pos += lz4->header.block_size;
    return 0;
}


/**
 * LZ4 doesn't have a stream interface like zlib and libbz2, so this implements our own similar
 *
 * .. todo::
 *
 *   Fix this so that it can use offset_hint to actually decompress only the requested chunk
 */
static int asdf_compressor_lz4_decomp(
    asdf_compressor_userdata_t *userdata,
    uint8_t *buf,
    size_t buf_size,
    size_t *offset_out,
    size_t offset_hint) {
    assert(userdata);
    asdf_compressor_lz4_userdata_t *lz4 = userdata;
    lz4->info.status = ASDF_COMPRESSOR_IN_PROGRESS;

    if (offset_hint < lz4->progress)
        return 0;

    size_t need = buf_size;
    size_t pos = 0;

    while (need > 0) {
        size_t avail = lz4->block.size - lz4->block.pos;

        if (avail == 0) {
            int ret = asdf_compresser_lz4_read_block(lz4);

            if (ret != 0)
                return ret;

            avail = lz4->block.size;
        }

        size_t take = avail < need ? avail : need;
        memcpy(buf + pos, lz4->block.buf + lz4->block.pos, take);
        pos += take;
        lz4->block.pos += take;
        need -= take;
    }

    if (offset_out)
        *offset_out = lz4->progress;

    lz4->progress += buf_size;

    if (lz4->progress >= (uint32_t)lz4->header.decomp_block_size)
        lz4->info.status = ASDF_COMPRESSOR_DONE;

    return 0;
}


ASDF_REGISTER_COMPRESSOR(
    lz4,
    asdf_compressor_lz4_init,
    asdf_compressor_lz4_destroy,
    asdf_compressor_lz4_info,
    asdf_compressor_lz4_decomp);
