/**
 * .. _asdf/file.h:
 *
 * This is the high-level public API for working with ASDF files.  It includes
 * functions for opening and closing ASDF file handles, represented by
 * `asdf_file_t` pointers.
 *
 * Most of these functions work on an open `asdf_file_t *` as their first
 * argument, and retrieve scalar values and more complex objects out of the
 * ASDF tree.
 */

//

#ifndef ASDF_FILE_H
#define ASDF_FILE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <asdf/emitter.h>
#include <asdf/parser.h>
#include <asdf/util.h>
#include <asdf/value.h>

ASDF_BEGIN_DECLS

/**
 * An opaque struct representing an open ASDF file handle
 *
 * Pointers to `asdf_file_t` are the primary interface to each open ASDF file
 * and can be created and allocated with `asdf_open`, `asdf_open_file`,
 * `asdf_open_fp`, or `asdf_open_mem`.
 */
typedef struct asdf_file asdf_file_t;


/**
 * .. _int file-configuration:
 *
 * Configuration
 * -------------
 */


/**
 * Options for decompression mode, for use with
 * :c:type:`asdf_config_t`
 *
 * .. todo::
 *
 *   Document modes
 *
 * .. todo::
 *
 *   When lazy is implemented there may likely be multiple implementations
 *   (userfaultfd, sigsegv, etc.).  Add options to specify exactly which
 *   implementation to use, where ASDF_DECOMP_MODE_LAZY by itself will
 *   choose the most appropriate choice (generally userfaultfd if available)
 */
typedef enum {
    /** Automatically select the best mode */
    ASDF_BLOCK_DECOMP_MODE_AUTO = 0,
    /** Force eager decompression */
    ASDF_BLOCK_DECOMP_MODE_EAGER,
    /**
     * Force lazy decompression *if possible*
     *
     * Lazy decompression is currently only implemented on recent-enough Linux
     * versions (4.11+) that support the userfaultfd system call.  If this
     * option is passed on a system where it is not supported it will
     * fall back to eager decompression.
     */
    ASDF_BLOCK_DECOMP_MODE_LAZY,
} asdf_block_decomp_mode_t;


/**
 * Struct containing extended options to use when opening and reading files
 *
 * For use with `asdf_open_ex` and relatives.
 */
typedef struct {
    /** Low-level parser configuration; see `asdf_parser_cfg_t` */
    asdf_parser_cfg_t parser;

    /** Low-level emitter configuration; see `asdf_emitter_cfg_t` */
    asdf_emitter_cfg_t emitter;

    /** Decompression options */
    struct {
        /** Decompression mode (see `asdf_block_decomp_mode_t`) */
        asdf_block_decomp_mode_t mode;

        /**
         * Max size in bytes of the decompressed data, above which
         * decompression to disk will be used (see :ref:`compression`)
         */
        size_t max_memory_bytes;

        /**
         * Max percentage (from ``0.0`` to ``1.0`` of total system memory
         * above which decompression to disk will be used
         * (see :ref:`compression`)
         */
        double max_memory_threshold;

        /**
         * Size in bytes of chunks to decompress at a time when using lazy
         * decompression
         *
         * Defaults to one page, and is always rounded up to the nearest page
         * size.
         */
        size_t chunk_size;

        /**
         * Optional temporary directory path to use when decompressing to disk
         */
        const char *tmp_dir;
    } decomp;
} asdf_config_t;


// Forward-declarations for asdf_open_ex and so on
asdf_file_t *asdf_open_file_ex(const char *filename, const char *mode, asdf_config_t *config);
asdf_file_t *asdf_open_fp_ex(FILE *fp, const char *filename, asdf_config_t *config);
asdf_file_t *asdf_open_mem_ex(const void *buf, size_t size, asdf_config_t *config);

/**
 * .. _file-openers:
 *
 * File openers
 * ------------
 */

/**
 * Opens an ASDF file for reading
 *
 * In fact this is just a convenience alias for `asdf_open_file`.
 *
 * :param filename: A null-terminated string containing the local filesystem
 *   path to open
 * :param mode: Currently must always be just ``"r"``.  This will support other
 *   opening modes in the future (e.g. for writes, updates).
 * :return: An `asdf_file_t *`
 */
