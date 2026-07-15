#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "asdf/log.h" // IWYU pragma: export

#include "context.h"
#include "util.h"


#ifndef ASDF_LOG_DEFAULT_LEVEL
#define ASDF_LOG_DEFAULT_LEVEL ASDF_LOG_WARN
#endif


#ifndef ASDF_LOG_MIN_LEVEL
#ifdef ASDF_CHECKING
#define ASDF_LOG_MIN_LEVEL ASDF_LOG_WARN
#else
#define ASDF_LOG_MIN_LEVEL ASDF_LOG_TRACE
#endif
#endif


/* Override the public ASDF_LOG macro with the internal polymorphic version.
 * Internal libasdf code passes a variety of asdf_base_t-based objects (parser,
 * emitter, stream, etc.) as the first argument, not just asdf_file_t *.
 * The public macro in <asdf/log.h> is for extension authors who always have
 * an asdf_file_t * (obtained via asdf_value_file()). */
#ifdef ASDF_LOG_ENABLED
#undef ASDF_LOG
#define ASDF_LOG(obj, level, ...) \
    do { \
        if ((level) >= ASDF_LOG_MIN_LEVEL) { \
            asdf_context_log(asdf_context_get(obj), (level), __FILE__, __LINE__, __VA_ARGS__); \
        } \
    } while (0)

/* Internal macro for logging with an explicit asdf_context_t */
#define ASDF_LOG_CTX(ctx, level, ...) \
    do { \
        if ((level) >= ASDF_LOG_MIN_LEVEL) { \
            asdf_context_log(ctx, (level), __FILE__, __LINE__, __VA_ARGS__); \
        } \
    } while (0)
#else
#define ASDF_LOG_CTX(obj, level, ...) ((void)0)
#endif


#ifdef ASDF_LOG_ENABLED
#define ASDF_LOG_LEVEL(obj) ((asdf_base_t *)(obj))->ctx->log.level
#else
#define ASDF_LOG_LEVEL(obj) 0
#endif


ASDF_LOCAL void asdf_context_log(
    asdf_context_t *ctx,
    asdf_log_level_t level,
    const char *file,
    int lineno,
    const char *fmt,
    ...);


/* Last-resort logging without an `asdf_context_t`, for internal use only */
ASDF_LOCAL void asdf_log_fallback(
    asdf_log_level_t level, const char *file, int lineno, const char *fmt, ...);


ASDF_LOCAL asdf_log_level_t asdf_log_level_from_env(void);
