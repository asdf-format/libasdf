/**
 * ASDF emitter, responsible for writing ASDF files
 *
 * Somewhat mirrors the ASDF parser but maintains state specific to writing files.  Still entirely
 * incomplete and provisional while we start building out write support.
 *
 * Later will also support tracking the sizes and offsets of each section written, determining when
 * flushing if a section needs to be moved/rewritten, etc.
 */
#pragma once

#include "asdf/emitter.h" // IWYU pragma: export

#include "context.h"
#include "stream.h"
#include "util.h"


typedef enum {
    ASDF_EMITTER_STATE_INITIAL,
    ASDF_EMITTER_STATE_ASDF_VERSION,
    ASDF_EMITTER_STATE_STANDARD_VERSION,
    ASDF_EMITTER_STATE_END,
    ASDF_EMITTER_STATE_ERROR
} asdf_emitter_state_t;


// Forward-declaration
typedef struct asdf_file asdf_file_t;


typedef struct asdf_emitter {
    asdf_base_t base;
    /**
     * The file to emit from
     *
     * Unlike ``asdf_parser_t`` which stands on its own, the emitter needs a
     * handle to the file to know what to emit.
     */
    asdf_file_t *file;
    asdf_emitter_cfg_t config;
    asdf_emitter_state_t state;
    asdf_stream_t *stream;
    bool done;
} asdf_emitter_t;


ASDF_LOCAL asdf_emitter_t *asdf_emitter_create(asdf_file_t *file, asdf_emitter_cfg_t *config);
ASDF_LOCAL void asdf_emitter_destroy(asdf_emitter_t *emitter);
ASDF_LOCAL asdf_emitter_state_t asdf_emitter_emit(asdf_emitter_t *emit);
ASDF_LOCAL int asdf_emitter_set_output_file(asdf_emitter_t *emitter, const char *filename);


static inline bool asdf_emitter_has_opt(asdf_emitter_t *emitter, asdf_emitter_opt_t opt) {
    assert(emitter);
    return ((emitter->config.flags & opt) == opt);
}