static inline asdf_file_t *asdf_open(const char *filename, const char *mode) {
    return asdf_open_file_ex(filename, mode, NULL);
}

/**
 * Opens an ASDF file for reading
 *
 * Equivalent to `asdf_open`.
 */
static inline asdf_file_t *asdf_open_file(const char *filename, const char *mode) {
    return asdf_open_file_ex(filename, mode, NULL);
}

/**
 * Opens an ASDF file from an already open `FILE *`
 *
 * This assumes the file is open for reading.
 *
 * :param fp: An open `FILE *`
 * :param filename: An optional filename for the open file.
 *   This need not be a real filesystem path, and can be any display name for
 *   the file; used mainly in error messages.
 * :return: An `asdf_file_t *`
 */
static inline asdf_file_t *asdf_open_fp(FILE *fp, const char *filename) {
    return asdf_open_fp_ex(fp, filename, NULL);
}

/**
 * Opens an ASDF file from an memory buffer
 *
 * :param buf: An arbitrary block of memory from a `void *`
 * :param size: The size of the memory buffer
 * :return: An `asdf_file_t *`
 */
static inline asdf_file_t *asdf_open_mem(const void *buf, size_t size) {
    return asdf_open_mem_ex(buf, size, NULL);
}


/**
 * Flush changes to disk if the file is open for writing
 *
 * .. todo::
 *
 *   Clarify error handling on flush.
 *
 * :param file: The `asdf_file_t *` to close
 * :return: 0 on success or a negative value if the file is not open for
 *   writing or another error occurred on write.
 */
ASDF_EXPORT int asdf_flush(asdf_file_t *file);


/**
 * Closes an open `asdf_file_t *`, freeing associated resources where possible
 *
 * Any other resources associated with that file handle, such as ndarrays,
 * should no longer be expected to work and should ideally be freed before
 * closing the file.
 *
 * If the file was open for writing, also attempts to call `asdf_flush` to
 * ensure any unwriten changes are written.
 *
 * :param file: The `asdf_file_t *` to close
 */
ASDF_EXPORT void asdf_close(asdf_file_t *file);


/**
 * Opens an ASDF file for reading
 *
 * Extended version of `asdf_open` taking an optional pointer to
 * :c:type:`asdf_config_t` configuration options, or `NULL` to
 * use the default options (equivalent to `asdf_open`).
 *
 * When passing in an `asdf_config_t *`, the config struct is *copied*:
 *
 * * This allows passing in the options from a local variable
 * * Prevents modifications of the options while the file is open
 * * In many cases you can leave options set to zero, and they will be filled
 *   in with defaults.
 *
 * This is an alias for `asdf_open_file_ex`.
 *
 * :param filename: A null-terminated string containing the local filesystem
 *   path to open
 * :param mode: Currently must always be just ``"r"``.  This will support other
 *   opening modes in the future (e.g. for writes, updates).
 * :param config: A pointer to an `asdf_config_t` (may be partially initialized)
 * :return: An `asdf_file_t *`
 */
static inline asdf_file_t *asdf_open_ex(
    const char *filename, const char *mode, asdf_config_t *config) {
    return asdf_open_file_ex(filename, mode, config);
}

/**
 * Opens an ASDF file for reading, with optional extended options
 *
 * Equivalent to `asdf_open`.
 */
ASDF_EXPORT asdf_file_t *asdf_open_file_ex(
    const char *filename, const char *mode, asdf_config_t *config);

/**
 * Opens an ASDF file from an already open `FILE *`, with optional extended
 * options
 *
 * This assumes the file is open for reading.
 *
 * :param fp: An open `FILE *`
 * :param filename: An optional filename for the open file.
 *   This need not be a real filesystem path, and can be any display name for
 *   the file; used mainly in error messages.
 * :param config: A pointer to an `asdf_config_t` (may be partially initialized)
 * :return: An `asdf_file_t *`
 */
ASDF_EXPORT asdf_file_t *asdf_open_fp_ex(FILE *fp, const char *filename, asdf_config_t *config);

/**
 * Opens an ASDF file from an memory buffer, with optional extended options
 *
 * :param buf: An arbitrary block of memory from a `void *`
 * :param size: The size of the memory buffer
 * :param config: A pointer to an `asdf_config_t` (may be partially initialized)
 * :return: An `asdf_file_t *`
 */
