/**
 * .. _asdf/log.h:
 *
 * Logging configuration and the logging API.
 *
 * libasdf emits diagnostic log messages associated with each open file.  Every
 * `asdf_file_t` carries a logging configuration (an `asdf_log_cfg_t`), which can
 * be provided up front through the ``log`` field of `asdf_config_t` when opening
 * a file with `asdf_open_ex`.  Messages at or above the configured
 * `asdf_log_level_t` are written to the configured stream (``stderr`` by
 * default), with a configurable set of prefix fields.  The stream is flushed
 * after every message.
 *
 * If no level is set explicitly the default is taken from the ``ASDF_LOG_LEVEL``
 * environment variable (one of ``NONE``, ``TRACE``, ``DEBUG``, ``INFO``,
 * ``WARN``, ``ERROR``, or ``FATAL``, case-insensitive), falling back to
 * ``WARN``.
 *
 * When the build supports color, output is colorized only if the stream is a
 * terminal and `asdf_log_cfg_t.no_color` is not set.  The ``ASDF_LOG_COLOR``
 * environment variable overrides the terminal check: ``ASDF_LOG_COLOR=1``
 * forces color on (even when the stream is not a terminal) and
 * ``ASDF_LOG_COLOR=0`` forces it off; any other value is ignored.  It does not
 * override `asdf_log_cfg_t.no_color`, so an application that disables color
 * always gets uncolored output.  Builds without color support ignore it.  The
 * environment is read once, when a file is opened.
 *
 * The library's own internal log statements are compiled in only when libasdf
 * is built with logging enabled (the default; disable with ``-DENABLE_LOG=NO``
 * under CMake or ``--disable-logging`` under the Autotools build).  The public
 * `asdf_file_log` function and `ASDF_LOG` macro are provided for extension
 * authors who wish to emit messages through the same configuration; see
 * :ref:`extensions`.
 */

//

#ifndef ASDF_LOG_H
#define ASDF_LOG_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <asdf/util.h>

/**
 * Severity levels for log messages, in increasing order of severity
 *
 * A configured level acts as a threshold: only messages at that level or higher
 * are emitted.  `ASDF_LOG_NONE` is higher than every message severity, so as a
 * threshold it disables logging entirely.
 */
typedef enum {
    /**
     * Use the default level: the value of the ``ASDF_LOG_LEVEL`` environment
     * variable if set, otherwise ``ASDF_LOG_DEFAULT_LEVEL`` (``ASDF_LOG_WARN``
     * unless libasdf was built with a different default)
     *
     * This is the zero value, so it is what a zero-initialized
     * `asdf_log_cfg_t` selects.  It is not a message severity.
     */
    ASDF_LOG_DEFAULT = 0,
    /** Very fine-grained tracing messages */
    ASDF_LOG_TRACE,
    /** Debugging messages */
    ASDF_LOG_DEBUG,
    /** Informational messages */
    ASDF_LOG_INFO,
    /** Warnings about recoverable problems */
    ASDF_LOG_WARN,
    /** Errors */
    ASDF_LOG_ERROR,
    /** Fatal, unrecoverable errors */
    ASDF_LOG_FATAL,
    /** Emit no messages; only meaningful as a threshold, not a severity */
    ASDF_LOG_NONE,
} asdf_log_level_t;

/**
 * One past the highest message severity (`ASDF_LOG_FATAL`)
 *
 * Message severities are the values from `ASDF_LOG_TRACE` up to, but not
 * including, this.  It does not count `ASDF_LOG_NONE`, which is a threshold
 * only.
 */
#define ASDF_LOG_NUM_LEVELS (ASDF_LOG_FATAL + 1)


#define ASDF_LOG_FIELDS(X) \
    X(ASDF_LOG_FIELD_LEVEL, 0) \
    X(ASDF_LOG_FIELD_PACKAGE, 1) \
    X(ASDF_LOG_FIELD_FILE, 2) \
    X(ASDF_LOG_FIELD_LINE, 3) \
    X(ASDF_LOG_FIELD_MSG, 4)


/**
 * Bitmask selecting every available log field
 *
 * See `asdf_log_field_t`.
 */
