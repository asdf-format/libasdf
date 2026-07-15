#pragma once

#include "asdf/error.h" // IWYU pragma: export
#include "context.h"


/* Internal error helper functions */
ASDF_LOCAL const char *asdf_context_error_get(asdf_context_t *ctx);
ASDF_LOCAL asdf_error_code_t asdf_context_error_code_get(asdf_context_t *ctx);
ASDF_LOCAL int asdf_context_saved_errno_get(asdf_context_t *ctx);

/* These take __FILE__ / __LINE__ from the call-site macro so that log messages
 * show the correct source location rather than a line inside error.c. */
ASDF_LOCAL void asdf_context_error_common(
    asdf_context_t *ctx, asdf_error_code_t code, const char *file, int lineno, ...);
ASDF_LOCAL void asdf_context_error_oom(asdf_context_t *ctx, const char *file, int lineno);
ASDF_LOCAL void asdf_context_error_system(
    asdf_context_t *ctx, int errnum, const char *file, int lineno);
ASDF_LOCAL void asdf_context_error_copy(asdf_context_t *dst, const asdf_context_t *src);


/**
 * Macros for setting errors on arbitrary ASDF base types
 *
 * These should be used more generally than the ``asdf_context_error_*`` functions.
 *
 * Versions of these are also defined in the public ``asdf/error.h`` header, but
 * that don't expose the ``asdf_context_t`` internals.  Internally these versions
 * are used instead.
 */

/* Read the current error message string */
#define ASDF_ERROR_GET(obj) asdf_context_error_get(((asdf_base_t *)(obj))->ctx)

/* Read the current error code */
#define ASDF_ERROR_CODE_GET(obj) asdf_context_error_code_get(((asdf_base_t *)(obj))->ctx)

/* Set an error with a code; optional variadic args are the format parameters for
 * the per-code format string defined in error.c */
#undef ASDF_ERROR_COMMON
#define ASDF_ERROR_COMMON(obj, code, ...) \
    asdf_context_error_common(asdf_context_get(obj), (code), __FILE__, __LINE__, ##__VA_ARGS__)

/* Set an out-of-memory error (never allocates) */
#undef ASDF_ERROR_OOM
#define ASDF_ERROR_OOM(obj) asdf_context_error_oom(asdf_context_get(obj), __FILE__, __LINE__)

/* Set a system (OS) error from an errno value */
#undef ASDF_ERROR_SYSTEM
#define ASDF_ERROR_SYSTEM(obj, errnum) \
    asdf_context_error_system(asdf_context_get(obj), (errnum), __FILE__, __LINE__)
