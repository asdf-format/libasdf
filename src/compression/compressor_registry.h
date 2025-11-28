#pragma once

#include <asdf/util.h>

#include "compression.h"


ASDF_LOCAL void asdf_compressor_register(asdf_compressor_t *comp);
ASDF_LOCAL const asdf_compressor_t *asdf_compressor_get(asdf_file_t *file, const char *compression);


#define ASDF_PREFIX asdf


/* Macro helpers */
#define _ASDF_PASTE(a, b) a##b
#define _ASDF_EXPAND(a, b) _ASDF_PASTE(a, b)


#define ASDF_COMPRESSOR_STATIC_NAME(extname) _ASDF_EXPAND(ASDF_PREFIX, _##compression##_compressor)


#define ASDF_COMPRESSOR_DEFINE(_compression, _init, _destroy, _info, _decomp) \
    static asdf_compressor_t ASDF_COMPRESSOR_STATIC_NAME(_compression) = { \
        .compression = #_compression, \
        .init = (_init), \
        .destroy = (_destroy), \
        .info = (_info), \
        .decomp = (_decomp)}

/**
 * Internal utility to register a new compressor extension
 *
 * Interface is provisional for now.
 */
#define ASDF_REGISTER_COMPRESSOR(compression, init, destroy, info, decomp) \
    ASDF_COMPRESSOR_DEFINE(compression, init, destroy, info, decomp); \
    static ASDF_CONSTRUCTOR void _ASDF_EXPAND( \
        ASDF_PREFIX, _register_##compression##_extension)(void) { \
        asdf_compressor_register(&ASDF_COMPRESSOR_STATIC_NAME(compression)); \
    }
