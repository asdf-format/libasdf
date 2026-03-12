#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "error.h"
#include "log.h"


/* Per-code format strings.  Codes whose format is NULL either carry no
 * message (ASDF_ERR_NONE) or compute the message dynamically
 * (ASDF_ERR_SYSTEM).  Codes whose format string contains no '%' are stored
 * as static strings with no heap allocation. */
static const char *const asdf_error_formats[] = {
    [ASDF_ERR_NONE] = NULL,
    [ASDF_ERR_UNKNOWN_STATE] = "unknown parser state",
    [ASDF_ERR_STREAM_INIT_FAILED] = "failed to initialize stream",
    [ASDF_ERR_STREAM_READ_ONLY] = "cannot write to a read-only stream or file",
    [ASDF_ERR_INVALID_ASDF_HEADER] = "invalid ASDF header",
    [ASDF_ERR_UNEXPECTED_EOF] = "unexpected end of file",
    [ASDF_ERR_INVALID_BLOCK_HEADER] = "invalid block header",
    [ASDF_ERR_BLOCK_MAGIC_MISMATCH] = "block magic mismatch",
    [ASDF_ERR_YAML_PARSER_INIT_FAILED] = "YAML parser initialization failed",
    [ASDF_ERR_YAML_PARSE_FAILED] = "YAML parsing failed",
    [ASDF_ERR_OUT_OF_MEMORY] = "out of memory",
    [ASDF_ERR_SYSTEM] = NULL, /* set dynamically from strerror */
    [ASDF_ERR_INVALID_ARGUMENT] = "invalid argument for %s: %s",
    [ASDF_ERR_UNKNOWN_COMPRESSION] = "unknown compression type: %s",
    [ASDF_ERR_COMPRESSION_FAILED] = "compression error: %s",
    [ASDF_ERR_EXTENSION_NOT_FOUND] = "no serializer registered for the %s extension",
    [ASDF_ERR_OVER_LIMIT] = "over limit: %s",
};

/* Per-code log levels */
static const asdf_log_level_t asdf_error_log_levels[] = {
    [ASDF_ERR_NONE] = ASDF_LOG_NONE,
    [ASDF_ERR_UNKNOWN_STATE] = ASDF_LOG_ERROR,
    [ASDF_ERR_STREAM_INIT_FAILED] = ASDF_LOG_ERROR,
    [ASDF_ERR_STREAM_READ_ONLY] = ASDF_LOG_ERROR,
    [ASDF_ERR_INVALID_ASDF_HEADER] = ASDF_LOG_ERROR,
    [ASDF_ERR_UNEXPECTED_EOF] = ASDF_LOG_ERROR,
    [ASDF_ERR_INVALID_BLOCK_HEADER] = ASDF_LOG_ERROR,
    [ASDF_ERR_BLOCK_MAGIC_MISMATCH] = ASDF_LOG_ERROR,
    [ASDF_ERR_YAML_PARSER_INIT_FAILED] = ASDF_LOG_FATAL,
    [ASDF_ERR_YAML_PARSE_FAILED] = ASDF_LOG_ERROR,
    [ASDF_ERR_OUT_OF_MEMORY] = ASDF_LOG_FATAL,
    [ASDF_ERR_SYSTEM] = ASDF_LOG_ERROR,
    [ASDF_ERR_INVALID_ARGUMENT] = ASDF_LOG_ERROR,
    [ASDF_ERR_UNKNOWN_COMPRESSION] = ASDF_LOG_ERROR,
    [ASDF_ERR_COMPRESSION_FAILED] = ASDF_LOG_ERROR,
    [ASDF_ERR_EXTENSION_NOT_FOUND] = ASDF_LOG_WARN,
};


/* Internal helper: replace the stored error string with a static (non-heap) one.
 * This never allocates, so it is safe to use from OOM paths. */
static void asdf_context_error_set_static(asdf_context_t *ctx, const char *error) {
    if (ctx->error_type == ASDF_ERROR_HEAP)
        free((void *)ctx->error);

    ctx->error = error;
    ctx->error_type = ASDF_ERROR_STATIC;
}


const char *asdf_context_error_get(asdf_context_t *ctx) {
    if (!ctx)
        return NULL;

    return ctx->error;
}


asdf_error_code_t asdf_context_error_code_get(asdf_context_t *ctx) {
    if (!ctx)
        return ASDF_ERR_NONE;

    return ctx->error_code;
}


