/**
 * .. _asdf/error.h:
 *
 * Public error codes for the libasdf error-handling API, and internal error
 * reporting macros.
 */

//

#ifndef ASDF_ERROR_H
#define ASDF_ERROR_H

#include <asdf/util.h>

ASDF_BEGIN_DECLS

/**
 * Error codes
 * -----------
 */

/**
 * Error codes set on an `asdf_file_t` or other context.
 *
 * Retrieve with `asdf_error_code`.
 * When the code is `ASDF_ERR_SYSTEM`, the original OS ``errno`` value is
 * available via `asdf_error_errno`.
 */
typedef enum {
    /** No error */
    ASDF_ERR_NONE = 0,
    /** Unknown parser state */
    ASDF_ERR_UNKNOWN_STATE,
    /** Stream initialization failed */
    ASDF_ERR_STREAM_INIT_FAILED,
    /** Attempted write to a read-only stream or file */
    ASDF_ERR_STREAM_READ_ONLY,
    /** Invalid ASDF file header */
    ASDF_ERR_INVALID_ASDF_HEADER,
    /** Unexpected end of file */
    ASDF_ERR_UNEXPECTED_EOF,
    /** Invalid block header */
    ASDF_ERR_INVALID_BLOCK_HEADER,
    /** Block magic bytes did not match */
    ASDF_ERR_BLOCK_MAGIC_MISMATCH,
    /** YAML parser initialization failed */
    ASDF_ERR_YAML_PARSER_INIT_FAILED,
    /** YAML parsing failed */
    ASDF_ERR_YAML_PARSE_FAILED,
    /** Out of memory */
    ASDF_ERR_OUT_OF_MEMORY,
    /** OS-level error; see `asdf_error_errno` for the original ``errno`` */
    ASDF_ERR_SYSTEM,
    /** Invalid argument
     *
     * When set with `ASDF_ERROR_COMMON` takes two parameters:
     *
     * - ``char *`` - the name of the function argument passed an invalid value
     * - ``char *`` - a string representation of the invalid value passed
     */
    ASDF_ERR_INVALID_ARGUMENT,
    /** Unknown compression type
     *
     * When set with `ASDF_ERROR_COMMON` takes one parameter:
     *
     * - ``char *`` - the unknown compression type string, NULL-terminated
     */
    ASDF_ERR_UNKNOWN_COMPRESSION,
    /** Compression or decompression error
     *
     * When set with `ASDF_ERROR_COMMON` takes one parameter:
     *
     * - ``char *`` - additional compression/decompression error details,
     *   typically passed from the underlying compression library or
     *   compressor extension.
     */
    ASDF_ERR_COMPRESSION_FAILED,
    /**
     * No serializer registered for extension
     *
     * When set with `ASDF_ERROR_COMMON` takes one parameter:
     *
     * - ``char *`` - the key naming the extension (typically a YAML tag)
     */
    ASDF_ERR_EXTENSION_NOT_FOUND,
    /**
     * A system limit has been reached
     *
     * When set with `ASDF_ERROR_COMMON` takes one parameter:
     *
     * - ``char *`` - additional details of the limit that that was surpassed
     */
    ASDF_ERR_OVER_LIMIT
} asdf_error_code_t;

/**
 * Reporting errors
 * ----------------
 *
 * The following macros are intended for setting internal error states in
 * libasdf.  Normally these are meant for internal use, but are exposed in
 * the public API for use by extension authors as well. Application code
 * (outside libasdf) should not use these, as they set error codes in libasdf
 * itself.
 *
 * Each of these macros also causes a log message to be emitted at
 * `ASDF_LOG_ERROR` or greater if compiled with ``ASDF_LOG_ENABLED``.
 */

// Forward declarations
typedef struct asdf_file asdf_file_t;
typedef struct asdf_value asdf_value_t;

// clang-format off

/**
 * Set an error with a code from `asdf_error_code_t`; optional variadic args
 * are the format parameters for the per-code format string, which depends
 * on the error code (some do not take any parameters).
 *
 * .. note::
 *
 *   These format strings are defined in the source at ``src/error.c``.  The
 *   per-code parameters are noted in the per-code documentation at
 *   `asdf_error_code_t` as best as possible, though refer to the source as
 *   the final arbiter.
 *
 * :param obj: The libasdf handle in which to record the error: an
 *   `asdf_file_t *`, or an `asdf_value_t *` (which records against its file).
 * :param code: The `asdf_error_code_t` to set
 * :param ...: Error code-specific additional parameters
 */
#define ASDF_ERROR_COMMON(obj, code, ...) _Generic((obj), \
    asdf_file_t *: asdf_file_error_common, \
    asdf_value_t *: asdf_value_error_common \
    )((obj), (code), __FILE__, __LINE__, ##__VA_ARGS__)


/**
 * Special case for reporting a memory error (e.g. failed allocation)
 *
 * This is roughly equivalent to setting
 * ``ASDF_ERROR_COMMON(obj, ASDF_ERR_OUT_OF_MEMORY)`` with some extra
 * precautions taken; use this as a shortcut for allocation failure error
 * paths.
 *
 * :param obj: The libasdf handle in which to record the error: an
 *   `asdf_file_t *`, or an `asdf_value_t *` (which records against its file).
 */
#define ASDF_ERROR_OOM(obj) _Generic((obj), \
    asdf_file_t *: asdf_file_error_oom, \
    asdf_value_t *: asdf_value_error_oom \
    )((obj), __FILE__, __LINE__)

/**
 * Set a system (OS) error from an errno value
 *
 * This sets an error in libasdf but with an error message derived from
 * ``strerror``.
 *
 * :param obj: The libasdf handle in which to record the error: an
 *   `asdf_file_t *`, or an `asdf_value_t *` (which records against its file).
 * :param errnum: An error, e.g. from a system call, retrieved from ``errno``
 */
#define ASDF_ERROR_SYSTEM(obj, errnum) \
    _Generic( \
        (obj), asdf_file_t *: asdf_file_error_system, asdf_value_t *: asdf_value_error_system)( \
        (obj), (errnum), __FILE__, __LINE__)

// clang-format on

// The following symbols must be exported for the above macros, but are
// intentionally left undocumented.

ASDF_EXPORT void asdf_file_error_common(
    asdf_file_t *file, asdf_error_code_t code, const char *src_file, int lineno, ...);
ASDF_EXPORT void asdf_value_error_common(
    asdf_value_t *file, asdf_error_code_t code, const char *src_file, int lineno, ...);
ASDF_EXPORT void asdf_file_error_oom(asdf_file_t *file, const char *src_file, int lineno);
ASDF_EXPORT void asdf_value_error_oom(asdf_value_t *file, const char *src_file, int lineno);
ASDF_EXPORT void asdf_file_error_system(
    asdf_file_t *value, int errnum, const char *src_file, int lineno);
ASDF_EXPORT void asdf_value_error_system(
    asdf_value_t *value, int errnum, const char *src_file, int lineno);

ASDF_END_DECLS

#endif /* ASDF_ERROR_H */
