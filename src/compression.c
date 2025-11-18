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

#include "block.h"
#include "compression.h"
#include "context.h"
#include "file.h"
#include "log.h"


asdf_block_comp_t asdf_block_comp_parse(asdf_context_t *ctx, const char *compression) {
    assert(compression);

    if (strncmp(compression, "zlib", ASDF_BLOCK_COMPRESSION_FIELD_SIZE) == 0)
        return ASDF_BLOCK_COMP_ZLIB;
    else if (strncmp(compression, "bzp2", ASDF_BLOCK_COMPRESSION_FIELD_SIZE) == 0)
        return ASDF_BLOCK_COMP_BZP2;
    else if (compression[0] == '\0')
        return ASDF_BLOCK_COMP_NONE;

    ASDF_LOG_CTX(
        ctx,
        ASDF_LOG_WARN,
        "unsupported block compression option %s; block data will simply be copied verbatim",
        compression);
    return ASDF_BLOCK_COMP_UNKNOWN;
}


static int asdf_create_temp_file(size_t data_size, const char *tmp_dir, int *out_fd) {
    char path[PATH_MAX];
    int fd;

    if (!tmp_dir)
        tmp_dir = "/tmp";

    snprintf(path, sizeof(path), "%s/asdf_block_XXXXXX", tmp_dir);

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


#define ASDF_ZLIB_FORMAT 15
#define ASDF_ZLIB_AUTODETECT 32


// TODO: Stupid test implementation, should be refactored
static int asdf_block_decomp_eager(asdf_block_t *block) {
    assert(block);

    int ret = 0;

    asdf_block_header_t *header = &block->info.header;

    switch (block->comp) {
    case ASDF_BLOCK_COMP_ZLIB: {
        z_stream strm = {0};
        strm.next_in = (Bytef *)block->raw_data;
        strm.avail_in = block->avail_size;
        strm.next_out = (Bytef *)block->data;
        strm.avail_out = header->data_size;

        ret = inflateInit2(&strm, ASDF_ZLIB_FORMAT + ASDF_ZLIB_AUTODETECT);

        if (ret != Z_OK)
            return ret;

        ret = inflate(&strm, Z_FINISH);
        inflateEnd(&strm);
        if (ret != Z_STREAM_END)
            return ret;
        return 0;
    }
    case ASDF_BLOCK_COMP_BZP2: {
        bz_stream strm = {0};
        strm.next_in = (char *)block->raw_data;
        strm.avail_in = block->avail_size;
        strm.next_out = (char *)block->data;
        strm.avail_out = header->data_size;

        ret = BZ2_bzDecompressInit(&strm, 0, 0);
        if (ret != BZ_OK)
            return ret;

        ret = BZ2_bzDecompress(&strm);
        BZ2_bzDecompressEnd(&strm);

        if (ret != BZ_STREAM_END)
            return ret;

        return 0;
    }
    default:
        break;
    }

    return -1;
}


/**
 * Opens a memory handle to contain decompressed block data
 *
 * TODO: Improve error handling here
 */
int asdf_block_open_compressed(asdf_block_t *block, size_t *size) {
    assert(block);

    asdf_config_t *config = block->file->config;
    asdf_block_header_t *header = &block->info.header;
    // Decide whether to use temp file or anonymous mmap
    size_t mem_threshold = config->decomp.max_memory_bytes;
    size_t data_size = header->data_size;

    if (mem_threshold == 0)
        mem_threshold = SIZE_MAX;

    bool use_file_backing = data_size > mem_threshold;

    if (use_file_backing) {
        if (asdf_create_temp_file(data_size, config->decomp.tmp_dir, &block->comp_fd) != 0) {
            return -1;
        }
        block->comp_own_fd = true;
        // Read-only for now
        block->data = mmap(NULL, data_size, PROT_READ | PROT_WRITE, MAP_SHARED, block->comp_fd, 0);
        if (block->data == MAP_FAILED) {
            close(block->comp_fd);
            return -1;
        }
    } else {
        // anonymous mmap
        // Read-only for now
        block->data = mmap(
            NULL, data_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (block->data == MAP_FAILED) {
            return -1;
        }

        block->comp_fd = -1;
        block->comp_own_fd = false;
    }

    // Eager decompress
    if (asdf_block_decomp_eager(block) != 0) {
        munmap(block->data, data_size);

        if (block->comp_own_fd)
            close(block->comp_own_fd);

        return -1;
    }

    // After decompression set PROT_READ for now (later this should depend on the mode flag the
    // file was opened with)
    mprotect(block->data, data_size, PROT_READ);

    if (size)
        *size = data_size;

    return 0;
}
