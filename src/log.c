#include <errno.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "context.h"
#include "file.h"
#include "log.h"
#include "util.h"


#define ANSI_RESET "\033[0m"
#define ANSI_DIM "\033[2m"
#define COLOR_BRIGHT_BLUE "\033[94m"
#define COLOR_CYAN "\033[36m"
#define COLOR_GREEN "\033[32m"
#define COLOR_YELLOW "\033[33m"
#define COLOR_RED "\033[31m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_DIM_GREY "\033[90m"
#define COLOR(color, str) color str ANSI_RESET
#define DIM(str) ANSI_DIM str ANSI_RESET

#define ASDF_LOG_LEVEL_ENV_VAR "ASDF_LOG_LEVEL"
#define ASDF_LOG_COLOR_ENV_VAR "ASDF_LOG_COLOR"


// Indexed by message severity; ASDF_LOG_DEFAULT is never a message severity
// so its slot is unused, and ASDF_LOG_NONE is past the end
static const char *level_names[] = {"DEFAULT", "TRACE", "DEBUG", "INFO", "WARN", "ERROR", "FATAL"};


_Static_assert(
    ASDF_LOG_NUM_LEVELS == ARRAY_SIZE(level_names),
    "Mismatch between log level enum and level_strings array");


#ifdef ASDF_LOG_COLOR
static const char *level_colors[] = {
    "", COLOR_BRIGHT_BLUE, COLOR_CYAN, COLOR_GREEN, COLOR_YELLOW, COLOR_RED, COLOR_MAGENTA};
_Static_assert(
    ASDF_LOG_NUM_LEVELS == ARRAY_SIZE(level_colors),
    "Mismatch between log level enum and level_strings array");
#endif


static asdf_log_level_t asdf_log_level_from_string(const char *str) {
    if (0 == strcasecmp(str, "NONE"))
        return ASDF_LOG_NONE;

    for (unsigned int idx = ASDF_LOG_TRACE; idx < ASDF_LOG_NUM_LEVELS; idx++) {
        if (0 == strcasecmp(str, level_names[idx]))
            return (asdf_log_level_t)idx;
    }

    // Fallback
    return ASDF_LOG_DEFAULT_LEVEL;
}


asdf_log_level_t asdf_log_level_from_env() {
    const char *env = getenv(ASDF_LOG_LEVEL_ENV_VAR);

    if (!env)
        return ASDF_LOG_DEFAULT_LEVEL;

    return asdf_log_level_from_string(env);
}


bool asdf_log_use_color(FILE *stream, bool no_color) {
#ifdef ASDF_LOG_COLOR
    const char *env = getenv(ASDF_LOG_COLOR_ENV_VAR);
    int saved_errno = errno;
    bool color = false;

    if (env && 0 == strcmp(env, "1"))
        return true;

    if (env && 0 == strcmp(env, "0"))
        return false;

    // Otherwise only colorize when writing to a terminal; for any other stream
    // (a file, a pipe, or a non-file stream with no descriptor at all) the
    // escape codes would end up in the output verbatim
    color = !no_color && isatty(fileno(stream));
    errno = saved_errno;
    return color;
#else
    (void)stream;
    (void)no_color;
    return false;
#endif
}


static void asdf_log_impl(
    FILE *stream,
    asdf_log_level_t level,
    asdf_log_fields_t fields,
    bool no_color,
    const char *file,
    int lineno,
    const char *fmt,
    va_list args) {
    // Logging must not clobber errno for callers that log before reporting it
    int saved_errno = errno;
    bool color = false;
    bool sep = false;

    // Messages can only be logged at a real severity; this also keeps the
    // level_names lookup in bounds
    if (UNLIKELY(level < ASDF_LOG_TRACE || level >= ASDF_LOG_NUM_LEVELS))
        return;

    // Don't allow logging "nothing" (logging should simply be disabled for
    // that); if fields is empty enable all fields
    fields = fields ? fields : ASDF_LOG_FIELD_ALL;

#ifdef ASDF_LOG_COLOR
    color = !no_color;
#else
    (void)no_color;
#endif

    if (fields & ASDF_LOG_FIELD_LEVEL) {
#ifdef ASDF_LOG_COLOR
        if (color)
            fprintf(
                stream,
                DIM("[") COLOR("%s", "%s") DIM("]"),
                level_colors[level],
                level_names[level]);
        else
#endif
            fprintf(stream, "[%-5s]", level_names[level]);

        sep = true;
    }

    if (fields & (ASDF_LOG_FIELD_PACKAGE | ASDF_LOG_FIELD_FILE | ASDF_LOG_FIELD_LINE)) {
        if (sep)
            fputc(' ', stream);

        if (color)
            fputs(COLOR_DIM_GREY, stream);

        if (fields & ASDF_LOG_FIELD_PACKAGE)
            fputs("(" PACKAGE_NAME ")", stream);

        if (fields & ASDF_LOG_FIELD_FILE)
            fprintf(stream, "%s:", file);

        if (fields & ASDF_LOG_FIELD_LINE)
            fprintf(stream, "%d:", lineno);

        if (color)
            fputs(ANSI_RESET, stream);

        sep = true;
    }

    if (fields & ASDF_LOG_FIELD_MSG) {
        if (sep)
            fputc(' ', stream);

        // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
        vfprintf(stream, fmt, args);
    }

    fputc('\n', stream);
    fflush(stream);
    errno = saved_errno;
}


void asdf_context_log(
    asdf_context_t *ctx,
    asdf_log_level_t level,
    const char *file,
    int lineno,
    const char *fmt,
    ...) {
    if (UNLIKELY(!ctx || !ctx->log.stream)) {
        asdf_log_fallback(ASDF_LOG_FATAL, file, lineno, "logging context not initialized");
        return;
    }

    if (ctx->log.level > level)
        return;

    // Bug in clang-tidy: https://github.com/llvm/llvm-project/issues/40656
    // Should be fixed in newer versions though...
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    va_list args;
    va_start(args, fmt);
    asdf_log_impl(
        ctx->log.stream, level, ctx->log.fields, ctx->log.no_color, file, lineno, fmt, args);
    va_end(args);
}


void asdf_file_log(
    const asdf_file_t *file,
    asdf_log_level_t level,
    const char *src_file,
    int lineno,
    const char *fmt,
    ...) {
    asdf_context_t *ctx = asdf_context_get((void *)file);
    va_list args;
    va_start(args, fmt);
    if (!ctx || !ctx->log.stream) {
        asdf_log_fallback(ASDF_LOG_FATAL, src_file, lineno, "logging context not initialized");
        va_end(args);
        return;
    }
    if (ctx->log.level > level) {
        va_end(args);
        return;
    }
    asdf_log_impl(
        ctx->log.stream, level, ctx->log.fields, ctx->log.no_color, src_file, lineno, fmt, args);
    va_end(args);
}


void asdf_log_fallback(asdf_log_level_t level, const char *file, int lineno, const char *fmt, ...) {
    // Bug in clang-tidy: https://github.com/llvm/llvm-project/issues/40656
    // Should be fixed in newer versions though...
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    va_list args;
    va_start(args, fmt);
    asdf_log_impl(
        stderr,
        level,
        ASDF_LOG_FIELD_ALL,
        !asdf_log_use_color(stderr, false),
        file,
        lineno,
        fmt,
        args);
    va_end(args);
}
