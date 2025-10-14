/**
 * Structures and methods pertaining to ASDF tree values
 *
 * .. todo::
 *
 *   Document API for generic values.
 */
#ifndef ASDF_VALUE_H
#define ASDF_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <asdf/util.h>

ASDF_BEGIN_DECLS


/**
 * Opaque struct representing a generic value from the ASDF tree
 */
typedef struct asdf_value asdf_value_t;

/**
 * Tags used to identify the type of a value in the ASDF tree
 *
 * See e.g. `asdf_value_get_type` to get the known type of an `asdf_value_t`.
 */
typedef enum {
    /** Unknown type; typically not encountered except in a parsing error */
    ASDF_VALUE_UNKNOWN,

    /** A YAML sequence (array) */
    ASDF_VALUE_SEQUENCE,

    /** A YAML mapping (a.k.a. dict, hash, etc.) */
    ASDF_VALUE_MAPPING,

    /** A generic scalar before coerced to a more specific type */
    ASDF_VALUE_SCALAR,

    /**
     * A string
     *
     * A scalar is considered a string if it is explicitly quoted with single-
     * or double-quotes, or uses a literal or folded scalar represenation style
     * in the YAML document, or any other scalar that cannot be coecered to one
     * of the other types defined in the `YAML Core Schema`_.
     */
    ASDF_VALUE_STRING,

    /**
     * A bool
     *
     * Values have this type if they are unquoted scalars ``true/True/TRUE`` or
     * ``false/False/FALSE`` (case-sensitive).
     */
    ASDF_VALUE_BOOL,

    /**
     * A null value
     *
     * Values are null if a key in a mapping is followed by nothing but
     * whitespace (there is no explicit value given) or if it contains the
     * unquoted scalars ``null/Null/NULL/~`` (case-sensitive).
     */
    ASDF_VALUE_NULL,

    /** Signed 8-bit integer */
    ASDF_VALUE_INT8,

    /** Signed 16-bit integer */
    ASDF_VALUE_INT16,

    /** Signed 32-bit integer */
    ASDF_VALUE_INT32,

    /** Signed 64-bit integer */
    ASDF_VALUE_INT64,

    /** Unsigned 8-bit integer */
    ASDF_VALUE_UINT8,

    /** Unsigned 16-bit integer */
    ASDF_VALUE_UINT16,

    /** Unsigned 32-bit integer */
    ASDF_VALUE_UINT32,

    /** Unsigned 64-bit integer */
    ASDF_VALUE_UINT64,

    /** 32-bit IEEE float */
    ASDF_VALUE_FLOAT,

    /** 64-bit IEEE float */
    ASDF_VALUE_DOUBLE,

    /**
     * Extension type
     *
     * Recognized if the value has a tag associated with a registered
     * extension.
     */
    ASDF_VALUE_EXTENSION
} asdf_value_type_t;


/**
 * Return type for many functions that return a value out of the ASDF tree
 *
 * `ASDF_VALUE_OK` means that the path to the value exists, and that the value
 * could be read as the type corresponding to the getter (e.g.
 * `asdf_get_string` returns an existing string).  Other values of this enum
 * are all errors, the most common being `ASDF_VALUE_ERR_NOT_FOUND` or
 * `ASDF_VALUE_ERR_TYPE_MISMATCH`.
 */
typedef enum {
    ASDF_VALUE_ERR_UNKNOWN = -2,
    
    /**
     * Error when the given JSON Path does not correspond to a path in the
     * ASDF tree
     */
    ASDF_VALUE_ERR_NOT_FOUND = -1,

    /** The value was read successfully */
    ASDF_VALUE_OK = 0,

    /**
     * A typed getter like `asdf_get_string` was called on a path that does
     * not point to a string (but some other type)
     */
    ASDF_VALUE_ERR_TYPE_MISMATCH,

    /**
     * This error mostly occurs if a value had an explicit tag like ``!!int``
     * but could not be parsed as an int.
     */
    ASDF_VALUE_ERR_PARSE_FAILURE,

    /**
     * Error that occurs mostly in the ``asdf_get_(u)int<N>`` such as
     * `asdf_get_int8` or `asdf_get_float` or `asdf_get_double` functions if
     * the value looks like a numeric value but cannot be represented in the C
     * type returned by the function.
     */
    ASDF_VALUE_ERR_OVERFLOW,

    /**
     * Error returned when memory could not be allocated for the returned
     * object (typically only if the system is out of memory).
     */
    ASDF_VALUE_ERR_OOM,
} asdf_value_err_t;


ASDF_EXPORT void asdf_value_destroy(asdf_value_t *value);
ASDF_EXPORT asdf_value_t *asdf_value_clone(asdf_value_t *value);

/**
 * Get the specific `asdf_value_type_t` of a generic `asdf_value_t`
 *
 * :param value: The `asdf_value_t *` handle
 * :return: The `asdf_value_type_t` enum member representing the value type
 */
