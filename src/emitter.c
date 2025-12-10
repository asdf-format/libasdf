#include <assert.h>
#include <stdbool.h>

#include "context.h"
#include "emitter.h"
#include "error.h"
#include "parse_util.h"
#include "stream.h"

/**
 * Default libasdf emitter configuration
 */
static const asdf_emitter_cfg_t default_asdf_emitter_cfg = {};


asdf_emitter_t *asdf_emitter_create(asdf_emitter_cfg_t *config) {
    asdf_emitter_t *emitter = calloc(1, sizeof(asdf_emitter_t));

    if (!emitter)
        return emitter;

    asdf_context_t *ctx = asdf_context_create();

    if (!ctx) {
        free(emitter);
        return NULL;
    }

    emitter->base.ctx = ctx;
    emitter->config = config ? *config : default_asdf_emitter_cfg;
    emitter->state = ASDF_EMITTER_STATE_INITIAL;
    emitter->done = false;
    return emitter;
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

    // TODO: Whether or not to write anything actually depends on if there is
    // at minimum a tree or one block to write
    if (asdf_emitter_has_opt(emitter, ASDF_EMITTER_OPT_EMIT_EMPTY))
        emitter->state = ASDF_EMITTER_STATE_ASDF_VERSION;
    else {
        emitter->state = ASDF_EMITTER_STATE_END;
        emitter->done = true;
    }

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
    return ASDF_EMITTER_STATE_END;
}


asdf_emitter_state_t asdf_emitter_emit(asdf_emitter_t *emitter) {
    while (!emitter->done) {
        asdf_emitter_state_t next_state = ASDF_EMITTER_STATE_ERROR;
        switch (emitter->state) {
        case ASDF_EMITTER_STATE_INITIAL:
            ASDF_ERROR_COMMON(emitter, ASDF_ERR_STREAM_INIT_FAILED);
            break;
        case ASDF_EMITTER_STATE_ASDF_VERSION:
            next_state = emit_asdf_version(emitter);
            break;
        case ASDF_EMITTER_STATE_STANDARD_VERSION:
            next_state = emit_standard_version(emitter);
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
