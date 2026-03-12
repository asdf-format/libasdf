#pragma once

#include "asdf/error.h" // IWYU pragma: export
#include "context.h"


/* Internal error helper functions */
ASDF_LOCAL const char *asdf_context_error_get(asdf_context_t *ctx);
ASDF_LOCAL asdf_error_code_t asdf_context_error_code_get(asdf_context_t *ctx);
ASDF_LOCAL int asdf_context_saved_errno_get(asdf_context_t *ctx);

/* These take __FILE__ / __LINE__ from the call-site macro so that log messages
 * show the correct source location rather than a line inside error.c. */
ASDF_LOCAL void asdf_context_error_set_common(
    asdf_context_t *ctx, asdf_error_code_t code, const char *file, int lineno, ...);
ASDF_LOCAL void asdf_context_error_set_oom(asdf_context_t *ctx, const char *file, int lineno);
ASDF_LOCAL void asdf_context_error_set_system(
    asdf_context_t *ctx, int errnum, const char *file, int lineno);
ASDF_LOCAL void asdf_context_error_copy(asdf_context_t *dst, const asdf_context_t *src);


/**
 * Macros for setting errors on arbitrary ASDF base types
 *
 * These should be used more generally than the ``asdf_context_error_*`` functions.
 */

/* Read the current error message string */
#define ASDF_ERROR_GET(obj) asdf_context_error_get(((asdf_base_t *)(obj))->ctx)

/* Read the current error code */
#define ASDF_ERROR_CODE_GET(obj) asdf_context_error_code_get(((asdf_base_t *)(obj))->ctx)

/* Set an error with a code; optional variadic args are the format parameters for
 * the per-code format string defined in error.c */
#define ASDF_ERROR_COMMON(obj, code, ...) \
    do { \
        ASDF_GET_CONTEXT(obj); \
        asdf_context_error_set_common(__ctx, (code), __FILE__, __LINE__, ##__VA_ARGS__); \
    } while (0)

/* Set an out-of-memory error (never allocates) */
#define ASDF_ERROR_OOM(obj) \
    do { \
        ASDF_GET_CONTEXT(obj); \
        asdf_context_error_set_oom(__ctx, __FILE__, __LINE__); \
    } while (0)

/* Set a system (OS) error from an errno value */
#define ASDF_ERROR_SYSTEM(obj, errnum) \
    do { \
        ASDF_GET_CONTEXT(obj); \
        asdf_context_error_set_system(__ctx, (errnum), __FILE__, __LINE__); \
    } while (0)
