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
#include <inttypes.h>
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


/** A parsed and validated LZ4 block header */
typedef struct {
    /**
     * Size of the raw LZ4 block following the header
     *
     * This excludes the decompressed size field, unlike the size stored in
     * the header itself, so the next header is ``comp_size`` bytes after
     * the end of this one.
     */
    uint32_t comp_size;
    /** Size of the LZ4 block once decompressed */
    uint32_t decomp_size;
} asdf_compressor_lz4_block_header_t;


typedef struct {
    asdf_compressor_info_t info;
    asdf_file_t *file;
    uint8_t *data;
    size_t data_size;
    /** Total size of the decompressed data */
    size_t dest_size;
    /** Position in ``data`` of the next LZ4 block header to read */
    size_t pos;
    size_t progress;

    /**
     * Intermediate buffer for decompressed blocks
     *
     * These are normally all the same size (except for the last block which
     * may be smaller), but the capacity is tracked separately in case a
     * malformed file declares a later block larger than the first.
     *
     * Later will maybe have a collection of these since for some cases, like tile reading,
     * it will be useful to have multiple fully decompressed but partially read blocks in memory
     * at once.
     */
    struct {
        uint8_t *buf;
        size_t cap;
        size_t size;
        size_t pos;
    } block;
} asdf_compressor_lz4_userdata_t;


/**
 * Parse and validate the LZ4 block header at ``pos`` in the compressed data
 *
 * Returns 0 on success, 1 if the compressed data ends before the end of the
 * header (which is not logged, as what that means depends on the caller), or
 * -1 if the header is invalid (which is logged).  On success the header is
 * also known to describe an LZ4 block that lies within the compressed data
 * and whose sizes fit the int arguments of the LZ4 API.
 */
static int asdf_compressor_lz4_header_parse(
    const asdf_compressor_lz4_userdata_t *lz4,
    size_t pos,
    asdf_compressor_lz4_block_header_t *header) {
    assert(lz4);
    assert(lz4->data);
    assert(header);
    assert(pos <= lz4->data_size);

    if (lz4->data_size - pos < ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE)
        return 1;

    uint32_t size = 0;
    uint32_t decomp_size = 0;
    memcpy(&size, lz4->data + pos, sizeof(size));
    memcpy(&decomp_size, lz4->data + pos + sizeof(size), sizeof(decomp_size));
    // Python asdf's big-endian size of the LZ4 block includes python-lz4's
    // little-endian decompressed size field
    size = be32toh(size);
    decomp_size = le32toh(decomp_size);

    if (size <= sizeof(decomp_size)) {
        ASDF_LOG(
            lz4->file, ASDF_LOG_ERROR, "zero-width LZ4 block encountered; aborting decompression");
        return -1;
    }

    uint32_t comp_size = size - (uint32_t)sizeof(decomp_size);
    size_t remaining = lz4->data_size - pos - ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE;

    if (comp_size > remaining) {
        ASDF_LOG(
            lz4->file,
            ASDF_LOG_ERROR,
            "LZ4 block size (%" PRIu32 ") exceeds the remaining compressed data (%zu); aborting "
            "decompression",
            comp_size,
            remaining);
        return -1;
    }

    if (comp_size > INT32_MAX) {
        ASDF_LOG(
            lz4->file,
            ASDF_LOG_ERROR,
            "LZ4 block size (%" PRIu32 ") is too large to decompress; aborting decompression",
            comp_size);
        return -1;
    }

    if (decomp_size > INT32_MAX) {
        ASDF_LOG(
            lz4->file,
            ASDF_LOG_ERROR,
            "invalid decompressed size LZ4 block encountered (%" PRIu32 "); aborting decompression",
            decomp_size);
        return -1;
    }

    header->comp_size = comp_size;
    header->decomp_size = decomp_size;
    return 0;
}