ASDF_EXPORT asdf_file_t *asdf_open_mem_ex(const void *buf, size_t size, asdf_config_t *config);


/**
 * .. _file-errors:
 *
 * Error handling
 * --------------
 */


/**
 * Retrieve an error on a file
 *
 * This is typically used to check for errors on the file itself, such as
 * parse errors, and not for user data errors (such as invalid type conversions
 * on an `asdf_value_t`).
 *
 * If passed `NULL`, returns any global error, if set (typically from errors
 * opening a file), or from library initialization.
 *
 * See the section on :ref:`error-handling` for more details.
 *
 * :param file: An open `asdf_file_t *` or `NULL`
 * :return: `NULL` if there is no error set, otherwise a pointer to the error
 *   message string
 */
ASDF_EXPORT const char *asdf_error(asdf_file_t *file);


/**
 * .. _file-value-getters:
 *
 * Reading values
 * --------------
 */

/**
 * The following functions are the high-level interface for retrieving typed
 * values out of the ASDF metadata tree.  These include plain scalar values,
 * mappings, sequences, as tagged data structures that have a registered
 * extension for handling them (this includes objects belonging to the ASDF
 * core schema, such as ``core/history_entry`` or ``core/ndarray``). The
 * getters for schema-specific objects are not documented here, but follow
 * the same patterns.
 *
 * For each type that can be read out of the ASDF tree there is an
 * ``asdf_is_<type>`` function which just checks the type and returns a `bool`.
 * Then there is an ``asdf_get_<type>`` function.  Each of these takes the
 * `asdf_file_t *` as their first argument, then a JSON Path expression for the
 * path within the tree to that value, and finally a pointer for the return
 * value's type.  Each of these functions return their value by reference
 * through an input argument.  The return value is always `asdf_value_err_t`.
 *
 * If the value exists and successfully converts to the requested type the
 * return value is `ASDF_VALUE_OK`.  There are other return values such as
 * `ASDF_VALUE_ERR_NOT_FOUND` (the path simply does not exist) or
 * `ASDF_VALUE_ERR_TYPE_MISMATCH` (a value exists at that path but is the wrong
 * type).  A few other more obscure errors can occur--see `asdf_value_err_t`.
 *
 * The one exception to the above is `asdf_get_value` which simply returns the
 * generic `asdf_value_t *` if the path exists, or `NULL` otherwise.  See
 * :ref:`asdf-value` for more details on generic values.
 *
 * .. todo::
 *
 *   Add support for referencing ASDF schemas.
 *
 * .. todo::
 *
 *   Link documentation for JSON Path
 */

/**
 * Get an arbitrary `asdf_value_t *` out of the tree
 *
 * :param file: The `asdf_file_t *` for the file
 * :param path: The JSON Path to the value
 * :return: An `asdf_value_t *` wrapping the value, or `NULL` if the path does
 *   not exist in the tree
 */
ASDF_EXPORT asdf_value_t *asdf_get_value(asdf_file_t *file, const char *path);

/**
 * Check if the value at the given tree path is a YAML mapping
 *
 * :param file: The `asdf_file_t *` for the file
 * :param path: The JSON Path to the value
 * :return: `true` if the value is a mapping, `false` if it is another
 *   type of value or if no value exists at that path.
 */
ASDF_EXPORT bool asdf_is_mapping(asdf_file_t *file, const char *path);

/**
 * Get a mapping out of the ASDF tree
 *
 * .. note::
 *
 *   Mappings are currently represented as generic `asdf_value_t *`, though if
 *   this function returns `ASDF_VALUE_OK` it is guaranteed to be a mapping.
 *   This function will also ignore tags, so that tagged objects like
 *   ``core/ndarray`` can be read as a raw YAML mapping.
 *
 * .. todo::
 *
 *   In the future may add a dedicated typedef for mappings to make this more
 *   explicit.
 *
 * :param file: The `asdf_file_t *` for the file
 * :param path: The JSON Path to the mapping
 * :param value: An `asdf_value_t **` into which to return the mapping
 * :return: `ASDF_VALUE_OK` if the value exists and is a mapping, otherwise
 *   `ASDF_VALUE_ERR_NOT_FOUND` or `ASDF_VALUE_ERR_TYPE_MISMATCH`.
 */
