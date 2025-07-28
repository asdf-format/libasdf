#include <stdatomic.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "context.h"
#include "error.h"
#include "log.h"
#include "util.h"


static asdf_global_context_t global_ctx = {0};
static atomic_bool global_ctx_initialized = false;


asdf_context_t *asdf_context_create() {
    asdf_context_t *ctx = malloc(sizeof(asdf_context_t));

    if (!ctx)
        return ctx;

    atomic_init(&ctx->refcount, 1);
    ctx->error = NULL;
    ctx->error_type = ASDF_ERROR_NONE;

    // Maybe initialize from ASDF_LOG_LEVEL environment variable, or fallback to default
    ctx->log.level = asdf_log_level_from_env();
    ctx->log.stream = stderr;
    return ctx;
}


void asdf_context_destroy(asdf_context_t *ctx) {
    if (!ctx)
        return;

    if (ctx->error_type == ASDF_ERROR_HEAP)
        free((void *)ctx->error);

    free(ctx);
}


void asdf_context_retain(asdf_context_t *ctx) {
    if (UNLIKELY(!ctx))
        return;

    atomic_fetch_add_explicit(&ctx->refcount, 1, memory_order_relaxed);
}


void asdf_context_release(asdf_context_t *ctx) {
    if (UNLIKELY(!ctx))
        return;
    if (atomic_fetch_sub_explicit(&ctx->refcount, 1, memory_order_acq_rel) == 1)
        asdf_context_destroy(ctx);
}


asdf_global_context_t *asdf_global_context_get() {
    return &global_ctx;
}


__attribute__((constructor)) static void asdf_global_context_create() {
    if (atomic_load_explicit(&global_ctx_initialized, memory_order_acquire))
        return;

    asdf_context_t *ctx = asdf_context_create();

    if (!ctx) {
        asdf_log_fallback(
            ASDF_LOG_FATAL,
            __FILE__,
            __LINE__,
            "failed to initialize libasdf global context, likely due to low memory");
        return;
    }

    global_ctx.base.ctx = ctx;
    atomic_store_explicit(&global_ctx_initialized, true, memory_order_release);
}


__attribute__((destructor)) static void asdf_global_context_destroy(void) {
    if (atomic_load_explicit(&global_ctx_initialized, memory_order_acquire)) {
        asdf_context_release(global_ctx.base.ctx);
        atomic_store_explicit(&global_ctx_initialized, false, memory_order_release);
    }
}
