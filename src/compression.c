/**
 * Internal utilities specifically for handling compressed blocks
 */
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <bzlib.h>
#include <zlib.h>

#include "asdf/file.h"
#include "config.h"

#include "block.h"
#include "compression.h"
#include "error.h"
#include "file.h"
#include "log.h"
#include "util.h"


void asdf_block_comp_close(asdf_block_t *block) {
    assert(block);
    asdf_block_comp_state_t *cs = block->comp_state;

    if (!cs)
        return;

    switch (cs->comp) {
    case ASDF_BLOCK_COMP_UNKNOWN:
    case ASDF_BLOCK_COMP_NONE:
        /* Nothing to do */
        break;
    case ASDF_BLOCK_COMP_ZLIB:
        if (cs->z) {
            inflateEnd(cs->z);
            free(cs->z);
        }
        break;
    case ASDF_BLOCK_COMP_BZP2:
        if (cs->bz) {
            BZ2_bzDecompressEnd(cs->bz);
            free(cs->bz);
        }
        break;
    }

    if (cs->dest)
        munmap(cs->dest, cs->dest_size);

    if (cs->own_fd)
        close(cs->fd);

    ZERO_MEMORY(cs, sizeof(asdf_block_comp_state_t));
    free(cs);
}


asdf_block_comp_t asdf_block_comp_parse(asdf_file_t *file, const char *compression) {
    assert(compression);

    if (strncmp(compression, "zlib", ASDF_BLOCK_COMPRESSION_FIELD_SIZE) == 0)
        return ASDF_BLOCK_COMP_ZLIB;

    if (strncmp(compression, "bzp2", ASDF_BLOCK_COMPRESSION_FIELD_SIZE) == 0)
        return ASDF_BLOCK_COMP_BZP2;

    if (compression[0] == '\0')
        return ASDF_BLOCK_COMP_NONE;

    ASDF_LOG(
        file,
        ASDF_LOG_WARN,
        "unsupported block compression option %s; block data will simply be copied verbatim",
        compression);
    return ASDF_BLOCK_COMP_UNKNOWN;
}


static int asdf_create_temp_file(size_t data_size, const char *tmp_dir, int *out_fd) {
    char path[PATH_MAX];
    int fd;

    if (!tmp_dir) {
        const char *tmp = getenv("ASDF_TMPDIR");
        tmp = (tmp && tmp[0]) ? tmp : getenv("TMPDIR");
        tmp_dir = (tmp && tmp[0]) ? tmp : "/tmp";
    }

    snprintf(path, sizeof(path), "%s/libasdf-block-XXXXXX", tmp_dir);

    fd = mkstemp(path);

    if (fd < 0)
        return -1;

    // unlink so it deletes on close
    unlink(path);

    if (ftruncate(fd, data_size) != 0) {
        close(fd);
        return -1;
    }

    *out_fd = fd;
    return 0;
}


static int asdf_block_decomp_until(asdf_block_comp_state_t *cs, size_t offset) {
    if (offset <= cs->produced)
        return 0; // already decompressed enough

    size_t need = offset - cs->produced;
    size_t new_produced = cs->dest_size;

    if (cs->produced + need > cs->dest_size)
        need = cs->dest_size - cs->produced;

    // Update the destination
    uint8_t *new_dest = cs->dest + cs->produced;

    // TODO: Probably offload this as well as per-compression-type
    // initialization to a separate, extensible interface
    switch (cs->comp) {
    case ASDF_BLOCK_COMP_UNKNOWN:
    case ASDF_BLOCK_COMP_NONE:
        return -1;
    case ASDF_BLOCK_COMP_ZLIB: {
        cs->z->next_out = new_dest;
        cs->z->avail_out = need;

        int ret = inflate(cs->z, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END)
            return ret;

        new_produced -= cs->z->avail_out;
        break;
    }
    case ASDF_BLOCK_COMP_BZP2: {
        cs->bz->next_out = (char *)new_dest;
        cs->bz->avail_out = need;

        int ret = BZ2_bzDecompress(cs->bz);
        if (ret != BZ_OK && ret != BZ_STREAM_END)
            return ret;

        new_produced -= cs->bz->avail_out;
        break;
    }
    }

    cs->produced = new_produced;
    return 0;
}


static int asdf_block_decomp_eager(asdf_block_comp_state_t *cs) {
    return asdf_block_decomp_until(cs, cs->dest_size);
}


