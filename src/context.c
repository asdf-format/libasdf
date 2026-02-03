#include <stdatomic.h>
#include <stdbool.h>
#include <stdlib.h>

#include "context.h"
#include "log.h"
#include "util.h"


static asdf_global_context_t global_ctx = {0};
static atomic_bool global_ctx_initialized = false;


static asdf_log_cfg_t asdf_context_log_config_default = {
    .stream = NULL, .level = ASDF_LOG_NONE, .no_color = false};


asdf_context_t *asdf_context_create(const asdf_log_cfg_t *log_config) {
    asdf_context_t *ctx = malloc(sizeof(asdf_context_t));

    if (!ctx)
        return ctx;

    atomic_init(&ctx->refcount, 1);
    ctx->error = NULL;
    ctx->error_type = ASDF_ERROR_NONE;

    if (!log_config)
        log_config = &asdf_context_log_config_default;

    ctx->log.level = log_config->level == ASDF_LOG_NONE ? asdf_log_level_from_env()
                                                        : log_config->level;
    ctx->log.fields = log_config->fields ? log_config->fields : ASDF_LOG_FIELD_ALL;
    ctx->log.stream = log_config->stream ? log_config->stream : stderr;
    ctx->log.no_color = log_config->no_color;
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


ASDF_CONSTRUCTOR static void asdf_global_context_create() {
    if (atomic_load_explicit(&global_ctx_initialized, memory_order_acquire))
        return;

    asdf_context_t *ctx = asdf_context_create(NULL);

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


ASDF_DESTRUCTOR static void asdf_global_context_destroy(void) {
    if (atomic_load_explicit(&global_ctx_initialized, memory_order_acquire)) {
        asdf_context_release(global_ctx.base.ctx);
        atomic_store_explicit(&global_ctx_initialized, false, memory_order_release);
    }
}


asdf_global_context_t *asdf_global_context_get() {
    // Ensure created
    asdf_global_context_create();
    return &global_ctx;
}
