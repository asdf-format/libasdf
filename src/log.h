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


#ifdef ASDF_LOG_ENABLED
#define ASDF_LOG(obj, level, fmt, ...) \
    do { \
        if ((level) >= ASDF_LOG_MIN_LEVEL) { \
            __ASDF_GET_CONTEXT(obj); \
            asdf_log(__ctx, (level), __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        } \
    } while (0)

#define ASDF_LOG_CTX(ctx, level, fmt, ...) \
    do { \
        if ((level) >= ASDF_LOG_MIN_LEVEL) { \
            asdf_log(ctx, (level), __FILE__, __LINE__, fmt, ##__VA_ARGS__); \
        } \
    } while (0)
#else
#define ASDF_LOG(obj, level, fmt, ...) ((void)0)
#define ASDF_LOG_CTX(obj, level, fmt, ...) ((void)0)
#endif


#ifdef ASDF_LOG_ENABLED
#define ASDF_LOG_LEVEL(obj) ((asdf_base_t *)(obj))->ctx->log.level
#else
#define ASDF_LOG_LEVEL(obj) 0
#endif


ASDF_LOCAL void asdf_log(
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
