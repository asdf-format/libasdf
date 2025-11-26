#include <assert.h>
#include <stdbool.h>

#include <bzlib.h>


#include "../error.h"
#include "../file.h"
#include "../log.h"

#include "compression.h"
#include "compressor_registry.h"


typedef struct {
    asdf_compressor_status_t status;
    bz_stream bz;
} asdf_compressor_bzp2_userdata_t;


static asdf_compressor_userdata_t *asdf_compressor_bzp2_init(
    asdf_block_t *block, const void *dest, size_t dest_size) {
    asdf_compressor_bzp2_userdata_t *userdata = NULL;

    userdata = calloc(1, sizeof(asdf_compressor_bzp2_userdata_t));

    if (!userdata) {
        ASDF_ERROR_OOM(block->file);
        return NULL;
    }

    bz_stream *bz = &userdata->bz;
    bz->next_in = (char *)block->data;
    bz->avail_in = block->avail_size;
    bz->next_out = (char *)dest;
    bz->avail_out = dest_size;

    int ret = BZ2_bzDecompressInit(bz, 0, 0);
    if (ret != BZ_OK) {
        ASDF_LOG(block->file, ASDF_LOG_ERROR, "error initializing bzip2 stream: %d", ret);
        return NULL;
    }

    userdata->status = ASDF_COMPRESSOR_INITIALIZED;
    return userdata;
}


static void asdf_compressor_bzp2_destroy(asdf_compressor_userdata_t *userdata) {
    assert(userdata);
    asdf_compressor_bzp2_userdata_t *bzp2 = userdata;

    if (bzp2->status != ASDF_COMPRESSOR_UNINITIALIZED)
        BZ2_bzDecompressEnd(&bzp2->bz);

    free(bzp2);
}


static asdf_compressor_status_t asdf_compressor_bzp2_status(asdf_compressor_userdata_t *userdata) {
    assert(userdata);
    asdf_compressor_bzp2_userdata_t *bzp2 = userdata;
    return bzp2->status;
}


static int asdf_compressor_bzp2_decomp(
    asdf_compressor_userdata_t *userdata, uint8_t *buf, size_t buf_size) {
    assert(userdata);
    asdf_compressor_bzp2_userdata_t *bzp2 = userdata;
    bzp2->status = ASDF_COMPRESSOR_IN_PROGRESS;
    bzp2->bz.next_out = (char *)buf;
    bzp2->bz.avail_out = buf_size;

    int ret = BZ2_bzDecompress(&bzp2->bz);
    if (ret != BZ_OK && ret != BZ_STREAM_END)
        return ret;

    if (ret == BZ_STREAM_END)
        bzp2->status = ASDF_COMPRESSOR_DONE;

    return 0;
}


ASDF_REGISTER_COMPRESSOR(
    bzp2,
    asdf_compressor_bzp2_init,
    asdf_compressor_bzp2_destroy,
    asdf_compressor_bzp2_status,
    asdf_compressor_bzp2_decomp);
