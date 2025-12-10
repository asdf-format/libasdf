#include <assert.h>
#include <stdbool.h>

#include "block.h"
#include "context.h"
#include "emitter.h"
#include "error.h"
#include "file.h"
#include "parse_util.h"
#include "stream.h"
#include "types/asdf_block_info_vec.h"

/**
 * Default libasdf emitter configuration
 */
static const asdf_emitter_cfg_t default_asdf_emitter_cfg = {};


asdf_emitter_t *asdf_emitter_create(asdf_file_t *file, asdf_emitter_cfg_t *config) {
    asdf_emitter_t *emitter = calloc(1, sizeof(asdf_emitter_t));

    if (!emitter)
        return emitter;

    emitter->base.ctx = file->base.ctx;
    asdf_context_retain(emitter->base.ctx);
    emitter->file = file;
    emitter->config = config ? *config : default_asdf_emitter_cfg;
    emitter->state = ASDF_EMITTER_STATE_INITIAL;
    emitter->done = false;
    return emitter;
}


/** Helper to determine if there is any content to write to the file */
static bool asdf_emitter_should_emit(asdf_emitter_t *emitter) {
    // TODO: Check if has tree to write
    bool should_emit = false;
    if (asdf_block_info_vec_size(&emitter->file->blocks) > 0)
        should_emit = true;

    if (asdf_emitter_has_opt(emitter, ASDF_EMITTER_OPT_EMIT_EMPTY))
        should_emit |= true;

    return should_emit;
}


int asdf_emitter_set_output_file(asdf_emitter_t *emitter, const char *filename) {
    assert(emitter);
    emitter->stream = asdf_stream_from_file(emitter->base.ctx, filename, true);

    if (!emitter->stream) {
        // TODO: Better error handling for file opening errors
        // For now just use this generic error
        ASDF_ERROR_COMMON(emitter, ASDF_ERR_STREAM_INIT_FAILED);
        return 1;
    }

    emitter->state = ASDF_EMITTER_STATE_INITIAL;
    return 0;
}


/** Helper utility to write a null-terminated string to the stream */
#define WRITE_STRING0(stream, string) \
    do { \
        size_t len = strlen((string)); \
        size_t n_written = asdf_stream_write((stream), (string), len); \
        if (n_written != len) \
            return ASDF_EMITTER_STATE_ERROR; \
    } while (0)


#define WRITE_NEWLINE(stream) \
    do { \
        if (1 != asdf_stream_write((stream), "\n", 1)) \
            return ASDF_EMITTER_STATE_ERROR; \
    } while (0)


static asdf_emitter_state_t emit_asdf_version(asdf_emitter_t *emitter) {
    assert(emitter);
    assert(emitter->stream && emitter->stream->is_writeable);
    WRITE_STRING0(emitter->stream, asdf_version_comment);
    WRITE_STRING0(emitter->stream, asdf_version_default);
    WRITE_NEWLINE(emitter->stream);
    asdf_stream_flush(emitter->stream);
    return ASDF_EMITTER_STATE_STANDARD_VERSION;
}


static asdf_emitter_state_t emit_standard_version(asdf_emitter_t *emitter) {
    WRITE_STRING0(emitter->stream, asdf_standard_comment);
    WRITE_STRING0(emitter->stream, asdf_standard_default);
    WRITE_NEWLINE(emitter->stream);
    asdf_stream_flush(emitter->stream);
    return ASDF_EMITTER_STATE_BLOCKS;
}


/**
 * Emit blocks to the file
 *
 * Very basic version that just emits the blocks serially; no compression is
 * supported yet or checksums, and the block header/data positions are assumed
 * unknown as yet.  Later this will need to be able to do things like backtrack
 * to write the header (or possibly compress to a temp file first to get
 * compression size--this might be useful for streaming), compute the the
 * checksum, etc)
 */
static asdf_emitter_state_t emit_blocks(asdf_emitter_t *emitter) {
    assert(emitter);
    assert(emitter->file);
    assert(emitter->stream);
    asdf_block_info_vec_t *blocks = &emitter->file->blocks;
    for (asdf_block_info_vec_iter_t it = asdf_block_info_vec_begin(blocks); it.ref;
         asdf_block_info_vec_next(&it)) {
        if (!asdf_block_info_write(emitter->stream, it.ref))
            return ASDF_EMITTER_STATE_ERROR;

        asdf_stream_flush(emitter->stream);
    }
    return ASDF_EMITTER_STATE_END;
}


asdf_emitter_state_t asdf_emitter_emit(asdf_emitter_t *emitter) {
    while (!emitter->done) {
        asdf_emitter_state_t next_state = ASDF_EMITTER_STATE_ERROR;
        switch (emitter->state) {
        case ASDF_EMITTER_STATE_INITIAL:
            // TODO: Whether or not to write anything actually depends on if there is
            // at minimum a tree or one block to write
            if (asdf_emitter_should_emit(emitter))
                next_state = ASDF_EMITTER_STATE_ASDF_VERSION;
            else {
                next_state = ASDF_EMITTER_STATE_END;
            }
            break;
        case ASDF_EMITTER_STATE_ASDF_VERSION:
            next_state = emit_asdf_version(emitter);
            break;
        case ASDF_EMITTER_STATE_STANDARD_VERSION:
            next_state = emit_standard_version(emitter);
            break;
        case ASDF_EMITTER_STATE_BLOCKS:
            next_state = emit_blocks(emitter);
            break;
        case ASDF_EMITTER_STATE_BLOCK_INDEX:
            // TODO
            break;
        case ASDF_EMITTER_STATE_END:
            next_state = ASDF_EMITTER_STATE_END;
            break;
        case ASDF_EMITTER_STATE_ERROR:
            next_state = ASDF_EMITTER_STATE_ERROR;
            break;
        }

        emitter->state = next_state;
        if (next_state == ASDF_EMITTER_STATE_ERROR || next_state == ASDF_EMITTER_STATE_END)
            emitter->done = true;
    }

    return emitter->state;
}


void asdf_emitter_destroy(asdf_emitter_t *emitter) {
    if (!emitter)
        return;

    if (emitter->stream)
        asdf_stream_close(emitter->stream);

    asdf_context_release(emitter->base.ctx);
    free(emitter);
}