ASDF_EXPORT asdf_value_type_t asdf_value_get_type(asdf_value_t *value);

ASDF_EXPORT const char *asdf_value_path(asdf_value_t *value);

/**
 * Returns a human-readable string representation of an `asdf_value_type_t`;
 * useful for error-reporting
 */
ASDF_EXPORT const char *asdf_value_type_string(asdf_value_type_t type);

/* Return the value's tag if it has an *explicit* tag (implict tags are not returned) */
ASDF_EXPORT const char *asdf_value_tag(asdf_value_t *value);

/* Mapping-related definitions */
ASDF_EXPORT bool asdf_value_is_mapping(asdf_value_t *value);
ASDF_EXPORT int asdf_mapping_size(asdf_value_t *mapping);
ASDF_EXPORT asdf_value_t *asdf_mapping_get(asdf_value_t *mapping, const char *key);

typedef struct _asdf_mapping_iter_impl _asdf_mapping_iter_impl_t;
typedef _asdf_mapping_iter_impl_t* asdf_mapping_iter_t;
typedef struct _asdf_mapping_iter_impl asdf_mapping_item_t;

ASDF_EXPORT asdf_mapping_iter_t asdf_mapping_iter_init(void);
ASDF_EXPORT const char *asdf_mapping_item_key(asdf_mapping_item_t *item);
ASDF_EXPORT asdf_value_t *asdf_mapping_item_value(asdf_mapping_item_t *item);
ASDF_EXPORT asdf_mapping_item_t *asdf_mapping_iter(asdf_value_t *mapping, asdf_mapping_iter_t *iter);


/* Sequence-related definitions */
ASDF_EXPORT bool asdf_value_is_sequence(asdf_value_t *value);
ASDF_EXPORT int asdf_sequence_size(asdf_value_t *sequence);
ASDF_EXPORT asdf_value_t *asdf_sequence_get(asdf_value_t *sequence, int index);

typedef struct _asdf_sequence_iter_impl _asdf_sequence_iter_impl_t;
typedef _asdf_sequence_iter_impl_t* asdf_sequence_iter_t;

ASDF_EXPORT asdf_sequence_iter_t asdf_sequence_iter_init(void);
ASDF_EXPORT asdf_value_t *asdf_sequence_iter(asdf_value_t *sequence, asdf_sequence_iter_t *iter);


/* Extension-related definitions */
// Forward declaration for asdf_extension_t
typedef struct _asdf_extension asdf_extension_t;

ASDF_EXPORT bool asdf_value_is_extension_type(asdf_value_t *value, const asdf_extension_t *ext);
ASDF_EXPORT asdf_value_err_t asdf_value_as_extension_type(asdf_value_t *value, const asdf_extension_t *ext, void **out);


/* Scalar-related definitions */
ASDF_EXPORT bool asdf_value_is_string(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_string(asdf_value_t *value, const char **out, size_t *out_len);
ASDF_EXPORT asdf_value_err_t asdf_value_as_string0(asdf_value_t *value, const char **out);

ASDF_EXPORT bool asdf_value_is_scalar(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_scalar(asdf_value_t *value, const char **out, size_t* out_len);
ASDF_EXPORT asdf_value_err_t asdf_value_as_scalar0(asdf_value_t *value, const char **out);

ASDF_EXPORT bool asdf_value_is_bool(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_bool(asdf_value_t *value, bool *out);

ASDF_EXPORT bool asdf_value_is_null(asdf_value_t *value);

ASDF_EXPORT bool asdf_value_is_int(asdf_value_t *value);
ASDF_EXPORT bool asdf_value_is_int8(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_int8(asdf_value_t *value, int8_t *out);
ASDF_EXPORT bool asdf_value_is_int16(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_int16(asdf_value_t *value, int16_t *out);
ASDF_EXPORT bool asdf_value_is_int32(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_int32(asdf_value_t *value, int32_t *out);
ASDF_EXPORT bool asdf_value_is_int64(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_int64(asdf_value_t *value, int64_t *out);
ASDF_EXPORT bool asdf_value_is_uint8(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_uint8(asdf_value_t *value, uint8_t *out);
ASDF_EXPORT bool asdf_value_is_uint16(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_uint16(asdf_value_t *value, uint16_t *out);
ASDF_EXPORT bool asdf_value_is_uint32(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_uint32(asdf_value_t *value, uint32_t *out);
ASDF_EXPORT bool asdf_value_is_uint64(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_uint64(asdf_value_t *value, uint64_t *out);

ASDF_EXPORT bool asdf_value_is_float(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_float(asdf_value_t *value, float *out);
ASDF_EXPORT bool asdf_value_is_double(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_double(asdf_value_t *value, double *out);

ASDF_END_DECLS

#endif /* ASDF_VALUE_H */