static asdf_compressor_userdata_t *asdf_compressor_lz4_init(
    const asdf_block_t *block, UNUSED(const void *dest), size_t dest_size) {
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
    userdata->dest_size = dest_size;
    userdata->pos = 0;
    userdata->progress = 0;

    // Peek at the first block header, which also means a compressed block
    // whose first LZ4 block is unusable fails here rather than on first read
    asdf_compressor_lz4_block_header_t header = {0};
    int ret = asdf_compressor_lz4_header_parse(userdata, 0, &header);

    if (ret != 0) {
        if (ret > 0)
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
    userdata->info.optimal_chunk_size = header.decomp_size;
    return userdata;
}


static void asdf_compressor_lz4_destroy(asdf_compressor_userdata_t *userdata) {
    asdf_compressor_lz4_userdata_t *lz4 = userdata;

    if (!lz4)
        return;

    free(lz4->block.buf);
    free(lz4);
}


static const asdf_compressor_info_t *asdf_compressor_lz4_info(
    asdf_compressor_userdata_t *userdata) {
    assert(userdata);
    asdf_compressor_lz4_userdata_t *lz4 = userdata;
    return &lz4->info;
}


/** Decompress the next LZ4 block into ``lz4->block`` and advance past it */
static int asdf_compressor_lz4_read_block(asdf_compressor_lz4_userdata_t *lz4) {
    asdf_compressor_lz4_block_header_t header = {0};
    int ret = asdf_compressor_lz4_header_parse(lz4, lz4->pos, &header);

    if (ret > 0)
        ASDF_LOG(
            lz4->file,
            ASDF_LOG_ERROR,
            "LZ4 compressed data ended before the full decompressed size was reached");

    if (ret != 0)
        return -1;

    // Allocate buffer for decompressed block
    // This is normally the same size through the entire compressed block
    // except possibly for the last LZ4 block which may be smaller
    if (header.decomp_size > lz4->block.cap) {
        uint8_t *buf = realloc(lz4->block.buf, header.decomp_size);

        if (!buf) {
            ASDF_ERROR_OOM(lz4->file);
            return -1;
        }

        lz4->block.buf = buf;
        lz4->block.cap = header.decomp_size;
    }

    lz4->block.size = header.decomp_size;
    lz4->block.pos = 0;

    // header_parse ensures both sizes fit in an int
    ret = LZ4_decompress_safe(
        (const char *)lz4->data + lz4->pos + ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE,
        (char *)lz4->block.buf,
        (int)header.comp_size,
        (int)header.decomp_size);

    if (ret < 0) {
        ASDF_LOG(lz4->file, ASDF_LOG_ERROR, "LZ4 block decompression failed: %d", ret);
        return ret;
    }

    if ((size_t)ret != lz4->block.size) {
        ASDF_LOG(
            lz4->file,
            ASDF_LOG_ERROR,
            "LZ4 block decompressed to %d bytes, but its header declares %zu bytes",
            ret,
            lz4->block.size);
        return -1;
    }

    lz4->pos += ASDF_COMPRESSOR_LZ4_BLOCK_HEADER_SIZE + header.comp_size;
    return 0;
}


/**
 * Size of blocks to compress
 *
 * This is the same default as used by the Python library; later we could make
 * this configurable if such a desire is ever expressed.
 */
#define ASDF_COMPRESSOR_LZ4_BLOCK_SIZE (1u << 22) /* 4 MB */


static int asdf_compressor_lz4_comp(
    const uint8_t *buf, size_t buf_size, uint8_t **out, size_t *out_size) {
    if (UNLIKELY(!buf || !out || !out_size))
        return -1;

    size_t capacity = 0;
    size_t offset = 0;
    *out = NULL;
    *out_size = 0;

    /* First pass: compute worst-case total size */
    while (offset < buf_size) {
        size_t chunk_size = buf_size - offset;
        if (chunk_size > ASDF_COMPRESSOR_LZ4_BLOCK_SIZE)
            chunk_size = ASDF_COMPRESSOR_LZ4_BLOCK_SIZE;

        int bound = LZ4_compressBound((int)chunk_size);
        if (bound <= 0)
            return -1;

        capacity += sizeof(uint32_t); /* BE compressed-size */
        capacity += sizeof(uint32_t); /* LE decompressed chunk size */
        capacity += (size_t)bound;
        offset += chunk_size;
    }

    uint8_t *output = malloc(capacity);

    if (!output)
        return -1;

    offset = 0;
    size_t out_offset = 0;

    while (offset < buf_size) {
        size_t chunk_size = buf_size - offset;
        if (chunk_size > ASDF_COMPRESSOR_LZ4_BLOCK_SIZE)
            chunk_size = ASDF_COMPRESSOR_LZ4_BLOCK_SIZE;

        int bound = LZ4_compressBound((int)chunk_size);

        /* Compress after both headers */
        int compressed_size = LZ4_compress_default(
            (const char *)(buf + offset),
            (char *)(output + out_offset + 2 * sizeof(uint32_t)),
            (int)chunk_size,
            bound);

        if (compressed_size <= 0) {
            free(output);
            return -1;
        }

        /* BE: compressed size + sizeof(uint32_t) (includes python-lz4 decompressed-size header) */
        uint32_t be_size = htobe32((uint32_t)(compressed_size + sizeof(uint32_t)));
        memcpy(output + out_offset, &be_size, sizeof(uint32_t));

        /* LE: decompressed chunk size */
        uint32_t le_chunk_size = htole32((uint32_t)chunk_size);
        memcpy(output + out_offset + sizeof(uint32_t), &le_chunk_size, sizeof(uint32_t));

        out_offset += 2 * sizeof(uint32_t) + (size_t)compressed_size;
        offset += chunk_size;
    }

    *out = output;
    *out_size = out_offset;

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

    // The last chunk may be partial; the remainder of buf is left untouched
    size_t remaining = lz4->dest_size - lz4->progress;
    size_t need = buf_size < remaining ? buf_size : remaining;
    size_t pos = 0;

    while (need > 0) {
        size_t avail = lz4->block.size - lz4->block.pos;

        if (avail == 0) {
            int ret = asdf_compressor_lz4_read_block(lz4);

            if (ret != 0)
                return ret;

            avail = lz4->block.size;

            if (avail == 0) {
                ASDF_LOG(
                    lz4->file,
                    ASDF_LOG_ERROR,
                    "empty LZ4 block encountered; aborting decompression");
                return -1;
            }
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

    if (lz4->progress >= lz4->dest_size)
        lz4->info.status = ASDF_COMPRESSOR_DONE;

    return 0;
}


ASDF_REGISTER_COMPRESSOR(
    lz4,
    asdf_compressor_lz4_init,
    asdf_compressor_lz4_destroy,
    asdf_compressor_lz4_info,
    asdf_compressor_lz4_comp,
    asdf_compressor_lz4_decomp);
