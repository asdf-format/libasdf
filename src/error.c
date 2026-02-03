#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "context.h"
#include "error.h"


static const char *const asdf_error_common_messages[] = {
    [ASDF_ERR_NONE] = NULL,
    [ASDF_ERR_UNKNOWN_STATE] = "Unknown parser state",
    [ASDF_ERR_STREAM_INIT_FAILED] = "Failed to initialize stream",
    [ASDF_ERR_STREAM_READ_ONLY] = "Tried to write to a read-only stream",
    [ASDF_ERR_UNEXPECTED_EOF] = "Unexpected end of file",
    [ASDF_ERR_INVALID_ASDF_HEADER] = "Invalid ASDF header",
    [ASDF_ERR_INVALID_BLOCK_HEADER] = "Invalid block header",
    [ASDF_ERR_BLOCK_MAGIC_MISMATCH] = "Block magic mismatch",
    [ASDF_ERR_YAML_PARSER_INIT_FAILED] = "YAML parser initialization failed",
    [ASDF_ERR_YAML_PARSE_FAILED] = "YAML parsing failed",
    [ASDF_ERR_OUT_OF_MEMORY] = "Out of memory",
};


/* Error helpers */
#define ASDF_ERROR_COMMON_MESSAGE(code) (asdf_error_common_messages[(code)])


const char *asdf_context_error_get(asdf_context_t *ctx) {
    if (!ctx)
        return NULL;

    return ctx->error;
}


void asdf_context_error_set_oom(asdf_context_t *ctx) {
    ctx->error = ASDF_ERROR_COMMON_MESSAGE(ASDF_ERR_OUT_OF_MEMORY);
    ctx->error_type = ASDF_ERROR_STATIC;
}


void asdf_context_error_set(asdf_context_t *ctx, const char *fmt, ...) {
    va_list args;
    int size;
    assert(ctx);

    if (ctx->error_type == ASDF_ERROR_HEAP)
        // Heap-allocated errors can be safely cast to (void *)
        // and freed.
        free((void *)ctx->error);

    ctx->error = NULL;

    va_start(args, fmt);

    // Bug in clang-tidy: https://github.com/llvm/llvm-project/issues/40656
    // Should be fixed in newer versions though...
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    size = vsnprintf(NULL, 0, fmt, args);
    va_end(args);

    // Ensure space for null termination
    char *error = malloc(size + 1);

    if (!error) {
        asdf_context_error_set_oom(ctx);
        return;
    }

    va_start(args, fmt);
    vsnprintf(error, size + 1, fmt, args);
    va_end(args);
    ctx->error = error; // implicit char* -> const char*; OK
    ctx->error_type = ASDF_ERROR_HEAP;
}


void asdf_context_error_set_static(asdf_context_t *ctx, const char *error) {
    if (ctx->error_type == ASDF_ERROR_HEAP)
        free((void *)ctx->error);

    ctx->error = error;
    ctx->error_type = ASDF_ERROR_STATIC;
}


void asdf_context_error_set_common(asdf_context_t *ctx, asdf_error_code_t code) {
    asdf_context_error_set_static(ctx, ASDF_ERROR_COMMON_MESSAGE(code));
}


void asdf_context_error_set_errno(asdf_context_t *ctx, int errnum) {
    ctx->error = strdup(strerror(errnum));
    ctx->error_type = ASDF_ERROR_HEAP;
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
}