#define ASDF_LOG_FIELD_ALL \
    (ASDF_LOG_FIELD_LEVEL | ASDF_LOG_FIELD_PACKAGE | ASDF_LOG_FIELD_FILE | ASDF_LOG_FIELD_LINE | \
     ASDF_LOG_FIELD_MSG)


/**
 * Flags selecting which fields the standard log formatter includes in each
 * line
 *
 * Combine these as a bitmask in `asdf_log_cfg_t.fields`.  The available flags
 * are ``ASDF_LOG_FIELD_LEVEL`` (the severity), ``ASDF_LOG_FIELD_PACKAGE`` (the
 * originating package/library), ``ASDF_LOG_FIELD_FILE`` and
 * ``ASDF_LOG_FIELD_LINE`` (the source location), and ``ASDF_LOG_FIELD_MSG``
 * (the message text itself).  `ASDF_LOG_FIELD_ALL` selects them all.
 *
 * Log formatting is not otherwise customizable yet.
 */
typedef enum {
// clang-format off
#define X(flag, bit) flag = (1ULL << (bit)),
    ASDF_LOG_FIELDS(X)
#undef X
    // clang-format on
} asdf_log_field_t;


/** A bitmask of `asdf_log_field_t` flags */
typedef uint64_t asdf_log_fields_t;


/**
 * Struct representing a single log message
 *
 * This is how log messages are represented by the logging system before
 * formatting them to lines to an I/O stream.  Applications wishing to
 * capture libasdf log messages and reformat them entirely, or forward them
 * to their own logging infrastructure can assign an `asdf_log_handler_t`
 * to `asdf_log_cfg_t.handler`.
 *
 * .. version-added:: 0.3
 */
typedef struct {
    /** The log level of the message */
    asdf_log_level_t level;
    /**
     * A string representing the software package that produced the message
     *
     * Currently this always just carries "libasdf", but in the future it
     * may be allowed to carry something more specific, like an extension
     * name, etc. Same as the value in the ``ASDF_LOG_FIELD_PACKAGE`` field
     * of the default handler.
     */
    const char *package;
    /** Name of the source file from which the log message emanated */
    const char *file;
    /** Line number in ``file`` from which the log message emanated */
    int lineno;
    /** The log message, formatted (without a trailing newline) */
    const char *msg;
} asdf_log_rec_t;


/**
 * Type for optional structured logging callback functions
 *
 * See `asdf_log_rec_t` and `asdf_log_cfg_t.handler`.  The handler is only
 * called for messages at or above the configured `asdf_log_cfg_t.level`.
 *
 * A handler may itself log through libasdf.  It may be called concurrently
 * from different threads if files sharing it are used from different threads,
 * in which case it must be thread-safe.  libasdf preserves ``errno`` across
 * the call, so the handler need not.
 *
 * The `asdf_log_rec_t *` for a single log message.  It is owned by
 * libasdf and is only valid for the duration of the call; copy anything
 * (including ``rec->msg``) that must outlive it.
 *
 * The custom user data, if any, assigned to the
 * `asdf_log_cfg_t.handler_userdata` that configured this log handler
 *
 * .. version-added:: 0.3
 */
typedef void (*asdf_log_handler_t)(const asdf_log_rec_t *rec, void *userdata);


/**
 * Per-file logging configuration
 *
 * Pass one of these as the ``log`` field of `asdf_config_t` to `asdf_open_ex`
 * to control logging for a file.  Any field left zero-initialized is filled in
 * with a default: the stream defaults to ``stderr``, the level to the value of
 * the ``ASDF_LOG_LEVEL`` environment variable (or ``ASDF_LOG_WARN``), and the
 * fields to `ASDF_LOG_FIELD_ALL`.  Set ``level`` to `ASDF_LOG_NONE` to disable
 * logging for the file.
 */
