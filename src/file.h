#pragma once

#include <libfyaml.h>

#include <asdf/file.h>

#include "context.h"
#include "parse.h"
#include "util.h"


typedef struct asdf_file {
    asdf_base_t base;
    asdf_parser_t *parser;
    struct fy_document *tree;
} asdf_file_t;


/* Internal helper to get the `struct fy_document` for the tree, if any */
ASDF_LOCAL struct fy_document *asdf_file_get_tree_document(asdf_file_t *file);


/**
 * User-level object for inspecting ASDF block metadata and data
 */
typedef struct asdf_block {
    asdf_file_t *file;
    asdf_block_info_t info;
    void *data;
    // Should be the same as used_size in the header but may be truncated in exceptional
    // cases (we should probably log a warning when it is)
    size_t data_size;
} asdf_block_t;
