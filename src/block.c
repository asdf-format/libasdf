/**
 * ASDF block functions
 */

#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#if defined(HAVE_MD5) && defined(HAVE_MD5_H)
#include <md5.h>
#endif

#include "block.h"
#include "compat/endian.h" // IWYU pragma: keep
#include "compression/compression.h"
#include "compression/compressor_registry.h"
#include "core/asdf.h"
#include "core/software.h"
#include "error.h"
#include "file.h"
#include "log.h"
#include "stream.h"
#include "types/asdf_block_info_vec.h"
#include "util.h"


const unsigned char asdf_block_magic[] = {'\xd3', 'B', 'L', 'K'};

const char asdf_block_index_header[] = "#ASDF BLOCK INDEX";


void asdf_block_info_init(
    size_t index, const void *data, size_t size, asdf_block_info_t *out_block) {
    out_block->index = index;
    // Fill out the header for as much as we know (prior to compression)
    out_block->header = (asdf_block_header_t){
        .header_size = ASDF_BLOCK_HEADER_SIZE,
        .allocated_size = size,
        .used_size = size,
        .data_size = size};
    out_block->header_pos = -1;
    out_block->data_pos = -1;
    out_block->data = data;
    out_block->data_size = size;
}


void asdf_block_info_deinit(asdf_block_info_t *block_info) {
    if (!block_info)
        return;

    if (block_info->data_owned) {
        if (block_info->data_mmapped)
            munmap((void *)block_info->data, block_info->data_size ? block_info->data_size : 1);
        else
            free((void *)block_info->data);

        block_info->data = NULL;
        block_info->data_owned = false;
        block_info->data_mmapped = false;
    }

    block_info->data_size = 0;
}


/**
 * Parse a block header pointed to by the current stream position
 *
 * Assigns the block info into the allocated `asdf_block_info_t *` output,
 * and returns true if a block could be read successfully.
 */
