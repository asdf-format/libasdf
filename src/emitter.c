#include <assert.h>
#include <stdbool.h>
#include <string.h>

#include <libfyaml.h>

#include "block.h"
#include "context.h"
#include "emitter.h"
#include "error.h"
#include "file.h"
#include "parse_util.h"
#include "stream.h"
#include "types/asdf_block_info_vec.h"
#include "yaml.h"

/**
 * Default libasdf emitter configuration
 */
static const asdf_emitter_cfg_t asdf_emitter_cfg_default = ASDF_EMITTER_CFG_DEFAULT;


asdf_emitter_t *asdf_emitter_create(asdf_file_t *file, asdf_emitter_cfg_t *config) {
    asdf_emitter_t *emitter = calloc(1, sizeof(asdf_emitter_t));

    if (!emitter)
        return emitter;

    emitter->base.ctx = file->base.ctx;
    asdf_context_retain(emitter->base.ctx);
    emitter->file = file;
    emitter->config = config ? *config : asdf_emitter_cfg_default;
    emitter->state = ASDF_EMITTER_STATE_INITIAL;
    emitter->done = false;
    return emitter;
}


static bool asdf_emitter_should_emit_tree(asdf_emitter_t *emitter) {
    bool emit_empty = asdf_emitter_has_opt(emitter, ASDF_EMITTER_OPT_EMIT_EMPTY_TREE);

    if (asdf_emitter_has_opt(emitter, ASDF_EMITTER_OPT_NO_EMIT_EMPTY_TREE))
        emit_empty = false;

    struct fy_document *tree = emitter->file->tree;

    // If the tree was never created, no reason to create it here either,
    // just return based on the emit option
    if (!tree)
        return emit_empty;

    struct fy_node *root = fy_document_root(tree);

    // If there is is no root node explicitly set libfyaml's emitter actually
    // returns an error. If the EMIT_EMPTY_TREE flag was set then this is ok
    // and we can just skip tree emission altogether.  Otherwise assign an
    // empty root node and proceed.
    if (!root) {
        if (!emit_empty)
            return false;

        root = fy_node_create_mapping(tree);
        // May not succeed in principle but we'll find out when it emits NULL
        // below
        fy_document_set_root(tree, root);
    } else if (!emit_empty && fy_node_mapping_is_empty(root)) {
        // root node is set but is an empty mapping, so same deal
        return false;
    }

    return true;
}


/** Helper to determine if there is any content to write to the file */
static bool asdf_emitter_should_emit(asdf_emitter_t *emitter) {
    bool should_emit = asdf_emitter_should_emit_tree(emitter);

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
    return ASDF_EMITTER_STATE_TREE;
}


typedef struct {
    asdf_stream_t *stream;
} asdf_fy_emitter_userdata_t;


/**
 * Custom outputter for the fy_emitter that just writes to our stream
 */
static int fy_emitter_stream_output(
    struct fy_emitter *fy_emit,
    UNUSED(enum fy_emitter_write_type type),
    const char *str,
    int len,
    void *userdata) {
    assert(fy_emit);
    assert(userdata);
    asdf_fy_emitter_userdata_t *asdf_userdata = userdata;

    if (!asdf_userdata->stream)
        return 0;

    return (int)asdf_stream_write(asdf_userdata->stream, str, len);
}


static void asdf_fy_emitter_finalize(struct fy_emitter *emitter) {
    const struct fy_emitter_cfg *cfg = fy_emitter_get_cfg(emitter);
    asdf_fy_emitter_userdata_t *userdata = cfg->userdata;
    free(userdata);
}


/**
 * Common logic for creating and configuring a libfyaml ``fy_emitter`` to write
 * to our stream with our default settings
 */
static struct fy_emitter *asdf_fy_emitter_create(asdf_emitter_t *emitter) {
    asdf_fy_emitter_userdata_t *userdata = malloc(sizeof(asdf_fy_emitter_userdata_t));

    if (!userdata)
        return NULL;

    userdata->stream = emitter->stream;

    // NOTE: There are many, many different options that can be passed to the
    // libfyaml document emitter; we will probably want to give some
    // opportunities to control this, and also determine which options hew
    // closest to the Python output
    struct fy_emitter_cfg config = {
        .flags = FYECF_DEFAULT | FYECF_DOC_END_MARK_ON,
        .output = fy_emitter_stream_output,
        .userdata = userdata};

    struct fy_emitter *fy_emitter = fy_emitter_create(&config);

    if (!fy_emitter)
        return NULL;

    fy_emitter_set_finalizer(fy_emitter, asdf_fy_emitter_finalize);

    /* Workaround for the bug in libfyaml 0.9.3 described in
     * https://github.com/asdf-format/libasdf/issues/144
     *
     * This works by writing dummy empty document to the emitter (with no
     * output, i.e. stream set to NULL) followed by an explicit document end
     * marker.
     *
     * This tricks the emitter state into thinking it's already written a
     * document and does not need to output a document end marker anymore.
     */
    if (strcmp(fy_library_version(), "0.9.3") == 0) {
        userdata->stream = NULL;
        struct fy_document *dummy = fy_document_create(NULL);
        fy_emit_document(fy_emitter, dummy);
        fy_emit_document_end(fy_emitter);
        fy_document_destroy(dummy);
        userdata->stream = emitter->stream;
    }