typedef struct {
    /** Destination stream for log output (defaults to ``stderr``) */
    FILE *stream;
    /** Minimum severity to emit; see `asdf_log_level_t` */
    asdf_log_level_t level;
    /**
     * Basic configuration of what fields are included in the standard log
     * formatter
     *
     * Log formatting is not fully customizable yet but specific fields may be
     * enabled / disabled; see `asdf_log_field_t`.
     */
    asdf_log_fields_t fields;
    /**
     * Disable colorized output even when the build supports it and ``stream``
     * is a terminal (output to any other stream is not colorized by default);
     * takes precedence over the ``ASDF_LOG_COLOR`` environment variable
     */
    bool no_color;
    /**
     * Optional callback to intercept log records and implement custom logging
     *
     * The handler receives an `const asdf_log_rec_t *` as well as a pointer
     * to the optional userdata declared below, and returns ``void``.
     *
     * If left as ``NULL`` (the default) libasdf's standard stream-based
     * logging is used (`asdf_log_default_handler`).
     *
     * .. version-added:: 0.3
     */
    asdf_log_handler_t handler;

    /**
     * Optional user-specified data passed to the `asdf_log_handler_t`
     *
     * This field is only meaningful if `asdf_log_cfg_t.handler` is passed a
     * non-default, non-NULL value and is otherwise ignored (the default
     * handler is always passed the effective log configuration instead).
     *
     * .. version-added:: 0.3
     */
    void *handler_userdata;
    /** Reserved for future options (leave zeroed); keeps the ABI stable */
    void *_reserved[4];
} asdf_log_cfg_t;


/* Forward declaration — full definition in <asdf/file.h> */
typedef struct asdf_file asdf_file_t;


ASDF_BEGIN_DECLS

/**
 * Emit a log message associated with a file's logging configuration
 *
 * This is the public logging entry point for extension authors.  The message
 * is formatted like ``printf`` and emitted only if ``level`` meets the
 * threshold configured for ``file``.  In most cases the `ASDF_LOG` macro is
 * more convenient, as it fills in the source file and line automatically.
 *
 * :param file: The `asdf_file_t *` whose log configuration to use; obtain it
 *   from an `asdf_value_t` with `asdf_value_file` if needed
 * :param level: The severity of the message; see `asdf_log_level_t`
 * :param src_file: Source file name to report (e.g. ``__FILE__``)
 * :param lineno: Source line number to report (e.g. ``__LINE__``)
 * :param fmt: A ``printf``-style format string
 * :param ...: Arguments for ``fmt``
 */
ASDF_EXPORT void asdf_file_log(
    const asdf_file_t *file,
    asdf_log_level_t level,
    const char *src_file,
    int lineno,
    const char *fmt,
    ...);

/**
 * Convenience wrapper around `asdf_file_log` that supplies ``__FILE__`` and
 * ``__LINE__`` automatically
 *
 * Expands to nothing unless libasdf is built with logging enabled (the
 * ``ASDF_LOG_ENABLED`` macro defined), so log statements can be left in place
 * with no overhead in a non-logging build.
 *
 * :param file: The `asdf_file_t *` whose log configuration to use
 * :param level: The severity of the message; see `asdf_log_level_t`
 * :param ...: A ``printf``-style format string followed by its arguments
 */
#ifdef ASDF_LOG_ENABLED
#define ASDF_LOG(file, level, ...) asdf_file_log((file), (level), __FILE__, __LINE__, __VA_ARGS__)
#else
#define ASDF_LOG(file, level, ...) ((void)0)
#endif


/**
 * The default stream-based log handler used for logging
 *
 * This is the log handler used when `asdf_log_cfg_t.handler` is not
 * overridden, and one may assign it explicitly e.g. to restore the default.
 *
 * When libasdf uses it for a file, its ``userdata`` is always that file's
 * effective log configuration.  It may also be called directly, for example
 * from a custom handler that adds to the standard output rather than replacing
 * it.  In that case the stream, fields and ``no_color`` are taken from the
 * given configuration (with the same defaults as for a file), and whether to
 * colorize is decided the same way as for a file.
 *
 * :param rec: The `asdf_log_rec_t *` for a single log message
 * :param userdata: The `asdf_log_cfg_t *` to format and write the message by,
 *   or ``NULL`` for all defaults
 *
 * .. version-added:: 0.3
 */
ASDF_EXPORT void asdf_log_default_handler(const asdf_log_rec_t *rec, void *userdata);

ASDF_END_DECLS


#endif /* ASDF_LOG_H */
