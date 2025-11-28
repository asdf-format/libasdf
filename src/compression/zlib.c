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
} asdf_compressor_zlib_userdata_t;


#define ASDF_ZLIB_FORMAT 15
#define ASDF_ZLIB_AUTODETECT 32


static asdf_compressor_userdata_t *asdf_compressor_zlib_init(
    asdf_block_t *block, const void *dest, size_t dest_size) {
    asdf_compressor_zlib_userdata_t *userdata = NULL;

    userdata = calloc(1, sizeof(asdf_compressor_zlib_userdata_t));

    if (!userdata) {
        ASDF_ERROR_OOM(block->file);
        return NULL;
    }

    z_stream *z = &userdata->z;
    z->next_in = (Bytef *)block->data;
    z->avail_in = block->avail_size;
    z->next_out = (Bytef *)dest;
    z->avail_out = dest_size;

    int ret = inflateInit2(z, ASDF_ZLIB_FORMAT + ASDF_ZLIB_AUTODETECT);

    if (ret != Z_OK) {
        ASDF_LOG(block->file, ASDF_LOG_ERROR, "error initializing zlib stream: %d", ret);
        return NULL;
    }

    userdata->info.status = ASDF_COMPRESSOR_INITIALIZED;
    userdata->info.optimal_chunk_size = 0;
    return userdata;
}


static void asdf_compressor_zlib_destroy(asdf_compressor_userdata_t *userdata) {
    assert(userdata);
    asdf_compressor_zlib_userdata_t *zlib = userdata;

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


static int asdf_compressor_zlib_decomp(
    asdf_compressor_userdata_t *userdata, uint8_t *buf, size_t buf_size) {
    assert(userdata);
    asdf_compressor_zlib_userdata_t *zlib = userdata;
    zlib->info.status = ASDF_COMPRESSOR_IN_PROGRESS;
    zlib->z.next_out = buf;
    zlib->z.avail_out = buf_size;

    int ret = inflate(&zlib->z, Z_NO_FLUSH);
    if (ret != Z_OK && ret != Z_STREAM_END)
        return ret;

    if (ret == Z_STREAM_END)
        zlib->info.status = ASDF_COMPRESSOR_DONE;

    return 0;
}


ASDF_REGISTER_COMPRESSOR(
    zlib,
    asdf_compressor_zlib_init,
    asdf_compressor_zlib_destroy,
    asdf_compressor_zlib_info,
    asdf_compressor_zlib_decomp);
