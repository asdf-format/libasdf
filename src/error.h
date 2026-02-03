#pragma once

#include "context.h"


typedef enum {
    ASDF_ERR_NONE = 0,

    ASDF_ERR_UNKNOWN_STATE,
    ASDF_ERR_STREAM_INIT_FAILED,
    ASDF_ERR_STREAM_READ_ONLY,
    ASDF_ERR_INVALID_ASDF_HEADER,
    ASDF_ERR_UNEXPECTED_EOF,
    ASDF_ERR_INVALID_BLOCK_HEADER,
    ASDF_ERR_BLOCK_MAGIC_MISMATCH,
    ASDF_ERR_YAML_PARSER_INIT_FAILED,
    ASDF_ERR_YAML_PARSE_FAILED,
    ASDF_ERR_OUT_OF_MEMORY,
} asdf_error_code_t;


/* Internal error helper functions */
ASDF_LOCAL const char *asdf_context_error_get(asdf_context_t *ctx);
ASDF_LOCAL void asdf_context_error_set(asdf_context_t *ctx, const char *fmt, ...);
ASDF_LOCAL void asdf_context_error_set_oom(asdf_context_t *ctx);
ASDF_LOCAL void asdf_context_error_set_static(asdf_context_t *ctx, const char *error);
ASDF_LOCAL void asdf_context_error_set_common(asdf_context_t *ctx, asdf_error_code_t code);
ASDF_LOCAL void asdf_context_error_set_errno(asdf_context_t *ctx, int errnum);
ASDF_LOCAL void asdf_context_error_copy(asdf_context_t *dst, const asdf_context_t *src);

/**
 * Macros for setting errors on arbitrary ASDF base types
 *
 * These should be used more generally than the ``asdf_context_error_*`` functions.
 */
#define ASDF_ERROR_GET(obj) asdf_context_error_get(((asdf_base_t *)(obj))->ctx)


#define ASDF_ERROR(obj, fmt, ...) \
    do { \
        ASDF_GET_CONTEXT(obj); \
        asdf_context_error_set(__ctx, (fmt), ##__VA_ARGS__); \
    } while (0)


#define ASDF_ERROR_OOM(obj) \
    do { \
        ASDF_GET_CONTEXT(obj); \
        asdf_context_error_set_oom(__ctx); \
    } while (0)


#define ASDF_ERROR_STATIC(obj, error) \
    do { \
        ASDF_GET_CONTEXT(obj); \
        asdf_context_error_set_static(__ctx, (error)); \
    } while (0)


#define ASDF_ERROR_COMMON(obj, code) \
    do { \
        ASDF_GET_CONTEXT(obj); \
        asdf_context_error_set_common(__ctx, (code)); \
    } while (0)


#define ASDF_ERROR_ERRNO(obj, errnum) \
    do { \
        ASDF_GET_CONTEXT(obj); \
        asdf_context_error_set_errno(__ctx, (errnum)); \
    } while (0)
