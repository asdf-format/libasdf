/** Internal compressed block utilities */
#pragma once

#include "context.h"


// TODO: (?) extension interface for adding new compression methods?
// Maybe useful for adding lz4 support as an example.  Punt for now...
typedef enum {
    ASDF_BLOCK_COMP_UNKNOWN = -1,
    ASDF_BLOCK_COMP_NONE = 0,
    ASDF_BLOCK_COMP_ZLIB = 1,
    ASDF_BLOCK_COMP_BZP2 = 2
} asdf_block_comp_t;


// Forward-declaration
typedef struct asdf_block asdf_block_t;


ASDF_LOCAL asdf_block_comp_t asdf_block_comp_parse(asdf_context_t *ctx, const char *compression);
ASDF_LOCAL int asdf_block_open_compressed(asdf_block_t *block, size_t *size);