#define ASDF_ZLIB_FORMAT 15
#define ASDF_ZLIB_AUTODETECT 32


/**
 * Opens a memory handle to contain decompressed block data
 *
 * TODO: Improve error handling here
 */
int asdf_block_comp_open(asdf_block_t *block) {
    assert(block);

    int ret = -1;

    if (block->comp == ASDF_BLOCK_COMP_UNKNOWN || block->comp == ASDF_BLOCK_COMP_NONE) {
        // Actually nothing to do, just return 0
        return 0;
    }

    asdf_block_comp_state_t *state = calloc(1, sizeof(asdf_block_comp_state_t));

    if (!state) {
        ASDF_ERROR_OOM(block->file);
        goto failure;
    }

    asdf_config_t *config = block->file->config;
    asdf_block_header_t *header = &block->info.header;
    // Decide whether to use temp file or anonymous mmap
    size_t max_memory_bytes = config->decomp.max_memory_bytes;
    double max_memory_threshold = config->decomp.max_memory_threshold;

    size_t max_memory = SIZE_MAX;

    if (max_memory_threshold > 0.0) {
        size_t total_memory = asdf_util_get_total_memory();

        if (total_memory > 0)
            max_memory = (size_t)(total_memory * max_memory_threshold);
    }

    if (max_memory_bytes > 0)
        max_memory = (max_memory_bytes < max_memory) ? max_memory_bytes : max_memory;

    size_t dest_size = header->data_size;

    bool use_file_backing = dest_size > max_memory;

    if (use_file_backing) {
        ASDF_LOG(
            block->file,
            ASDF_LOG_INFO,
            "compressed data in block %d is %d bytes, exceeding the memory threshold %d; "
            "using temp file",
            block->info.index,
            dest_size,
            max_memory);

        if (asdf_create_temp_file(dest_size, config->decomp.tmp_dir, &state->fd) != 0) {
            goto failure;
        }
        // Read-only for now
        state->dest = mmap(NULL, dest_size, PROT_READ | PROT_WRITE, MAP_SHARED, state->fd, 0);
        if (state->dest == MAP_FAILED) {
            close(state->fd);
            goto failure;
        } else {
            state->own_fd = true;
        }
    } else {
        // anonymous mmap
        // Read-only for now
        state->dest = mmap(
            NULL, dest_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (state->dest == MAP_FAILED) {
            goto failure;
        }

        state->fd = -1;
        state->own_fd = false;
    }

    state->comp = block->comp;
    state->dest_size = dest_size;

    // Initialize compression lib-specific structures
    switch (state->comp) {
    case ASDF_BLOCK_COMP_UNKNOWN:
    case ASDF_BLOCK_COMP_NONE:
        /* shouldn't even be here */
        UNREACHABLE();
    case ASDF_BLOCK_COMP_ZLIB: {
        z_stream *z = calloc(1, sizeof(z_stream));
        if (!z) {
            ASDF_ERROR_OOM(block->file);
            goto failure;
        }
        z->next_in = (Bytef *)block->data;
        z->avail_in = block->avail_size;
        z->next_out = (Bytef *)state->dest;
        z->avail_out = state->dest_size;
        state->z = z;

        ret = inflateInit2(z, ASDF_ZLIB_FORMAT + ASDF_ZLIB_AUTODETECT);

        if (ret != Z_OK)
            goto failure;

        break;
    }
    case ASDF_BLOCK_COMP_BZP2: {
        bz_stream *bz = calloc(1, sizeof(bz_stream));

        if (!bz) {
            ASDF_ERROR_OOM(block->file);
            goto failure;
        }

        bz->next_in = (char *)block->data;
        bz->avail_in = block->avail_size;
        bz->next_out = (char *)state->dest;
        bz->avail_out = state->dest_size;
        state->bz = bz;

        ret = BZ2_bzDecompressInit(bz, 0, 0);
        if (ret != BZ_OK)
            goto failure;

        break;
    }
    }

    // Eager decompress
    if (asdf_block_decomp_eager(state) != 0) {
        asdf_block_comp_close(block);
        return -1;
    }

    // After decompression set PROT_READ for now (later this should depend on the mode flag the
    // file was opened with)
    mprotect(state->dest, dest_size, PROT_READ);
    block->comp_state = state;
    return 0;
failure:
    asdf_block_comp_close(block);
    return ret;
}