ASDF_EXPORT asdf_value_err_t
asdf_get_mapping(asdf_file_t *file, const char *path, asdf_value_t **value);

/**
 * Check if the value at the given tree path is a YAML sequence
 *
 * :param file: The `asdf_file_t *` for the file
 * :param path: The JSON Path to the value
 * :return: `true` if the value is a sequence, `false` if it is another
 *   type of value or if no value exists at that path.
 */
ASDF_EXPORT bool asdf_is_sequence(asdf_file_t *file, const char *path);

/**
 * Get a sequence out of the ASDF tree
 *
 * .. note::
 *
 *   Sequences are currently represented as generic `asdf_value_t *`, though if
 *   this function returns `ASDF_VALUE_OK` it is guaranteed to be a sequence.
 *   Like `asdf_get_mapping`, this function will ignore tags, so that tagged
 *   sequences associated with an extension schema can be read as a raw YAML
 *   sequence.
 *
 * .. todo::
 *
 *   In the future may add a dedicated typedef for sequences to make this more
 *   explicit.
 *
 * :param file: The `asdf_file_t *` for the file
 * :param path: The JSON Path to the sequence
 * :param value: An `asdf_value_t **` into which to return the sequence
 * :return: `ASDF_VALUE_OK` if the value exists and is a sequence, otherwise
 *   `ASDF_VALUE_ERR_NOT_FOUND` or `ASDF_VALUE_ERR_TYPE_MISMATCH`.
 */
ASDF_EXPORT asdf_value_err_t
asdf_get_sequence(asdf_file_t *file, const char *path, asdf_value_t **value);

/**
 * Check if the value at the given tree path is a string scalar
 *
 * .. note::
 *
 *   libasdf adheres to the `YAML Core Schema`_ in the interpretation of scalar
 *   values.  So here "is a string" means strictly not interpreted as any other
 *   data type (int, bool, etc.) under the YAML.  This is the same convention
 *   used in many other programming languages like Python, etc.
 *
 *   To check if the value is simply a scalar of any type use `asdf_is_scalar`.
 *
 *
 * :param file: The `asdf_file_t *` for the file
 * :param path: The JSON Path to the string
 * :return: `true` if the value is a string, `false` if it is another
 *   type of value or if no value exists at that path.
 */
ASDF_EXPORT bool asdf_is_string(asdf_file_t *file, const char *path);

/**
 * Get a string out of the ASDF tree
 *
 * This version returns the string without a null terminator, and the length of
 * the string into the ``out_len`` parameter.  This employs zero-copy where
 * possible, so the memory pointing to the string may become unusable once the
 * file is closed.
 *
 * .. note::
 *
 *   See the note about `asdf_is_string`.  This only returns `ASDF_VALUE_OK` if
 *   the value exists and is strictly a string.  For a more generic version
 *   that returns the raw text of a scalar see `asdf_get_scalar`.
 *
 * :param file: The `asdf_file_t *` for the file
 * :param path: The JSON Path to the string
 * :param out: A `const char **` into which to return the string as a `const char *`
 * :param out_len: A `size_t *` into which to return the length of the string
 * :return: `ASDF_VALUE_OK` if the value exists and is a string, otherwise
 *   `ASDF_VALUE_ERR_NOT_FOUND` or `ASDF_VALUE_ERR_TYPE_MISMATCH`.
 */
ASDF_EXPORT asdf_value_err_t
asdf_get_string(asdf_file_t *file, const char *path, const char **out, size_t *out_len);

/**
 * Get a null-terminated string out of the ASDF tree
 *
 * Like `asdf_get_string` but returns a null-terminated copy of the string.
 *
 * :param file: The `asdf_file_t *` for the file
 * :param path: The JSON Path to the string
 * :param out: A `const char **` into which to return the string as a `const char *`
 * :return: `ASDF_VALUE_OK` if the value exists and is a string, otherwise
 *   `ASDF_VALUE_ERR_NOT_FOUND` or `ASDF_VALUE_ERR_TYPE_MISMATCH`.
 */
ASDF_EXPORT asdf_value_err_t
asdf_get_string0(asdf_file_t *file, const char *path, const char **out);