int asdf_context_saved_errno_get(asdf_context_t *ctx) {
    if (!ctx)
        return 0;

    return ctx->saved_errno;
}


void asdf_context_error_set_common(
    asdf_context_t *ctx, asdf_error_code_t code, const char *src_file, int lineno, ...) {
    assert(ctx);
    assert(code != ASDF_ERR_SYSTEM); /* use asdf_context_error_set_system for OS errors */

    const char *fmt = asdf_error_formats[code];
    asdf_log_level_t level = asdf_error_log_levels[code];

    ctx->error_code = code;
    ctx->saved_errno = 0;

    if (!fmt) {
        /* ASDF_ERR_NONE */
        if (ctx->error_type == ASDF_ERROR_HEAP)
            free((void *)ctx->error);
        ctx->error = NULL;
        ctx->error_type = ASDF_ERROR_NONE;
        return;
    }

    if (!strchr(fmt, '%')) {
        /* No format specifiers -- use the table string directly (no allocation) */
        asdf_context_error_set_static(ctx, fmt);
    } else {
        /* Format string has specifiers -- build the message on the heap */
        if (ctx->error_type == ASDF_ERROR_HEAP)
            free((void *)ctx->error);

        ctx->error = NULL;

        va_list args;
        va_start(args, lineno);
        // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
        int size = vsnprintf(NULL, 0, fmt, args);
        va_end(args);

        char *error = malloc(size + 1);

        if (!error) {
            /* OOM while setting error -- fall back to the static OOM message */
            ctx->error = asdf_error_formats[ASDF_ERR_OUT_OF_MEMORY];
            ctx->error_type = ASDF_ERROR_STATIC;
            ctx->error_code = ASDF_ERR_OUT_OF_MEMORY;
            return;
        }

        va_start(args, lineno);
        vsnprintf(error, size + 1, fmt, args);
        va_end(args);

        ctx->error = error;
        ctx->error_type = ASDF_ERROR_HEAP;
    }

#ifdef ASDF_LOG_ENABLED
    if (level >= ASDF_LOG_MIN_LEVEL && ctx->error)
        asdf_log(ctx, level, src_file, lineno, "%s", ctx->error);
#else
    (void)level;
    (void)src_file;
    (void)lineno;
#endif
}


void asdf_context_error_set_oom(asdf_context_t *ctx, const char *src_file, int lineno) {
    /* Never allocates -- safe to call from OOM paths */
    asdf_context_error_set_static(ctx, asdf_error_formats[ASDF_ERR_OUT_OF_MEMORY]);
    ctx->error_code = ASDF_ERR_OUT_OF_MEMORY;
    ctx->saved_errno = 0;

#ifdef ASDF_LOG_ENABLED
    if (ASDF_LOG_FATAL >= ASDF_LOG_MIN_LEVEL && ctx->error)
        asdf_log(ctx, ASDF_LOG_FATAL, src_file, lineno, "%s", ctx->error);
#else
    (void)src_file;
    (void)lineno;
#endif
}


void asdf_context_error_set_system(
    asdf_context_t *ctx, int errnum, const char *src_file, int lineno) {
    assert(ctx);

    if (ctx->error_type == ASDF_ERROR_HEAP)
        free((void *)ctx->error);

    ctx->error = strdup(strerror(errnum));
    ctx->error_type = ASDF_ERROR_HEAP;
    ctx->error_code = ASDF_ERR_SYSTEM;
    ctx->saved_errno = errnum;

#ifdef ASDF_LOG_ENABLED
    if (ASDF_LOG_ERROR >= ASDF_LOG_MIN_LEVEL && ctx->error)
        asdf_log(ctx, ASDF_LOG_ERROR, src_file, lineno, "%s", ctx->error);
#else
    (void)src_file;
    (void)lineno;
#endif
}


void asdf_context_error_copy(asdf_context_t *dst, const asdf_context_t *src) {
    if (!dst)
        dst = asdf_get_context_helper(NULL);

    if (!src)
        src = asdf_get_context_helper(NULL);

    if (dst == src || !dst || !src)
        return;

    if (src->error_type == ASDF_ERROR_HEAP && src->error)
        dst->error = strdup(src->error);
    else
        dst->error = src->error;

    dst->error_type = src->error_type;
    dst->error_code = src->error_code;
    dst->saved_errno = src->saved_errno;
}
