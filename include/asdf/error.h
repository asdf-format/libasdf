#ifndef ASDF_ERROR_H
#define ASDF_ERROR_H

/**
 * .. _asdf/error.h:
 *
 * Public error codes for the libasdf error-handling API.
 */

#include <asdf/util.h>

ASDF_BEGIN_DECLS

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
    /** Invalid argument */
    ASDF_ERR_INVALID_ARGUMENT,
    /** Unknown compression type */
    ASDF_ERR_UNKNOWN_COMPRESSION,
    /** Compression or decompression error */
    ASDF_ERR_COMPRESSION_FAILED,
    /** No serializer registered for extension */
    ASDF_ERR_EXTENSION_NOT_FOUND,
    /** A system limit has been reached */
    ASDF_ERR_OVER_LIMIT
} asdf_error_code_t;

ASDF_END_DECLS

#endif /* ASDF_ERROR_H */