/**
 * Check if the value at the given tree path is a YAML scalar of any kind
 *
 * :param file: The `asdf_file_t *` for the file
 * :param path: The JSON Path to the value
 * :return: `true` if the value is a scalar, `false` if it is another
 *   type of value or if no value exists at that path.
 */
ASDF_EXPORT bool asdf_is_scalar(asdf_file_t *file, const char *path);

/**
 * Like `asdf_get_string` but returns the raw text of a scalar value as a
 * string without interpretation under the `YAML Core Schema`_.
 *
 * This can be especially useful in the implementation of :ref:`extensions`
 * to process tagged scalars.
 *
 * :param file: The `asdf_file_t *` for the file
 * :param path: The JSON Path to the scalar
 * :param out: A `const char **` into which to return the scalar as a
 *   `const char *`
 * :param out_len: A `size_t *` into which to return the length of the scalar
 * :return: `ASDF_VALUE_OK` if the value exists and is a scalar, otherwise
 *   `ASDF_VALUE_ERR_NOT_FOUND` or `ASDF_VALUE_ERR_TYPE_MISMATCH`.
 */
ASDF_EXPORT asdf_value_err_t
asdf_get_scalar(asdf_file_t *file, const char *path, const char **out, size_t *out_len);

/**
 * Like `asdf_get_scalar0` but returns a null-terminated string
 *
 * :param file: The `asdf_file_t *` for the file
 * :param path: The JSON Path to the scalar
 * :param out: A `const char **` into which to return the scalar as a
 *   `const char *`
 * :return: `ASDF_VALUE_OK` if the value exists and is a scalar, otherwise
 *   `ASDF_VALUE_ERR_NOT_FOUND` or `ASDF_VALUE_ERR_TYPE_MISMATCH`.
 */
ASDF_EXPORT asdf_value_err_t
asdf_get_scalar0(asdf_file_t *file, const char *path, const char **out);

/**
 * Check if the value at the given tree path is a boolean scalar
 *
 * This returns true for the non-string (that is, unquoted) scalars
 * ``true/True/TRUE``, ``false/False/FALSE`` as well as ints ``0`` or ``1``
 * strictly.
 *
 * :param file: The `asdf_file_t *` for the file
 * :param path: The JSON Path to the bool
 * :return: `true` if the value is a bool, `false` if it is another
 *   type of value or if no value exists at that path.
 */
ASDF_EXPORT bool asdf_is_bool(asdf_file_t *file, const char *path);

/**
 * Get a bool value out of the ASDF tree
 *
 * See `asdf_is_bool`.
 *
 * :param file: The `asdf_file_t *` for the file
 * :param path: The JSON Path to the string
 * :param out: A `bool *` into which to return the bool
 * :return: `ASDF_VALUE_OK` if the value exists and is a bool, otherwise
 *   `ASDF_VALUE_ERR_NOT_FOUND` or `ASDF_VALUE_ERR_TYPE_MISMATCH`.
 */
ASDF_EXPORT asdf_value_err_t asdf_get_bool(asdf_file_t *file, const char *path, bool *out);

/**
 * Check if the value at the given tree path is null
 *
 * This returns true for the unquoted scalars ``null/Null/NULL`` or ``~`` as
 * well as empty values (e.g. if a mapping key is followed by nothing but
 * whitespace).
 *
 * There is no corresponding ``asdf_get_null`` as it would probably be useless.
 *
 * :param file: The `asdf_file_t *` for the file
 * :param path: The JSON Path to the null value
 * :return: `true` if the value is null, `false` if it is another
 *   type of value or if no value exists at that path.
 */
ASDF_EXPORT bool asdf_is_null(asdf_file_t *file, const char *path);

