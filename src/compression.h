/** Internal compressed block utilities */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <bzlib.h>
#include <zlib.h>

#include <asdf/file.h>


/**
 * Whether lazy decompression is available
 *
 * Currently only works when built on new-enough Linux versions that have
 * userfaultfd support, though can provide other implementations later.
 */
#define ASDF_BLOCK_DECOMP_LAZY_AVAILABLE HAVE_USERFAULTFD


/**
 * Stores state and info for block decompression
 */
typedef struct {
    asdf_block_comp_t comp;
    int fd;
    bool own_fd;
    size_t produced;
    uint8_t *dest;
    size_t dest_size;
    /**
     * Compression-lib-specific data, effectively something like
     *
     * z_stream and bz_stream are included since these are built in, but we
     * leave open the possibility for others in a void*
     */
    union {
        z_stream *z;
        bz_stream *bz;
        void *uz;
    };
} asdf_block_comp_state_t;


// Forward-declaration
typedef struct asdf_block asdf_block_t;


ASDF_LOCAL asdf_block_comp_t asdf_block_comp_parse(asdf_file_t *file, const char *compression);
ASDF_LOCAL int asdf_block_comp_open(asdf_block_t *block);
ASDF_LOCAL void asdf_block_comp_close(asdf_block_t *block);