    return fy_emitter;
}


/**
 * Emit the YAML tree to the file
 */
static asdf_emitter_state_t emit_tree(asdf_emitter_t *emitter) {
    if (!asdf_emitter_should_emit_tree(emitter))
        return ASDF_EMITTER_STATE_BLOCKS;

    struct fy_document *tree = asdf_file_get_tree_document(emitter->file);

    // There *should* be a tree now, even if empty
    if (!tree)
        return ASDF_EMITTER_STATE_ERROR;

    struct fy_emitter *fy_emitter = asdf_fy_emitter_create(emitter);

    if (!fy_emitter)
        return ASDF_EMITTER_STATE_ERROR;

    if (fy_emit_document(fy_emitter, tree) != 0) {
        fy_emitter_destroy(fy_emitter);
        return ASDF_EMITTER_STATE_ERROR;
    }

    fy_emitter_destroy(fy_emitter);
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
    bool checksum = !(emitter->config.flags & ASDF_EMITTER_OPT_NO_BLOCK_CHECKSUM);

    for (asdf_block_info_vec_iter_t it = asdf_block_info_vec_begin(blocks); it.ref;
         asdf_block_info_vec_next(&it)) {
        if (!asdf_block_info_write(emitter->stream, it.ref, checksum))
            return ASDF_EMITTER_STATE_ERROR;

        asdf_stream_flush(emitter->stream);
    }
    return ASDF_EMITTER_STATE_BLOCK_INDEX;
}


static asdf_emitter_state_t emit_block_index(asdf_emitter_t *emitter) {
    assert(emitter);
    assert(emitter->file);
    assert(emitter->stream);

    asdf_emitter_state_t next_state = ASDF_EMITTER_STATE_END;

    if (asdf_emitter_has_opt(emitter, ASDF_EMITTER_OPT_NO_BLOCK_INDEX)) {
        /* Do not write the block index. Technically the block index is optional and can be
         * removed.  The ASDF Standard also states that the block index is incompatible with
         * streaming mode though I'm not sure I understand the rationale behind that.  It's true
         * that if a block is open for streaming you're supposed to be able to continue appending
         * to it ad-infinitum.  But one could also have the possibility of "closing" a stream,
         * disallowing further writing, at which point it could make sense to append a block
         * block index.  Nevertheless, that idea does not yet exist as part of the standard
         */
        return next_state;
    }

    asdf_block_info_vec_t *blocks = &emitter->file->blocks;

    if (asdf_block_info_vec_size(blocks) <= 0)
        // No blocks, so no need to write a block index
        return next_state;

    struct fy_document *doc = asdf_yaml_create_empty_document(NULL);
    struct fy_node *index = NULL;
    struct fy_node *offset = NULL;
    struct fy_emitter *fy_emitter = NULL;

    // For the rest of the function it's useful to assume the next state will be
    // an error unless we explicitly succeed
    next_state = ASDF_EMITTER_STATE_ERROR;

    if (!doc) {
        ASDF_ERROR_OOM(emitter);
        goto cleanup;
    }

    // Sequence node to hold the block index--it is the root of the block index document
    index = fy_node_create_sequence(doc);

    if (!index || 0 != fy_document_set_root(doc, index)) {
        // TODO: Error message for this? Hard to see why it would fail except a memory error
        ASDF_ERROR_OOM(emitter);
        goto cleanup;
    }

    for (asdf_block_info_vec_iter_t it = asdf_block_info_vec_begin(blocks); it.ref;
         asdf_block_info_vec_next(&it)) {

        if (it.ref->header_pos < 0) {
            // Invalid block; should not happen
            goto cleanup;
        }

        offset = fy_node_buildf(doc, "%" PRIu64, (uint64_t)it.ref->header_pos);

        if (!offset || 0 != fy_node_sequence_append(index, offset)) {
            ASDF_ERROR_OOM(emitter);
            goto cleanup;
        }
    }

    WRITE_STRING0(emitter->stream, asdf_block_index_header);
    WRITE_NEWLINE(emitter->stream);

    fy_emitter = asdf_fy_emitter_create(emitter);

    if (!fy_emitter) {
        ASDF_ERROR_OOM(emitter);
        goto cleanup;
    }

    if (fy_emit_document(fy_emitter, doc) != 0)
        goto cleanup;

    next_state = ASDF_EMITTER_STATE_END;

cleanup:
    if (doc)
        fy_document_destroy(doc);

    if (fy_emitter)
        fy_emitter_destroy(fy_emitter);

    return next_state;
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
        case ASDF_EMITTER_STATE_TREE:
            next_state = emit_tree(emitter);
            break;
        case ASDF_EMITTER_STATE_BLOCKS:
            next_state = emit_blocks(emitter);
            break;
        case ASDF_EMITTER_STATE_BLOCK_INDEX:
            next_state = emit_block_index(emitter);
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
