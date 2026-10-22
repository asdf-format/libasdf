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

// We don't support _WIN32 yet really but I have a branch where I'm working on
// it so might as well supply its equivalents since they apparently exist, in
// anticipation of that.
#ifdef _WIN32
#define flockfile _lock_file
#define funlockfile _unlock_file
#endif

/*
 * Size of the on-stack buffer log messages are formatted into; longer
 * messages are formatted into a heap buffer instead
 */
#define ASDF_LOG_MSG_BUF_SIZE 512

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

    // Eexplicitly disabled color in the config has the strongest claim:
    // it may be parsing the output, and escape codes would break that,
    // for example.
    if (no_color)
        return false;

    if (env && 0 == strcmp(env, "1"))
        return true;

    if (env && 0 == strcmp(env, "0"))
        return false;

    // Otherwise only colorize when writing to a terminal; for any other stream
    // (a file, a pipe, or a non-file stream with no descriptor at all) the
    // escape codes would end up in the output verbatim
    color = isatty(fileno(stream));
    errno = saved_errno;
    return color;
#else
    (void)stream;
    (void)no_color;
    return false;
#endif
}


/*
 * Write a log record to ``stream`` in the standard format
 *
 * ``color`` is the final decision on whether to colorize; see
 * `asdf_log_use_color`.  The whole line is written under the stream's lock so
 * lines from different threads logging to the same stream do not interleave.
 */
static void asdf_log_write(
    const asdf_log_rec_t *rec, FILE *stream, asdf_log_fields_t fields, bool color) {
    bool sep = false;

    // Don't allow logging "nothing" (logging should simply be disabled for
    // that); if fields is empty enable all fields
    fields = fields ? fields : ASDF_LOG_FIELD_ALL;
#ifndef ASDF_LOG_COLOR
    color = false;
#endif

    flockfile(stream);

    if (fields & ASDF_LOG_FIELD_LEVEL) {
#ifdef ASDF_LOG_COLOR
        if (color)
            fprintf(
                stream,
                DIM("[") COLOR("%s", "%s") DIM("]"),
                level_colors[rec->level],
                level_names[rec->level]);
        else
#endif
            fprintf(stream, "[%-5s]", level_names[rec->level]);

        sep = true;
    }

    if (fields & (ASDF_LOG_FIELD_PACKAGE | ASDF_LOG_FIELD_FILE | ASDF_LOG_FIELD_LINE)) {
        if (sep)
            fputc(' ', stream);

        if (color)
            fputs(COLOR_DIM_GREY, stream);

        if (fields & ASDF_LOG_FIELD_PACKAGE)
            fprintf(stream, "(%s)", rec->package);

        if (fields & ASDF_LOG_FIELD_FILE)
            fprintf(stream, "%s:", rec->file);

        if (fields & ASDF_LOG_FIELD_LINE)
            fprintf(stream, "%d:", rec->lineno);

        if (color)
            fputs(ANSI_RESET, stream);

        sep = true;
    }

    if (fields & ASDF_LOG_FIELD_MSG) {
        if (sep)
            fputc(' ', stream);

        fputs(rec->msg, stream);
    }

    fputc('\n', stream);
    fflush(stream);
    funlockfile(stream);
}


void asdf_log_default_handler(const asdf_log_rec_t *rec, void *userdata) {
    const asdf_log_cfg_t *cfg = userdata;
    FILE *stream = (cfg && cfg->stream) ? cfg->stream : stderr;
    asdf_log_fields_t fields = cfg ? cfg->fields : ASDF_LOG_FIELD_ALL;
    int saved_errno = errno;

    if (UNLIKELY(!rec || rec->level < ASDF_LOG_TRACE || rec->level >= ASDF_LOG_NUM_LEVELS))
        return;

    // Called directly with an arbitrary config, so the color decision has to
    // be made here (the context's own config is resolved once up front and
    // does not come through this path; see asdf_log_emit)
    asdf_log_write(rec, stream, fields, asdf_log_use_color(stream, cfg ? cfg->no_color : false));
    errno = saved_errno;
}


/*
 * Format a log message and pass it to the handler of a resolved log config
 * (one built by `asdf_context_create`)
 *
 * The record and the formatted message live on the stack (the message moves to
 * the heap only if it does not fit in ASDF_LOG_MSG_BUF_SIZE), so nothing is
 * shared between calls: concurrent logging from different threads, and
 * handlers that themselves log, are both safe.
 */
static void asdf_log_emit(
    const asdf_log_cfg_t *cfg,
    asdf_log_level_t level,
    const char *file,
    int lineno,
    const char *fmt,
    va_list args) {
    // Logging must not clobber errno for callers that log before reporting it
    int saved_errno = errno;
    char buf[ASDF_LOG_MSG_BUF_SIZE];
    char *msg = buf;
    va_list args_copy;
    int len = 0;

    // Messages can only be logged at a real severity; this also keeps the
    // level_names lookup in bounds
    if (UNLIKELY(level < ASDF_LOG_TRACE || level >= ASDF_LOG_NUM_LEVELS))
        return;

    va_copy(args_copy, args);
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    len = vsnprintf(buf, sizeof(buf), fmt, args);

    if (UNLIKELY(len < 0)) {
        va_end(args_copy);
        errno = saved_errno;
        return;
    }

    if (UNLIKELY((size_t)len >= sizeof(buf))) {
        msg = malloc((size_t)len + 1);

        // On allocation failure log the truncated message rather than nothing
        if (msg)
            vsnprintf(msg, (size_t)len + 1, fmt, args_copy);
        else
            msg = buf;
    }

    va_end(args_copy);

    asdf_log_rec_t rec = {
        .level = level, .package = PACKAGE_NAME, .file = file, .lineno = lineno, .msg = msg};

    /* The default handler isn't called directly as it wraps behavior alread
     * covered here like getting the color decision and saving errno; it's
     * supplied rather so that users can wrap the default handler if they wish
     * to */
    if (cfg->handler == asdf_log_default_handler)
        asdf_log_write(&rec, cfg->stream, cfg->fields, !cfg->no_color);
    else
        cfg->handler(&rec, cfg->handler_userdata);

    if (msg != buf)
        free(msg);

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
    asdf_log_emit(&ctx->log, level, file, lineno, fmt, args);
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
    asdf_log_emit(&ctx->log, level, src_file, lineno, fmt, args);
    va_end(args);
}


void asdf_log_fallback(asdf_log_level_t level, const char *file, int lineno, const char *fmt, ...) {
    // Bug in clang-tidy: https://github.com/llvm/llvm-project/issues/40656
    // Should be fixed in newer versions though...
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    asdf_log_cfg_t cfg = {
        .stream = stderr,
        .fields = ASDF_LOG_FIELD_ALL,
        .no_color = !asdf_log_use_color(stderr, false),
        .handler = asdf_log_default_handler};
    va_list args;
    va_start(args, fmt);
    asdf_log_emit(&cfg, level, file, lineno, fmt, args);
    va_end(args);
}