/**
 * .. _int getters:
 *
 * Integer getters
 * ---------------
 *
 * The following functions are the type checkers and getters for integer types.
 *
 * When libasdf detects an integer scalar it assigns to it the smallest C
 * integer type that can hold that value.  For example the number ``42`` is
 * typed as `ASDF_VALUE_UINT8`.
 *
 * However, integer up-casting to larger integer types.  Downcasting that would
 * cause an overflow is not allowed.  For example ``42`` can be cast to an
 * ``int16``, but ``-42`` cannot be cast to a ``uint16``.
 *
 * .. note::
 *
 *   In practice, unless you know some schema expects a small integer for a
 *   value, you will mostly just want to use `asdf_get_int64`.
 *
 * With the ``asdf_get_(uint)N`` getters the `asdf_value_err_t` return value
 * may also be `ASDF_VALUE_ERR_OVERFLOW` if the value is an integer that is too
 * large to represent in the requested type.
 *
 * Big integers (greater than ``UINT64_MAX`` or less than ``INT64_MAX``) are
 * not supported--in fact the ASDF Standard expressly
 * `forbids <ASDF Numeric Literals>`_ writing them to ASDF files.  Nevertheless
 * it could be supported in the future if the need arises.  In fact,
 * technically the ASDF Standard disallows integers greater than ``INT64_MAX``
 * but here we do allow unsigned integers up to ``UINT64_MAX``.
 */

/**
 * Check if the value at the given tree path is a integer scalar of any byte
 * size
 *
 * :param file: The `asdf_file_t *` for the file
 * :param path: The JSON Path to the bool
 * :return: `true` if the value is an integer, `false` if it is another
 *   type of value or if no value exists at that path.
 */
ASDF_EXPORT bool asdf_is_int(asdf_file_t *file, const char *path);

/** See :ref:`int getters` */
ASDF_EXPORT bool asdf_is_int8(asdf_file_t *file, const char *path);

/** See :ref:`int getters` */
ASDF_EXPORT asdf_value_err_t asdf_get_int8(asdf_file_t *file, const char *path, int8_t *out);

/** See :ref:`int getters` */
ASDF_EXPORT bool asdf_is_int16(asdf_file_t *file, const char *path);

/** See :ref:`int getters` */
ASDF_EXPORT asdf_value_err_t asdf_get_int16(asdf_file_t *file, const char *path, int16_t *out);

/** See :ref:`int getters` */
ASDF_EXPORT bool asdf_is_int32(asdf_file_t *file, const char *path);

/** See :ref:`int getters` */
ASDF_EXPORT asdf_value_err_t asdf_get_int32(asdf_file_t *file, const char *path, int32_t *out);

/** See :ref:`int getters` */
ASDF_EXPORT bool asdf_is_int64(asdf_file_t *file, const char *path);

/** See :ref:`int getters` */
ASDF_EXPORT asdf_value_err_t asdf_get_int64(asdf_file_t *file, const char *path, int64_t *out);

/** Alias for `asdf_get_int64` */
#define asdf_get_int asdf_get_int64

/** See :ref:`int getters` */
ASDF_EXPORT bool asdf_is_uint8(asdf_file_t *file, const char *path);

/** See :ref:`int getters` */
ASDF_EXPORT asdf_value_err_t asdf_get_uint8(asdf_file_t *file, const char *path, uint8_t *out);

/** See :ref:`int getters` */
ASDF_EXPORT bool asdf_is_uint16(asdf_file_t *file, const char *path);

/** See :ref:`int getters` */
ASDF_EXPORT asdf_value_err_t asdf_get_uint16(asdf_file_t *file, const char *path, uint16_t *out);

/** See :ref:`int getters` */
ASDF_EXPORT bool asdf_is_uint32(asdf_file_t *file, const char *path);

/** See :ref:`int getters` */
ASDF_EXPORT asdf_value_err_t asdf_get_uint32(asdf_file_t *file, const char *path, uint32_t *out);

/** See :ref:`int getters` */
ASDF_EXPORT bool asdf_is_uint64(asdf_file_t *file, const char *path);

/** See :ref:`int getters` */
ASDF_EXPORT asdf_value_err_t asdf_get_uint64(asdf_file_t *file, const char *path, uint64_t *out);

/**
 * .. _float getters:
 *
 * Float getters
 * -------------
 *
 * Similarly to the integer getters the `asdf_is_float` method will return true
 * if the floating point value can be represented as accurately in a 32-bit
 * float as in a double (the mantissa and exponent are small).
 *
 * Otherwise it is safe to `asdf_is_double` and `asdf_get_double` for most
 * cases. The `asdf_value_err_t` return value can also be
 * `ASDF_VALUE_ERR_OVERFLOW` if the number is too large to represent as an
 * IEEE 64-bit float (in particular, if `strtod` sets `errno = ERANGE`).
 */

