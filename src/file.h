#pragma once

#include <libfyaml.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "asdf/file.h" // IWYU pragma: export

#include "context.h"
#include "emitter.h"
#include "parser.h"


#ifdef HAVE_USERFAULTFD
/**
 * A macro which can be used at compile-time to check if lazy-mode
 * decompression is available.
 *
 * Currently only works when built on new-enough Linux versions that have
 * userfaultfd support, though can provide other implementations later.
 */
#define ASDF_BLOCK_DECOMP_LAZY_AVAILABLE HAVE_USERFAULTFD
#endif


/**
 * Open modes for files
 */
typedef enum {
    ASDF_FILE_MODE_INVALID = -1,
    ASDF_FILE_MODE_READ_ONLY,
    ASDF_FILE_MODE_WRITE_ONLY
} asdf_file_mode_t;


typedef struct asdf_file {
    asdf_base_t base;
    asdf_config_t *config;
    asdf_file_mode_t mode;
    asdf_parser_t *parser;
    asdf_emitter_t *emitter;
    struct fy_document *tree;
} asdf_file_t;


/* Internal helper to get the `struct fy_document` for the tree, if any */
ASDF_LOCAL struct fy_document *asdf_file_get_tree_document(asdf_file_t *file);

// Forward-declaration
typedef struct _asdf_block_comp_state_t asdf_block_comp_state_t;

/**
 * User-level object for inspecting ASDF block metadata and data
 */
typedef struct asdf_block {
    asdf_file_t *file;
    asdf_block_info_t info;
    void *data;
    // Should be the same as used_size in the header but may be truncated in exceptional
    // cases (we should probably log a warning when it is)
    size_t avail_size;

    const char *compression;
    asdf_block_comp_state_t *comp_state;
} asdf_block_t;