bool asdf_block_info_read(asdf_stream_t *stream, asdf_block_info_t *out_block) {
    off_t header_pos = asdf_stream_tell(stream);
    size_t avail = 0;
    const uint8_t *buf = NULL;

    // TODO: ASDF 2.0.0 proposes adding a checksum to the block header
    // Here we will want to check that as well.
    // In fact we should probably ignore anything that starts with a block
    // magic but then contains garbage.  But we will need some heuristics
    // for what counts as "garbage"
    // Go ahead and allocate storage for the block info
    asdf_stream_consume(stream, ASDF_BLOCK_MAGIC_SIZE);

    if (UNLIKELY(ASDF_ERROR_GET(stream) != NULL))
        return false;

    buf = asdf_stream_next(stream, FIELD_SIZEOF(asdf_block_header_t, header_size), &avail);

    if (!buf) {
        ASDF_ERROR_COMMON(stream, ASDF_ERR_INVALID_BLOCK_HEADER);
        return false;
    }

    if (avail < 2) {
        ASDF_ERROR_COMMON(stream, ASDF_ERR_UNEXPECTED_EOF);
        return false;
    }

    asdf_block_header_t *header = &out_block->header;
    // NOLINTNEXTLINE(readability-magic-numbers)
    header->header_size = (buf[0] << 8) | buf[1];
    if (header->header_size < ASDF_BLOCK_HEADER_SIZE) {
        ASDF_ERROR_COMMON(stream, ASDF_ERR_INVALID_BLOCK_HEADER);
        return false;
    }

    asdf_stream_consume(stream, FIELD_SIZEOF(asdf_block_header_t, header_size));

    if (UNLIKELY(ASDF_ERROR_GET(stream) != NULL))
        return false;

    buf = asdf_stream_next(stream, header->header_size, &avail);

    if (avail < header->header_size) {
        ASDF_ERROR_COMMON(stream, ASDF_ERR_UNEXPECTED_EOF);
        return false;
    }

    // Parse block fields
    uint32_t flags =
        // NOLINTNEXTLINE(readability-magic-numbers)
        (((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16) | ((uint32_t)buf[2] << 8) | buf[3]);
    memcpy(
        header->compression,
        (char *)buf + ASDF_BLOCK_COMPRESSION_OFFSET,
        sizeof(header->compression));

    uint64_t allocated_size = 0;
    uint64_t used_size = 0;
    uint64_t data_size = 0;
    memcpy(&allocated_size, buf + ASDF_BLOCK_ALLOCATED_SIZE_OFFSET, sizeof(allocated_size));
    memcpy(&used_size, buf + ASDF_BLOCK_USED_SIZE_OFFSET, sizeof(used_size));
    memcpy(&data_size, buf + ASDF_BLOCK_DATA_SIZE_OFFSET, sizeof(data_size));

    header->flags = flags;
    header->allocated_size = be64toh(allocated_size);
    header->used_size = be64toh(used_size);
    header->data_size = be64toh(data_size);
    memcpy(header->checksum, buf + ASDF_BLOCK_CHECKSUM_OFFSET, sizeof(header->checksum));

    asdf_stream_consume(stream, header->header_size);

    if (UNLIKELY(ASDF_ERROR_GET(stream) != NULL))
        return false;

    out_block->header_pos = header_pos;
    out_block->data_pos = asdf_stream_tell(stream);
    return true;
}


#define WRITE_CHECK(stream, data, size) \
    do { \
        size_t n_written = asdf_stream_write((stream), (data), (size)); \
        if (n_written != (size)) { \
            ret = false; \
            goto cleanup; \
        } \
    } while (0)


bool asdf_block_info_write(
    asdf_file_t *file, asdf_block_info_t *block, asdf_stream_t *out, bool checksum) {
    assert(out);
    assert(out->is_writeable);
    assert(block);

    bool ret = true;
    uint8_t *comp_buf = NULL;  /* transient compressed output */
    asdf_block_t decomp = {0}; /* transient decompressor (recompress path) */
    bool decomp_open = false;
    void *input_map = NULL; /* input region opened for a file-backed block */
    asdf_stream_t *in_stream = (file && file->parser) ? file->parser->stream : NULL;
    const asdf_compressor_t *compressor = block->write_compressor;

    /*
     * Source the block's bytes.  A block with in-memory data uses it directly;
     * a block still backed by the input file is read straight from there,
     * possibly with decompression.
     */
    const void *src = NULL;
    size_t src_size = 0;
    bool src_is_compressed = false;

    if (block->data != NULL) {
        src = block->data;
        src_size = block->data_size;
        src_is_compressed = block->data_is_compressed;
    } else if (block->data_pos >= 0 && in_stream) {
        size_t avail = 0;
        input_map = in_stream->open_mem(
            in_stream, block->data_pos, (size_t)block->header.used_size, &avail);

        if (!input_map) {
            ASDF_ERROR_OOM(file);
            return false;
        }

        src = input_map;
        src_size = avail;
        src_is_compressed = (block->header.compression[0] != '\0');
    }

    /*
     * Decide the bytes actually written and the compression field:
     *  - a write compressor + uncompressed source: compress it;
     *  - a write compressor + already-compressed source: decompress then
     *    recompress (transient buffers only);
     *  - otherwise: write the source verbatim (preserving its compression).
     */
    const void *write_data = NULL;
    size_t write_size = 0;
    const char *comp_name = NULL; /* NULL => uncompressed (empty field) */

    if (compressor != NULL && src != NULL && !src_is_compressed) {
        if (compressor->comp(src, src_size, &comp_buf, &write_size) != 0) {
            ret = false;
            goto cleanup;
        }
        write_data = comp_buf;
        comp_name = compressor->compression;
    } else if (compressor != NULL && src != NULL && src_is_compressed) {
        decomp.file = file;
        decomp.info = *block;
        decomp.data = (void *)src;
        decomp.avail_size = src_size;
        decomp.should_close = false;

        if (asdf_block_comp_open(&decomp) != 0) {
            ASDF_ERROR_COMMON(
                file, ASDF_ERR_COMPRESSION_FAILED, "failed to decompress block for recompression");
            ret = false;
            goto cleanup;
        }

        decomp_open = true;

        if (compressor->comp(
                decomp.comp_state->dest, decomp.comp_state->dest_size, &comp_buf, &write_size) !=
            0) {
            ret = false;
            goto cleanup;
        }

        write_data = comp_buf;
        comp_name = compressor->compression;
    } else {
        write_data = src;
        write_size = src_size;
        comp_name = src_is_compressed ? block->header.compression : NULL;
    }

    block->header_pos = asdf_stream_tell(out);
    WRITE_CHECK(out, asdf_block_magic, ASDF_BLOCK_MAGIC_SIZE);

    uint16_t header_size = htobe16(ASDF_BLOCK_HEADER_SIZE);
    WRITE_CHECK(out, &header_size, sizeof(uint16_t));

    uint32_t flags = htobe32(block->header.flags);
    WRITE_CHECK(out, &flags, sizeof(uint32_t));

    char comp_field[ASDF_BLOCK_COMPRESSION_FIELD_SIZE] = {0};
    if (comp_name)
        memcpy(comp_field, comp_name, ASDF_BLOCK_COMPRESSION_FIELD_SIZE);
    WRITE_CHECK(out, comp_field, ASDF_BLOCK_COMPRESSION_FIELD_SIZE);

    /* used_size is the stored (post-compression) size; allocated_size may be
     * larger to reserve room for the block to grow in place. 0 (or any value
     * smaller than used_size) means "same as used_size". */
    uint64_t used = write_size;
    uint64_t allocated = block->header.allocated_size;
    if (allocated < used)
        allocated = used;

    uint64_t allocated_be = htobe64(allocated);
    WRITE_CHECK(out, &allocated_be, sizeof(uint64_t));
    uint64_t used_be = htobe64(used);
    WRITE_CHECK(out, &used_be, sizeof(uint64_t));
    /* data_size--always the uncompressed size */
    uint64_t data_size_be = htobe64(block->header.data_size);
    WRITE_CHECK(out, &data_size_be, sizeof(uint64_t));

#ifdef HAVE_MD5
    if (checksum) {
        asdf_md5_ctx_t md5_ctx = {0};
        asdf_md5_init(&md5_ctx);
        asdf_md5_update(&md5_ctx, write_data, write_size);
        asdf_md5_final(&md5_ctx, (unsigned char *)&block->header.checksum);
    } else {
        ASDF_LOG(out, ASDF_LOG_DEBUG, "block checksum calculation disabled by emitter flags");
    }
#else
    (void)checksum;
    ASDF_LOG(
        out,
        ASDF_LOG_WARN,
        PACKAGE_NAME " was compiled without MD5 support; block "
                     "checksum will not be written");
#endif
    WRITE_CHECK(out, block->header.checksum, ASDF_BLOCK_CHECKSUM_FIELD_SIZE);

    block->data_pos = asdf_stream_tell(out);

    if (write_size > 0)
        WRITE_CHECK(out, write_data, write_size);

    /* Pad with zeros up to allocated_size when the block reserves extra space */
    if (allocated > used) {
        static const uint8_t zeros[512] = {0};
        uint64_t remaining = allocated - used;

        while (remaining > 0) {
            size_t chunk = remaining > sizeof(zeros) ? sizeof(zeros) : (size_t)remaining;
            WRITE_CHECK(out, zeros, chunk);
            remaining -= chunk;
        }
    }

cleanup:
    /* comp_buf is transient; block->data (if owned) persists and is freed on
     * file teardown via asdf_block_info_deinit */
    free(comp_buf);
    if (decomp_open)
        asdf_block_comp_close(&decomp);
    /* TODO: Slightly awkward; see about refactoring the block compression
     * interfaces so it's not necessary to manage this transient block */
    free((void *)decomp.compression);
    if (input_map && in_stream)
        in_stream->close_mem(in_stream, input_map);
    return ret;
}


int asdf_block_info_compression_set(
    asdf_file_t *file, asdf_block_info_t *block_info, const char *compression) {
    if (UNLIKELY(!file || !block_info))
        return -1;

    const asdf_compressor_t *comp = asdf_compressor_get(file, compression);

    if (!comp) {
        ASDF_ERROR_COMMON(file, ASDF_ERR_UNKNOWN_COMPRESSION, compression);
        return -1;
    }

    block_info->write_compressor = comp;
    return 0;
}


#ifdef HAVE_MD5
#ifdef HAVE_MD5_H
/** libmd md5.h implementation (only one currently available) */
void asdf_md5_init(asdf_md5_ctx_t *ctx) {
    MD5Init(&ctx->ctx);
}


void asdf_md5_update(asdf_md5_ctx_t *ctx, const void *data, size_t len) {
    MD5Update(&ctx->ctx, data, len);
}


#ifndef ASDF_MD5_FINAL_WORKAROUND
void asdf_md5_final(asdf_md5_ctx_t *ctx, unsigned char digest[16]) {
    MD5Final(digest, &ctx->ctx);
}
#else
/*
 * Open-coded MD5Final, used where configure found a competing MD5Final
 * outside libmd (see the check in configure.ac).  Not naming the symbol is
 * the only reliable way to avoid binding to the wrong one.
 *
 * This does exactly what libmd's MD5Final does: pad, write the four state
 * words out little-endian, then clear the context.  MD5Init, MD5Update and
 * MD5Pad are unaffected, so they are still used as-is.
 */
void asdf_md5_final(asdf_md5_ctx_t *ctx, unsigned char digest[16]) {
    MD5Pad(&ctx->ctx);

    for (size_t idx = 0; idx < 4; idx++) {
        uint32_t word = ctx->ctx.state[idx];
        digest[idx * 4] = (unsigned char)word;
        digest[idx * 4 + 1] = (unsigned char)(word >> 8);
        digest[idx * 4 + 2] = (unsigned char)(word >> 16);
        digest[idx * 4 + 3] = (unsigned char)(word >> 24);
    }

    ZERO_MEMORY(&ctx->ctx, sizeof(ctx->ctx));
}
#endif
#endif
#endif


/* User-facing block-related methods */
size_t asdf_block_count(asdf_file_t *file) {
    if (!file)
        return 0;

    /* Because blocks are the last things we expect to find in a file (modulo the optional block
     * index) we cannot return the block count accurately without parsing the full file.  Relying
     * on the block index alone for the count is also not guaranteed to be accurate since it is
     * only a hint (a hint that nonetheless allows the parser to complete much faster when
     * possible).  So here we ensure the file is parsed to completion then return the block count.
     */
    asdf_parser_t *parser = asdf_file_parser(file);

    if (parser && !parser->done) {
        while (!parser->done) {
            asdf_event_iterate(parser);
        }

        // Copy the parser's block info into the file's
        asdf_block_info_vec_copy(&file->blocks, parser->block.infos);
    }

    return (size_t)asdf_block_info_vec_size(&file->blocks);
}

asdf_block_t *asdf_block_open(asdf_file_t *file, size_t index) {
    if (!file)
        return NULL;

    size_t n_blocks = asdf_block_count(file);

    if (index >= n_blocks) {
        ASDF_LOG(
            file,
            ASDF_LOG_WARN,
            "block index %zu does not exist (the file contains %zu blocks)",
            index,
            n_blocks);
        return NULL;
    }

    asdf_block_t *block = calloc(1, sizeof(asdf_block_t));

    if (!block) {
        ASDF_ERROR_OOM(file);
        return NULL;
    }

    asdf_block_info_vec_t *blocks = &file->blocks;
    const asdf_block_info_t *info = asdf_block_info_vec_at(blocks, (isize)index);
    block->file = file;
    block->data = NULL;
    block->should_close = false;
    block->info = *info;
    /* A view never owns the file's block data; ownership stays with file->blocks */
    block->info.data_owned = false;
    block->detached = false;
    block->comp_state = NULL;
    return block;
}


asdf_block_t *asdf_block_create(asdf_file_t *file, const void *data, size_t size) {
    if (!file)
        return NULL;

    if (file->mode == ASDF_FILE_MODE_READ_ONLY) {
        ASDF_ERROR_COMMON(file, ASDF_ERR_STREAM_READ_ONLY);
        return NULL;
    }

    asdf_block_t *block = calloc(1, sizeof(asdf_block_t));

    if (!block) {
        ASDF_ERROR_OOM(file);
        return NULL;
    }

    block->file = file;
    block->detached = true;
    block->info.index = SIZE_MAX; /* not yet appended */
    block->info.header.header_size = ASDF_BLOCK_HEADER_SIZE;
    block->info.header_pos = -1;
    block->info.data_pos = -1;

    /* data != NULL: adopt (borrow) it.  data == NULL with a non-zero size:
     * allocate an owned buffer the caller can fill (retrieve it idempotently
     * with asdf_block_data_alloc). */
    if (data) {
        asdf_block_data_set(block, data, size);
    } else if (size > 0) {
        if (!asdf_block_data_alloc(block, size)) {
            asdf_block_destroy(block);
            return NULL;
        }
    }

    return block;
}


void asdf_block_destroy(asdf_block_t *block) {
    if (!block)
        return;

    /* A created-but-not-appended block owns its in-memory data; once appended,
     * ownership has transferred to the file and detached is cleared. */
    if (block->detached)
        asdf_block_info_deinit(&block->info);

    asdf_block_close(block);
}


void asdf_block_close(asdf_block_t *block) {
    if (!block)
        return;

    if (block->comp_state)
        asdf_block_comp_close(block);

    if (block->compression)
        free((void *)block->compression);

    // If the block has an open data handle, close it
    if (block->should_close && block->data) {
        asdf_stream_t *stream = block->file->parser->stream;
        stream->close_mem(stream, block->data);
    }

    ZERO_MEMORY(block, sizeof(asdf_block_t));
    free(block);
}


void *asdf_block_data_alloc(asdf_block_t *block, size_t size) {
    if (!block)
        return NULL;

    /* Idempotent: reuse an already-allocated owned uncompressed buffer of the
     * same size (e.g. one allocated by asdf_block_create(file, NULL, size)) */
    if (block->info.data && block->info.data_owned && block->info.data_mmapped &&
        !block->info.data_is_compressed && block->info.data_size == size)
        return (void *)block->info.data;

    /* Drop any previously-set data */
    asdf_block_info_deinit(&block->info);

    size_t mmap_len = size ? size : 1;
    void *buf = mmap(NULL, mmap_len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (buf == MAP_FAILED) {
        ASDF_ERROR_OOM(block->file);
        return NULL;
    }

    block->info.data = buf;
    block->info.data_size = size;
    block->info.data_owned = true;
    block->info.data_mmapped = true;
    block->info.data_is_compressed = false;
    block->info.header.data_size = size;
    block->info.header.used_size = size;
    return buf;
}


int asdf_block_data_set(asdf_block_t *block, const void *data, size_t size) {
    if (!block)
        return -1;

    /* Drop any previously-set owned data; the new data is borrowed and must
     * outlive the block (until the file is written). */
    asdf_block_info_deinit(&block->info);

    block->info.data = data;
    block->info.data_size = size;
    block->info.data_owned = false;
    block->info.data_is_compressed = false;
    block->info.header.data_size = size;
    block->info.header.used_size = size;
    return 0;
}


int asdf_block_data_set_compressed(
    asdf_block_t *block,
    const void *data,
    size_t size,
    uint64_t data_size,
    const char *compression) {
    if (!block)
        return -1;

    /* Drop any previously-set owned data; like asdf_block_data_set the
     * already-compressed bytes are borrowed and must outlive the block until
     * the file is written--they are emitted verbatim. */
    asdf_block_info_deinit(&block->info);

    block->info.data = data;
    block->info.data_size = size;
    block->info.data_owned = false;
    block->info.data_is_compressed = true;
    block->info.write_compressor = NULL;
    block->info.header.data_size = data_size; /* uncompressed size */
    block->info.header.used_size = size;      /* compressed size */

    memset(block->info.header.compression, 0, ASDF_BLOCK_COMPRESSION_FIELD_SIZE);
    if (compression && *compression)
        strncpy(block->info.header.compression, compression, ASDF_BLOCK_COMPRESSION_FIELD_SIZE);

    return 0;
}


int asdf_block_allocated_size_set(asdf_block_t *block, uint64_t allocated_size) {
    if (!block)
        return -1;

    block->info.header.allocated_size = allocated_size;

    /* Propagate to the file's block_info if already appended */
    if (!block->detached && block->info.index != SIZE_MAX) {
        asdf_block_info_t *file_block = asdf_block_info_vec_at_mut(
            &block->file->blocks, (isize)block->info.index);

        if (file_block)
            file_block->header.allocated_size = allocated_size;
    }

    return 0;
}


asdf_block_t *asdf_block_append(asdf_file_t *file, asdf_block_t *block) {
    if (UNLIKELY(!file || !block))
        return NULL;

    if (file->mode == ASDF_FILE_MODE_READ_ONLY) {
        ASDF_ERROR_COMMON(file, ASDF_ERR_STREAM_READ_ONLY);
        return NULL;
    }

    if (!block->detached) {
        ASDF_LOG(file, ASDF_LOG_ERROR, "asdf_block_append: block is already appended to a file");
        return NULL;
    }

    size_t n_blocks = asdf_block_count(file);

    if (n_blocks >= SSIZE_MAX) {
        ASDF_ERROR_COMMON(file, ASDF_ERR_OVER_LIMIT, "block count exceeds maximum");
        return NULL;
    }

    block->file = file;
    block->info.index = n_blocks;

    if (!asdf_block_info_vec_push(&file->blocks, block->info)) {
        ASDF_ERROR_OOM(file);
        return NULL;
    }

    /* Ownership of any owned data transfers to the file's block_info copy; the
     * handle is now a non-owning view onto the appended block. */
    block->info.data_owned = false;
    block->detached = false;
    return block;
}


size_t asdf_block_data_size(asdf_block_t *block) {
    return block->info.header.data_size;
}


const void *asdf_block_data_impl(asdf_block_t *block, size_t *size, bool decompress) {
    if (!block)
        return NULL;

    /* Cached data from a previous stream open */
    if (block->data && block->should_close) {
        if (size)
            *size = block->avail_size;

        return block->data;
    }

    /* In-memory block content: created/appended, or materialized by the emitter */
    if (block->info.data) {
        if (decompress && block->info.data_is_compressed) {
            if (!block->comp_state) {
                block->data = (void *)block->info.data;
                block->avail_size = block->info.data_size;
                block->should_close = false;

                if (asdf_block_comp_open(block) != 0) {
                    ASDF_LOG(block->file, ASDF_LOG_ERROR, "failed to open compressed block data");
                    return NULL;
                }
            }

            if (block->comp_state) {
                if (size)
                    *size = block->comp_state->dest_size;

                return block->comp_state->dest;
            }
        }

        if (size)
            *size = block->info.data_size;

        return block->info.data;
    }

    /* No in-memory data and no backing file region: the block has no data */
    asdf_parser_t *parser = block->file->parser;

    if (block->info.data_pos < 0 || !parser || !parser->stream) {
        if (size)
            *size = 0;

        return NULL;
    }

    asdf_stream_t *stream = parser->stream;
    size_t avail = 0;
    void *data = stream->open_mem(
        stream, block->info.data_pos, block->info.header.used_size, &avail);
    block->data = data;
    block->should_close = true;
    block->avail_size = avail;

    // Open compressed data if applicable
    if (decompress) {
        if (asdf_block_comp_open(block) != 0) {
            ASDF_LOG(block->file, ASDF_LOG_ERROR, "failed to open compressed block data");
            return NULL;
        }

        if (block->comp_state) {
            // Return the destination of the compressed data
            if (size)
                *size = block->comp_state->dest_size;

            return block->comp_state->dest;
        } // else was not compressed to begin with
    }

    if (size)
        *size = avail;

    // Just the raw data
    return block->data;
}


const void *asdf_block_data(asdf_block_t *block, size_t *size) {
    return asdf_block_data_impl(block, size, true);
}


const void *asdf_block_data_raw(asdf_block_t *block, size_t *size) {
    return asdf_block_data_impl(block, size, false);
}


const char *asdf_block_compression_orig(asdf_block_t *block) {
    if (!block)
        return "";

    if (!block->compression)
        block->compression = strndup(
            block->info.header.compression, ASDF_BLOCK_COMPRESSION_FIELD_SIZE);

    if (!block->compression) {
        ASDF_ERROR_OOM(block->file);
        return "";
    }

    return block->compression;
}


const char *asdf_block_compression(asdf_block_t *block) {
    if (!block)
        return "";

    // If the user set an output compression different from the original input
    // compression
    if (block->info.write_compressor)
        return block->info.write_compressor->compression;

    return asdf_block_compression_orig(block);
}


int asdf_block_compression_set(asdf_block_t *block, const char *compression) {
    if (!block)
        return -1;

    int ret = asdf_block_info_compression_set(block->file, &block->info, compression);

    if (ret != 0)
        return ret;

    /* Propagate to file->blocks so the emitter sees the change (a detached
     * block carries write_compressor in its own info until appended) */
    if (!block->detached && block->info.index != SIZE_MAX) {
        asdf_block_info_t *file_block = asdf_block_info_vec_at_mut(
            &block->file->blocks, (isize)block->info.index);

        if (file_block)
            file_block->write_compressor = block->info.write_compressor;
    }

    return 0;
}


const unsigned char *asdf_block_checksum(asdf_block_t *block) {
    if (!block)
        return NULL;

    const asdf_block_header_t *header = &block->info.header;
    return header->checksum;
}


#define ASDF_PYTHON_CHECKSUM_BUG_MAJOR_VERSION 5


static inline bool asdf_library_has_checksum_bug(asdf_software_t *software) {
    return (
        strcmp(software->name, "asdf") == 0 &&
        software->version->major <= ASDF_PYTHON_CHECKSUM_BUG_MAJOR_VERSION);
}


bool asdf_block_checksum_verify(
    asdf_block_t *block, unsigned char computed[ASDF_BLOCK_CHECKSUM_DIGEST_SIZE]) {
    if (!block)
        return false;

#ifndef HAVE_MD5
    (void)block;
    (void)computed;
    return true;
#else
    const asdf_block_header_t *header = &block->info.header;
    size_t size = 0;
    asdf_md5_ctx_t md5_ctx = {0};
    unsigned char digest[ASDF_BLOCK_CHECKSUM_DIGEST_SIZE] = {0};
    const void *data = NULL;
    asdf_meta_t *meta = NULL;
    asdf_file_t *file = block->file;

    /* Python asdf has a bug that when it writes binary blocks it computes
     * the checksum based on the uncompressed data, not the compressed data.
     * In this case then we must use the decompresed data to compute the
     * checksum.  This is slated to be fixed in a later version; for now
     * the problem exists in all versions at least 5.x and below.
     * See https://github.com/asdf-format/asdf/issues/2015 */
    const char *comp = asdf_block_compression_orig(block);
    if (comp && *comp != '\0') {
        asdf_value_err_t err = asdf_get_meta(file, "", &meta);

        if (ASDF_VALUE_OK == err && meta && asdf_library_has_checksum_bug(meta->asdf_library)) {
            ASDF_LOG(
                file,
                ASDF_LOG_WARN,
                "%s version %s has compressed data checksum bug; "
                "the checksum will be verified against the uncompressed data",
                meta->asdf_library->name,
                meta->asdf_library->version->version);
            data = asdf_block_data(block, &size);
            asdf_meta_destroy(meta);
        } else {
            data = asdf_block_data_raw(block, &size);
        }
    } else {
        data = asdf_block_data_raw(block, &size);
    }

    if (!data)
        return false;

    asdf_md5_init(&md5_ctx);
    asdf_md5_update(&md5_ctx, data, size);
    asdf_md5_final(&md5_ctx, digest);
    bool valid = memcmp(header->checksum, digest, ASDF_BLOCK_CHECKSUM_DIGEST_SIZE) == 0;

    if (computed)
        memcpy(computed, digest, ASDF_BLOCK_CHECKSUM_DIGEST_SIZE);

    return valid;
#endif
}