/** See :ref:`float getters` */
ASDF_EXPORT bool asdf_is_float(asdf_file_t *file, const char *path);

/** See :ref:`float getters` */
ASDF_EXPORT asdf_value_err_t asdf_get_float(asdf_file_t *file, const char *path, float *out);

/** See :ref:`float getters` */
ASDF_EXPORT bool asdf_is_double(asdf_file_t *file, const char *path);

/** See :ref:`float getters` */
ASDF_EXPORT asdf_value_err_t asdf_get_double(asdf_file_t *file, const char *path, double *out);

/**
 * Extension object getters
 * ------------------------
 *
 * .. todo::
 *
 *   Needs :ref:`extensions` documentation.
 */
ASDF_EXPORT bool asdf_is_extension_type(asdf_file_t *file, const char *path, asdf_extension_t *ext);

/**
 * .. todo::
 *
 *   Needs :ref:`extensions` documentation.
 */
ASDF_EXPORT asdf_value_err_t
asdf_get_extension_type(asdf_file_t *file, const char *path, asdf_extension_t *ext, void **out);

/**
 * Block-related APIs
 * ------------------
 *
 * More commonly you will use the ``core/ndarray`` suite of APIs for
 * accessing block data associated with a ``core/ndarray``.  However, these
 * provide "low-level" access to blocks directly.
 */

/**
 * Opaque struct type representing information about an ASDF binary block
 *
 * Many of the block-related APIs work on `asdf_block_t *` arguments.
 */
typedef struct asdf_block asdf_block_t;


/**
 * Return the total number of binary blocks in the ASDF file
 *
 * :param file: The `asdf_file_t *` for the file
 * :return: Number of blocks in the file as a `size_t`
 */
ASDF_EXPORT size_t asdf_block_count(asdf_file_t *file);

/**
 * Open a block for reading the raw bytes from it
 *
 * This needs to be called before using `asdf_block_data` and should have a
 * complementary `asdf_block_close` when done.  When the file is read from
 * disk this sets up a memory map to the block.
 *
 * :param file: The `asdf_file_t *` for the file
 * :param index: The index of the block starting from 0
 * :return: An `asdf_block_t *` handle representing the block
 */
ASDF_EXPORT asdf_block_t *asdf_block_open(asdf_file_t *file, size_t index);

/**
 * Close an open `asdf_block_t *` handle
 *
 * After calling this any previous pointers to the block data are invalid.
 *
 * :param block: The `asdf_block_t *` handle
 */
ASDF_EXPORT void asdf_block_close(asdf_block_t *block);


/**
 * Append a new block to the file, given the data and size of the block
 *
 * Currently requires the file to be open in write mode.
 *
 * The same data can be duplicated to multiple blocks simply by repeated calls
 * to this function; there is not currently any tracking as to whether the same
 * data array already has an associated block in the file.
 *
 * :param file: The `asdf_file_t *` handle
 * :param data: The data array to write to the block (uncompressed)
 * :param size: The uncompressed size of the block data
 * :return: The index of the appended block, or a negative value if appending
 *   the block failed
 */
ASDF_EXPORT ssize_t asdf_block_append(asdf_file_t *file, const void *data, size_t size);


/**
 * Get the (uncompressed) size of the block data
 *
 * :param block: The `asdf_block_t *` handle
 * :return: The size of the block data as a `size_t`
 */
ASDF_EXPORT size_t asdf_block_data_size(asdf_block_t *block);


/**
 * Get the compression type, if any, of a block
 *
 * :param block: The `asdf_block_t *` handle
 * :return: A NULL-terminated string containing the compression type, if any
 */
ASDF_EXPORT const char *asdf_block_compression(asdf_block_t *block);


/**
 * Returns a `void *` to the beginning of the block data, and optionally its size
 *
 * .. note::
 *
 *   Compressed blocks are not yet supported, or rather, are not automatically
 *   decompressed.  This is a TODO.
 *
 * :param block: The `asdf_block_t *` handle
 * :param size: Optional `size_t *` into which the size of the block data is
 *   is returned
 * :return: A `void *` to the block data
 */
ASDF_EXPORT const void *asdf_block_data(asdf_block_t *block, size_t *size);

ASDF_END_DECLS

#endif /* ASDF_FILE_H */
