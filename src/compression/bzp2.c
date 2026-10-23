#include <assert.h>
#include <stdbool.h>

#include <bzlib.h>


#include "../error.h"
#include "../file.h"
#include "../log.h"

#include "compression.h"
#include "compressor_registry.h"


typedef struct {
    asdf_compressor_info_t info;
    bz_stream bz;
    size_t progress;
} asdf_compressor_bzp2_userdata_t;


static asdf_compressor_userdata_t *asdf_compressor_bzp2_init(
    const asdf_block_t *block, const void *dest, size_t dest_size) {
    asdf_compressor_bzp2_userdata_t *userdata = NULL;

    userdata = calloc(1, sizeof(asdf_compressor_bzp2_userdata_t));

    if (!userdata) {
        ASDF_ERROR_OOM(block->file);
        return NULL;
    }

    bz_stream *stream = &userdata->bz;
    stream->next_in = (char *)block->data;
    stream->avail_in = block->avail_size;
    stream->next_out = (char *)dest;
    stream->avail_out = dest_size;

    int ret = BZ2_bzDecompressInit(stream, 0, 0);
    if (ret != BZ_OK) {
        ASDF_LOG(block->file, ASDF_LOG_ERROR, "error initializing bzip2 stream: %d", ret);
        return NULL;
    }

    userdata->info.status = ASDF_COMPRESSOR_INITIALIZED;
    userdata->info.optimal_chunk_size = 0;
    userdata->progress = 0;
    return userdata;
}


static void asdf_compressor_bzp2_destroy(asdf_compressor_userdata_t *userdata) {
    asdf_compressor_bzp2_userdata_t *bzp2 = userdata;

    if (!bzp2)
        return;

    if (bzp2->info.status != ASDF_COMPRESSOR_UNINITIALIZED)
        BZ2_bzDecompressEnd(&bzp2->bz);

    free(bzp2);
}


static const asdf_compressor_info_t *asdf_compressor_bzp2_info(
    asdf_compressor_userdata_t *userdata) {
    assert(userdata);
    asdf_compressor_bzp2_userdata_t *bzp2 = userdata;
    return &bzp2->info;
}


#define ASDF_COMPRESSOR_BZP2_BLOCK_SIZE 9
#define ASDF_COMPRESSOR_BZP2_WORK_FACTOR 30
/**
 * From the bzip2 manual:
 *
 *     To guarantee that the compressed data will fit in its buffer, allocate
 *     an output buffer of size 1% larger than the uncompressed data, plus six
 *     hundred extra bytes.
 */
#define ASDF_COMPRESSOR_BZP2_BUF_CAPACITY(buf_size) ((buf_size) + ((buf_size) / 100) + 601)


static int asdf_compressor_bzp2_comp(
    const uint8_t *buf, size_t buf_size, uint8_t **out, size_t *out_size) {

    int ret;
    bz_stream stream;

    if (UNLIKELY(!buf || !out || !out_size))
        return -1;

    *out = NULL;
    *out_size = 0;

    memset(&stream, 0, sizeof(stream));

    /* Recommended block size 9 (max compression), verbosity 0, workFactor 30 */
    /* TODO: Allow additional compression options in config */
    ret = BZ2_bzCompressInit(
        &stream, ASDF_COMPRESSOR_BZP2_BLOCK_SIZE, 0, ASDF_COMPRESSOR_BZP2_WORK_FACTOR);
    if (ret != BZ_OK)
        return ret;

    /* Worst-case expansion per bzip2 docs */
    size_t capacity = ASDF_COMPRESSOR_BZP2_BUF_CAPACITY(buf_size);
    uint8_t *output = malloc(capacity);

    if (!output) {
        BZ2_bzCompressEnd(&stream);
        return -1;
    }

    stream.next_in = (char *)buf;
    stream.avail_in = buf_size;
    stream.next_out = (char *)output;
    stream.avail_out = capacity;

    ret = BZ2_bzCompress(&stream, BZ_FINISH);

    if (ret != BZ_STREAM_END) {
        free(output);
        BZ2_bzCompressEnd(&stream);
        return ret;
    }

    *out_size = capacity - stream.avail_out;
    *out = output;

    BZ2_bzCompressEnd(&stream);

    return 0;
}


static int asdf_compressor_bzp2_decomp(
    asdf_compressor_userdata_t *userdata,
    uint8_t *buf,
    size_t buf_size,
    size_t *offset_out,
    size_t offset_hint) {
    assert(userdata);
    asdf_compressor_bzp2_userdata_t *bzp2 = userdata;
    bzp2->info.status = ASDF_COMPRESSOR_IN_PROGRESS;
    bzp2->bz.next_out = (char *)buf;
    bzp2->bz.avail_out = buf_size;

    if (offset_hint < bzp2->progress)
        return 0;

    int ret = BZ2_bzDecompress(&bzp2->bz);
    if (ret != BZ_OK && ret != BZ_STREAM_END)
        return ret;

    if (offset_out)
        *offset_out = bzp2->progress;

    bzp2->progress += buf_size;

    if (ret == BZ_STREAM_END)
        bzp2->info.status = ASDF_COMPRESSOR_DONE;

    return 0;
}


ASDF_REGISTER_COMPRESSOR(
    bzp2,
    asdf_compressor_bzp2_init,
    asdf_compressor_bzp2_destroy,
    asdf_compressor_bzp2_info,
    asdf_compressor_bzp2_comp,
    asdf_compressor_bzp2_decomp);
