#include <assert.h>
#include <stdbool.h>

#include <zlib.h>

#include "../error.h"
#include "../file.h"
#include "../log.h"

#include "compression.h"
#include "compressor_registry.h"


typedef struct {
    asdf_compressor_info_t info;
    z_stream z;
    size_t progress;
} asdf_compressor_zlib_userdata_t;


#define ASDF_ZLIB_FORMAT 15
#define ASDF_ZLIB_AUTODETECT 32


static asdf_compressor_userdata_t *asdf_compressor_zlib_init(
    const asdf_block_t *block, const void *dest, size_t dest_size) {
    asdf_compressor_zlib_userdata_t *userdata = NULL;

    userdata = calloc(1, sizeof(asdf_compressor_zlib_userdata_t));

    if (!userdata) {
        ASDF_ERROR_OOM(block->file);
        return NULL;
    }

    z_stream *stream = &userdata->z;
    stream->next_in = (Bytef *)block->data;
    stream->avail_in = block->avail_size;
    stream->next_out = (Bytef *)dest;
    stream->avail_out = dest_size;

    int ret = inflateInit2(stream, ASDF_ZLIB_FORMAT + ASDF_ZLIB_AUTODETECT);

    if (ret != Z_OK) {
        ASDF_LOG(block->file, ASDF_LOG_ERROR, "error initializing zlib stream: %d", ret);
        return NULL;
    }

    userdata->info.status = ASDF_COMPRESSOR_INITIALIZED;
    userdata->info.optimal_chunk_size = 0;
    userdata->progress = 0;
    return userdata;
}


static void asdf_compressor_zlib_destroy(asdf_compressor_userdata_t *userdata) {
    asdf_compressor_zlib_userdata_t *zlib = userdata;

    if (!zlib)
        return;

    if (zlib->info.status != ASDF_COMPRESSOR_UNINITIALIZED)
        inflateEnd(&zlib->z);

    free(zlib);
}


static const asdf_compressor_info_t *asdf_compressor_zlib_info(
    asdf_compressor_userdata_t *userdata) {
    assert(userdata);
    asdf_compressor_zlib_userdata_t *zlib = userdata;
    return &zlib->info;
}


static int asdf_compressor_zlib_comp(
    const uint8_t *buf, size_t buf_size, uint8_t **out, size_t *out_size) {

    if (UNLIKELY(!buf || !out || !out_size))
        return -1;

    *out = NULL;
    *out_size = 0;

    /* zlib uses uLong (typically 32-bit or 64-bit depending on build) */
    if (buf_size > (size_t)ULONG_MAX)
        return -1;

    uLong src_len = (uLong)buf_size;
    uLong bound = compressBound(src_len);

    uint8_t *output = malloc(bound);

    if (!output)
        return -1;

    uLong dest_len = bound;

    /* Z_BEST_COMPRESSION or make this configurable later */
    int ret = compress2(output, &dest_len, buf, src_len, Z_BEST_COMPRESSION);

    if (ret != Z_OK) {
        free(output);
        return ret;
    }

    *out = output;
    *out_size = (size_t)dest_len;

    return 0;
}


static int asdf_compressor_zlib_decomp(
    asdf_compressor_userdata_t *userdata,
    uint8_t *buf,
    size_t buf_size,
    size_t *offset_out,
    size_t offset_hint) {
    assert(userdata);
    asdf_compressor_zlib_userdata_t *zlib = userdata;
    zlib->info.status = ASDF_COMPRESSOR_IN_PROGRESS;

    if (offset_hint < zlib->progress)
        return 0;

    zlib->z.next_out = buf;
    zlib->z.avail_out = buf_size;

    int ret = inflate(&zlib->z, Z_NO_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END)
        return ret;

    if (offset_out)
        *offset_out = zlib->progress;

    zlib->progress += buf_size;

    if (ret == Z_STREAM_END)
        zlib->info.status = ASDF_COMPRESSOR_DONE;

    return 0;
}


ASDF_REGISTER_COMPRESSOR(
    zlib,
    asdf_compressor_zlib_init,
    asdf_compressor_zlib_destroy,
    asdf_compressor_zlib_info,
    asdf_compressor_zlib_comp,
    asdf_compressor_zlib_decomp);
