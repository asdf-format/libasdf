/**
 * Structures and methods pertaining to ASDF tree values
 *
 * .. todo::
 *
 *   Document API for generic values.
 */

//

#ifndef ASDF_VALUE_H
#define ASDF_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

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
     * Error when the given :ref:`yaml-pointer` does not correspond to a path
     * in the ASDF tree
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

    /**
     * Error returned when setting a value on a file or other value set to
     * read-only/immutable.
     */
    ASDF_VALUE_ERR_READ_ONLY
} asdf_value_err_t;


// Forward-declaration
typedef struct asdf_file asdf_file_t;


/**
 * Free memory held by an `asdf_value_t`
 *
 * Calling this does *not* destroy any memory allocated for C-native data
 * coerced from the value, including extension types.  It just frees up the
 * `asdf_value_t` that wrapped it.
 *
 * :param value: The `asdf_value_t *` handle
 */
ASDF_EXPORT void asdf_value_destroy(asdf_value_t *value);
ASDF_EXPORT asdf_value_t *asdf_value_clone(asdf_value_t *value);
ASDF_EXPORT asdf_value_t *asdf_value_clone_to_file(asdf_value_t *value, asdf_file_t *file);

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

// Forward-declaration needed for `asdf_value_file`
typedef struct asdf_file asdf_file_t;

/** Get the `asdf_file_t *` handle to the file to which a value belongs */
ASDF_EXPORT const asdf_file_t *asdf_value_file(asdf_value_t *value);

/** Mappings */

/**
 * Opaque struct represening a mapping value
 *
 * .. note::
 *
 *   By design, it is safe to cast a an `asdf_value_t *` to
 *   `asdf_mapping_t *` so long as the value has been checked to be a
 *   sequence.
 */
typedef struct asdf_mapping asdf_mapping_t;

ASDF_EXPORT bool asdf_value_is_mapping(asdf_value_t *value);
ASDF_EXPORT int asdf_mapping_size(asdf_mapping_t *mapping);
ASDF_EXPORT asdf_value_err_t asdf_value_as_mapping(asdf_value_t *value, asdf_mapping_t **out);
ASDF_EXPORT asdf_value_t *asdf_value_of_mapping(asdf_mapping_t *mapping);
ASDF_EXPORT asdf_mapping_t *asdf_mapping_create(asdf_file_t *file);
ASDF_EXPORT asdf_mapping_t *asdf_mapping_clone(asdf_mapping_t *mapping);
ASDF_EXPORT asdf_mapping_t *asdf_mapping_clone_to_file(asdf_mapping_t *mapping, asdf_file_t *file);
ASDF_EXPORT void asdf_mapping_destroy(asdf_mapping_t *mapping);

/**
 * Return an `asdf_value_t` from the given mapping at a given key
 *
 * If the key does not exist in the mapping, or the first argument is not a
 * mapping at all, returns `NULL`
 *
 * :param mapping: An `asdf_mapping_t *` containing a mapping
 * :param key: The key into the mapping
 * :return: The `asdf_value_t *` wrapping the value at that key, if any.
 *   Make sure to release it with `asdf_value_destroy` when no-longer needed.
 */
ASDF_EXPORT asdf_value_t *asdf_mapping_get(asdf_mapping_t *mapping, const char *key);

typedef struct _asdf_mapping_iter_impl _asdf_mapping_iter_impl_t;


/** Opaque struct holding mapping iterator state */
typedef _asdf_mapping_iter_impl_t *asdf_mapping_iter_t;

/**
 * Opaque struct representing a single (key, value) pair returned when
 * iterating over a mapping with `asdf_mapping_iter`
 *
 * .. todo::
 *
 *   Finish documenting me.
 */
typedef struct _asdf_mapping_iter_impl asdf_mapping_item_t;

ASDF_EXPORT asdf_mapping_iter_t asdf_mapping_iter_init(void);
ASDF_EXPORT const char *asdf_mapping_item_key(asdf_mapping_item_t *item);
ASDF_EXPORT asdf_value_t *asdf_mapping_item_value(asdf_mapping_item_t *item);

/**
 * Iterate over a mapping value
 *
 * .. todo::
 *
 *   Finish documenting me.
 */
ASDF_EXPORT asdf_mapping_item_t *asdf_mapping_iter(
    asdf_mapping_t *mapping, asdf_mapping_iter_t *iter);


/**
 * Update / merge a mapping with key/value pairs from a second mapping
 *
 * If keys in the RHS mapping already exist in the LHS they are overwritten,
 * otherwise appended.
 *
 * Values in the RHS mapping are *copied* during the update process, so the
 * RHS mapping remains valid.
 *
 * .. todo::
 *
 *   Finish documenting me.
 */
ASDF_EXPORT asdf_value_err_t asdf_mapping_update(asdf_mapping_t *mapping, asdf_mapping_t *update);

/**
 * Set values on mappings
 *
 * .. todo::
 *
 *   Document these.
 */
ASDF_EXPORT asdf_value_err_t
asdf_mapping_set(asdf_mapping_t *mapping, const char *key, asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t
asdf_mapping_set_string(asdf_mapping_t *mapping, const char *key, const char *str, size_t len);
ASDF_EXPORT asdf_value_err_t
asdf_mapping_set_string0(asdf_mapping_t *mapping, const char *key, const char *str);

ASDF_EXPORT asdf_value_err_t
asdf_mapping_set_bool(asdf_mapping_t *mapping, const char *key, bool val);

ASDF_EXPORT asdf_value_err_t asdf_mapping_set_null(asdf_mapping_t *mapping, const char *key);

ASDF_EXPORT asdf_value_err_t
asdf_mapping_set_int8(asdf_mapping_t *mapping, const char *key, int8_t val);
ASDF_EXPORT asdf_value_err_t
asdf_mapping_set_int16(asdf_mapping_t *mapping, const char *key, int16_t val);
ASDF_EXPORT asdf_value_err_t
asdf_mapping_set_int32(asdf_mapping_t *mapping, const char *key, int32_t val);
ASDF_EXPORT asdf_value_err_t
asdf_mapping_set_int64(asdf_mapping_t *mapping, const char *key, int64_t val);
ASDF_EXPORT asdf_value_err_t
asdf_mapping_set_uint8(asdf_mapping_t *mapping, const char *key, uint8_t val);
ASDF_EXPORT asdf_value_err_t
asdf_mapping_set_uint16(asdf_mapping_t *mapping, const char *key, uint16_t val);
ASDF_EXPORT asdf_value_err_t
asdf_mapping_set_uint32(asdf_mapping_t *mapping, const char *key, uint32_t val);
ASDF_EXPORT asdf_value_err_t
asdf_mapping_set_uint64(asdf_mapping_t *mapping, const char *key, uint64_t val);

ASDF_EXPORT asdf_value_err_t
asdf_mapping_set_float(asdf_mapping_t *mapping, const char *key, float val);
ASDF_EXPORT asdf_value_err_t
asdf_mapping_set_double(asdf_mapping_t *mapping, const char *key, double val);

ASDF_EXPORT asdf_value_err_t
asdf_mapping_set_mapping(asdf_mapping_t *mapping, const char *key, asdf_mapping_t *value);

// Forward-declaration
typedef struct asdf_sequence asdf_sequence_t;

ASDF_EXPORT asdf_value_err_t
asdf_mapping_set_sequence(asdf_mapping_t *mapping, const char *key, asdf_sequence_t *value);


/**
 * Release memory resources used by `asdf_mapping_item_t`
 *
 * If the iterator was run to exhaustion (i.e. `asdf_mapping_iter` called
 * until returning `NULL`) this happens automatically, but in cases where the
 * iteration is stopped early it is necessary to free these resources manually
 * once the contained value is no longer needed.
 *
 * This also destroys the contained `asdf_value_t`.
 *
 * :param item: The `asdf_mapping_item_t *` to destroy
 */
ASDF_EXPORT void asdf_mapping_item_destroy(asdf_mapping_item_t *item);


/** Sequences */

/**
 * Opaque struct representing a sequence value
 *
 * .. note::
 *
 *   By design, it is safe to cast a an `asdf_value_t *` to
 *   `asdf_sequence_t *` so long as the value has been checked to be a
 *   sequence.
 */
typedef struct asdf_sequence asdf_sequence_t;

ASDF_EXPORT bool asdf_value_is_sequence(asdf_value_t *value);
ASDF_EXPORT int asdf_sequence_size(asdf_sequence_t *sequence);
ASDF_EXPORT asdf_value_t *asdf_sequence_get(asdf_sequence_t *sequence, int index);
ASDF_EXPORT asdf_value_err_t asdf_value_as_sequence(asdf_value_t *value, asdf_sequence_t **out);
ASDF_EXPORT asdf_value_t *asdf_value_of_sequence(asdf_sequence_t *sequence);
ASDF_EXPORT asdf_sequence_t *asdf_sequence_create(asdf_file_t *file);
ASDF_EXPORT void asdf_sequence_destroy(asdf_sequence_t *sequence);

/** Opaque struct holding sequence iterator state */
typedef void *asdf_sequence_iter_t;

ASDF_EXPORT asdf_sequence_iter_t asdf_sequence_iter_init(void);


/**
 * Iterate over a sequence value
 *
 * .. todo::
 *
 *   Finish documenting me.
 */
ASDF_EXPORT asdf_value_t *asdf_sequence_iter(asdf_sequence_t *sequence, asdf_sequence_iter_t *iter);


/**
 * Append values to sequences
 *
 * .. todo::
 *
 *   Document these.
 */
ASDF_EXPORT asdf_value_err_t asdf_sequence_append(asdf_sequence_t *sequence, asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t
asdf_sequence_append_string(asdf_sequence_t *sequence, const char *str, size_t len);
ASDF_EXPORT asdf_value_err_t
asdf_sequence_append_string0(asdf_sequence_t *sequence, const char *str);

ASDF_EXPORT asdf_value_err_t asdf_sequence_append_bool(asdf_sequence_t *sequence, bool val);

ASDF_EXPORT asdf_value_err_t asdf_sequence_append_null(asdf_sequence_t *sequence);

ASDF_EXPORT asdf_value_err_t asdf_sequence_append_int8(asdf_sequence_t *sequence, int8_t val);
ASDF_EXPORT asdf_value_err_t asdf_sequence_append_int16(asdf_sequence_t *sequence, int16_t val);
ASDF_EXPORT asdf_value_err_t asdf_sequence_append_int32(asdf_sequence_t *sequence, int32_t val);
ASDF_EXPORT asdf_value_err_t asdf_sequence_append_int64(asdf_sequence_t *sequence, int64_t val);
ASDF_EXPORT asdf_value_err_t asdf_sequence_append_uint8(asdf_sequence_t *sequence, uint8_t val);
ASDF_EXPORT asdf_value_err_t asdf_sequence_append_uint16(asdf_sequence_t *sequence, uint16_t val);
ASDF_EXPORT asdf_value_err_t asdf_sequence_append_uint32(asdf_sequence_t *sequence, uint32_t val);
ASDF_EXPORT asdf_value_err_t asdf_sequence_append_uint64(asdf_sequence_t *sequence, uint64_t val);

ASDF_EXPORT asdf_value_err_t asdf_sequence_append_float(asdf_sequence_t *sequence, float val);
ASDF_EXPORT asdf_value_err_t asdf_sequence_append_double(asdf_sequence_t *sequence, double val);

ASDF_EXPORT asdf_value_err_t
asdf_sequence_append_mapping(asdf_sequence_t *sequence, asdf_mapping_t *value);
ASDF_EXPORT asdf_value_err_t
asdf_sequence_append_sequence(asdf_sequence_t *sequence, asdf_sequence_t *value);


/**
 * Any container-related functions
 *
 * Here "container" means either sequence or mapping though maybe could extend
 * this interface in the future for custom container types
 *
 * These routines provide a convience for iterating over a container regardless
 * if it is a sequence or mapping using a common interface.
 *
 * .. todo::
 *
 *   Document these better.
 */

//

/** Opaque struct containing container iterator state */
typedef void *asdf_container_iter_t;

/**
 * Opaque struct for values returned from `asdf_container_iter`
 *
 * This combines the value returned from the container iteration as its key
 * (if iterating over a mapping) or index (if iterating over a sequence) as
 * well as internal iterator state.
 *
 * Use the functions `asdf_container_item_key`, `asdf_container_item_index`
 * and `asdf_container_item_value` to extract properties from the container
 * item.
 */
typedef struct asdf_container_item asdf_container_item_t;


ASDF_EXPORT asdf_container_iter_t asdf_container_iter_init(void);


/**
 * If iterating over a mapping with `asdf_container_iter`, returns the key of
 * a single item from the container
 *
 * :param item: The `asdf_container_item_t *` returned by `asdf_container_iter`
 * :return: The key in the mapping of the value, or NULL if the container was
 *   not a mapping
 */
ASDF_EXPORT const char *asdf_container_item_key(asdf_container_item_t *item);


/**
 * If iterating over a sequence with `asdf_container_iter`, returns the index of
 * a single item from the container
 *
 * :param item: The `asdf_container_item_t *` returned by `asdf_container_iter`
 * :return: The index of the value in a sequence, or -1 if the container was
 *   not a sequence
 */
ASDF_EXPORT int asdf_container_item_index(asdf_container_item_t *item);


/**
 * If iterating over a sequence with `asdf_container_iter`, returns the value
 * of that container item (where "item" is the pair of the value and its key/
 * index)
 *
 * :param item: The `asdf_container_item_t *` returned by `asdf_container_iter`
 * :return: The underling `asdf_value_t *` generic value pointer
 */
ASDF_EXPORT asdf_value_t *asdf_container_item_value(asdf_container_item_t *item);


/**
 * Generic container iterator
 *
 * Similar to `asdf_mapping_iter` but can be used with either a sequence or a
 * mapping
 *
 * In most cases you will want to use `asdf_mapping_iter` or
 * `asdf_sequence_iter` more specifically, but sometimes it is useful to have a
 * generic iteration method that can handle both.
 *
 * :param item: The `asdf_container_item_t *` returned by `asdf_container_iter`
 * :return: The underling `asdf_value_t *` generic value pointer
 */
ASDF_EXPORT asdf_container_item_t *asdf_container_iter(
    asdf_value_t *container, asdf_container_iter_t *iter);

/**
 * Release memory resources used by `asdf_container_item_t`
 *
 * If the iterator was run to exhaustion (i.e. `asdf_container_iter` called
 * until returning `NULL`) this happens automatically, but in cases where the
 * iteration is stopped early it is necessary to free these resources manually
 * once the contained value is no longer needed.
 *
 * :param item: The `asdf_container_item_t *` to destroy
 */
ASDF_EXPORT void asdf_container_item_destroy(asdf_container_item_t *item);

ASDF_EXPORT bool asdf_value_is_container(asdf_value_t *value);


/** Extension-related functions */

// Forward declaration for asdf_extension_t
typedef struct _asdf_extension asdf_extension_t;

/**
 * Check if an `asdf_value_t *` has the specified extension type
 *
 * .. note::
 *
 *   This is usually wrapped by some helper utility named like ``asdf_value_is_<extention>``,
 *   such as ``asdf_value_is_ndarray``.
 *
 *   But that is equivalent to running:
 *
 *   .. code:: c
 *
 *     const asdf_extension_t *ext = asdf_extension_get(file, "tag:...");
 *     asdf_value_is_extension_type(value, ext);
 */
ASDF_EXPORT bool asdf_value_is_extension_type(asdf_value_t *value, const asdf_extension_t *ext);


/**
 * Cast an `asdf_value_t *` to the specified extension type
 *
 * .. note::
 *
 *   This is usually wrapped by some helper utility named like ``asdf_value_as_<extention>``,
 *   such as ``asdf_value_as_ndarray``.
 *
 *   But that is equivalent to running:
 *
 *   .. code:: c
 *
 *     asdf_ndarray_t *ndarray = NULL;
 *     const asdf_extension_t *ext = asdf_extension_get(file, "tag:...");
 *     asdf_value_as_extension_type(value, ext, &ndarray);
 */
ASDF_EXPORT asdf_value_err_t
asdf_value_as_extension_type(asdf_value_t *value, const asdf_extension_t *ext, void **out);


ASDF_EXPORT asdf_value_t *asdf_value_of_extension_type(
    asdf_file_t *file, const void *obj, const asdf_extension_t *ext);

/** Generic value functions */

/**
 * Given an arbitrary `asdf_value_type_t` enum member, check if the value has
 * that type.
 *
 * .. note::
 *
 *   For checking against a specific extension type it's still necessary to use
 *   `asdf_value_is_extension_type`
 */
ASDF_EXPORT bool asdf_value_is_type(asdf_value_t *value, asdf_value_type_t type);


/**
 * Retrieve the underlying value of an arbitrary `asdf_value_t` as the type
 * associated with an `asdf_value_type_t`
 *
 * The output address is passed in as a `void *`.  In the case of
 * `ASDF_VALUE_STRING` the value is always returned as a 0-terminated string.
 *
 * .. note::
 *
 *   For getting the value of a specific extension type it's still necessary to use
 *   `asdf_value_as_extension_type`
 */
ASDF_EXPORT asdf_value_err_t
asdf_value_as_type(asdf_value_t *value, asdf_value_type_t type, void *out);


/* Scalar-related definitions */
ASDF_EXPORT bool asdf_value_is_string(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t
asdf_value_as_string(asdf_value_t *value, const char **out, size_t *out_len);
ASDF_EXPORT asdf_value_t *asdf_value_of_string(asdf_file_t *file, const char *value, size_t len);
ASDF_EXPORT asdf_value_err_t asdf_value_as_string0(asdf_value_t *value, const char **out);
ASDF_EXPORT asdf_value_t *asdf_value_of_string0(asdf_file_t *file, const char *value);

ASDF_EXPORT bool asdf_value_is_scalar(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t
asdf_value_as_scalar(asdf_value_t *value, const char **out, size_t *out_len);
ASDF_EXPORT asdf_value_err_t asdf_value_as_scalar0(asdf_value_t *value, const char **out);

ASDF_EXPORT bool asdf_value_is_bool(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_bool(asdf_value_t *value, bool *out);
ASDF_EXPORT asdf_value_t *asdf_value_of_bool(asdf_file_t *file, bool value);

ASDF_EXPORT bool asdf_value_is_null(asdf_value_t *value);
ASDF_EXPORT asdf_value_t *asdf_value_of_null(asdf_file_t *file);

ASDF_EXPORT bool asdf_value_is_int(asdf_value_t *value);

ASDF_EXPORT bool asdf_value_is_int8(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_int8(asdf_value_t *value, int8_t *out);
ASDF_EXPORT asdf_value_t *asdf_value_of_int8(asdf_file_t *file, int8_t value);

ASDF_EXPORT bool asdf_value_is_int16(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_int16(asdf_value_t *value, int16_t *out);
ASDF_EXPORT asdf_value_t *asdf_value_of_int16(asdf_file_t *file, int16_t value);

ASDF_EXPORT bool asdf_value_is_int32(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_int32(asdf_value_t *value, int32_t *out);
ASDF_EXPORT asdf_value_t *asdf_value_of_int32(asdf_file_t *file, int32_t value);

ASDF_EXPORT bool asdf_value_is_int64(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_int64(asdf_value_t *value, int64_t *out);
ASDF_EXPORT asdf_value_t *asdf_value_of_int64(asdf_file_t *file, int64_t value);

ASDF_EXPORT bool asdf_value_is_uint8(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_uint8(asdf_value_t *value, uint8_t *out);
ASDF_EXPORT asdf_value_t *asdf_value_of_uint8(asdf_file_t *file, uint8_t value);

ASDF_EXPORT bool asdf_value_is_uint16(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_uint16(asdf_value_t *value, uint16_t *out);
ASDF_EXPORT asdf_value_t *asdf_value_of_uint16(asdf_file_t *file, uint16_t value);

ASDF_EXPORT bool asdf_value_is_uint32(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_uint32(asdf_value_t *value, uint32_t *out);
ASDF_EXPORT asdf_value_t *asdf_value_of_uint32(asdf_file_t *file, uint32_t value);

ASDF_EXPORT bool asdf_value_is_uint64(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_uint64(asdf_value_t *value, uint64_t *out);
ASDF_EXPORT asdf_value_t *asdf_value_of_uint64(asdf_file_t *file, uint64_t value);

ASDF_EXPORT bool asdf_value_is_float(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_float(asdf_value_t *value, float *out);
ASDF_EXPORT asdf_value_t *asdf_value_of_float(asdf_file_t *file, float value);

ASDF_EXPORT bool asdf_value_is_double(asdf_value_t *value);
ASDF_EXPORT asdf_value_err_t asdf_value_as_double(asdf_value_t *value, double *out);
ASDF_EXPORT asdf_value_t *asdf_value_of_double(asdf_file_t *file, double value);

/** Tree traversal functions */

/**
 * Type definition for predicate functions used in `asdf_value_find` and
 * friends
 *
 * Simply takes an arbitrary value as `asdf_value_t *` and returns `true` if
 * the predicate matches the value.
 */
typedef bool (*asdf_value_pred_t)(asdf_value_t *value);

/**
 * Opaque struct representing a value found with `asdf_value_find` and friends
 *
 * Use `asdf_find_item_path` to return the path of the found value, and
 * `asdf_find_item_value` to return the corresponding value.
 */
typedef struct asdf_find_item asdf_find_item_t;

/**
 * Traverse the tree breadth-first starting from ``root`` until a value
 * matching the given predicate is found
 *
 * :param value: `asdf_value_t *` handle for the root node in which to start
 *   the search--if the value is neither a mapping or sequence this function
 *   will still return non-NULL if this value matches the predicate
 * :param pred: A predicate function to match the value to return; see
 *   `asdf_value_pred_t`
 * :return: An `asdf_find_item_t *` for the first matching value or NULL if the
 *   tree is exhausted without finding a matching value
 */
ASDF_EXPORT asdf_find_item_t *asdf_value_find(asdf_value_t *root, asdf_value_pred_t pred);

/**
 * Return the path of a value found with `asdf_value_find` and friends
 *
 * :param item: `asdf_find_item_t *` result from `asdf_value_find`, `asdf_value_find_iter`
 *   `asdf_value_find_ex`
 * :return: Full path of the found value
 */
ASDF_EXPORT const char *asdf_find_item_path(asdf_find_item_t *item);

/**
 * Return the value found with `asdf_value_find` and friends
 *
 * :param item: `asdf_find_item_t *` result from `asdf_value_find`, `asdf_value_find_iter`
 *   `asdf_value_find_ex`
 * :return: The `asdf_value_t *` handle to the found value
 */
ASDF_EXPORT asdf_value_t *asdf_find_item_value(asdf_find_item_t *item);


/**
 * Opaque struct used to manage state across `asdf_value_find_iter` calls
 */
typedef void *asdf_find_iter_t;


/**
 * Traverse the tree breadth-first starting from ``root`` until a value
 * matching the given predicate is found; subsequent calls with the same
 * `asdf_find_iter_t *` handle will return all matching values until the
 * tree has been exhaustively traversed
 *
 * A typical usage pattern is similar to `asdf_mapping_iter`, like:
 *
 * .. code::
 *
 *   asdf_find_iter_t iter = asdf_find_iter_init();
 *   asdf_find_item_t *item = NULL;
 *
 *   while ((item = asdf_value_find_iter(root, pred, &iter))) {
 *     // Do something with found item
 *   }
 *
 * :param value: `asdf_value_t *` handle for the root node in which to start
 *   the search--if the value is neither a mapping or sequence this function
 *   will still return non-NULL if this value matches the predicate
 * :param pred: A predicate function to match the value to return; see
 *   `asdf_value_pred_t`
 * :param iter: A pointer to an `asdf_find_iter_t` used internally to track
 *   the iteration state
 * :return: An `asdf_find_item_t *` for the first matching value or NULL if the
 *   tree is exhausted without finding a matching value
 */
ASDF_EXPORT asdf_find_item_t *asdf_value_find_iter(
    asdf_value_t *root, asdf_value_pred_t pred, asdf_find_iter_t *iter);

/**
 * Alias for `true` for use with `asdf_value_find_ex` and
 * `asdf_find_iter_init_ex` for added clarity
 */
#define ASDF_DEPTH_FIRST true


/**
 * Alias for `false` for use with `asdf_value_find_ex` and
 * `asdf_find_iter_init_ex` for added clarity
 */
#define ASDF_BREADTH_FIRST false


/**
 * Extended version of `asdf_value_find` with additional options
 *
 * :param value: `asdf_value_t *` handle for the root node in which to start
 *   the search--if the value is neither a mapping or sequence this function
 *   will still return non-NULL if this value matches the predicate
 * :param pred: A predicate function to match the value to return; see
 *   `asdf_value_pred_t`
 * :param depth_first: If `true` descend the tree in depth-first order;
 *   otherwise the tree is traversed breadth-first
 * :param descend_pred: Optional predicate (NULL indicates descend into all
 *   nodes) indicating whether a given mapping or sequence should be descended
 *   during traversal
 * :param max_depth: Maximum depth into the tree to descend; -1 indicates no
 *   limit
 *
 *   There are also some built-in defaults `asdf_find_descend_mapping_only` and
 *   `asdf_find_descend_sequence_only`, as well as `asdf_find_descend_all`
 *   (which is equivalent to passing `NULL`).
 * :return: An `asdf_find_item_t *` for the first matching value or NULL if the
 *   tree is exhausted without finding a matching value
 */
ASDF_EXPORT asdf_find_item_t *asdf_value_find_ex(
    asdf_value_t *root,
    asdf_value_pred_t pred,
    bool depth_first,
    asdf_value_pred_t descend_pred,
    ssize_t max_depth);


/**
 * For use as the ``descend_pred`` argument to `asdf_value_find_ex` and
 * `asdf_find_iter_init_ex`
 *
 * Only descend into mappings (don't iterate over sequences).
 */
static inline bool asdf_find_descend_mapping_only(asdf_value_t *value) {
    return asdf_value_is_mapping(value);
}


/**
 * For use as the ``descend_pred`` argument to `asdf_value_find_ex` and
 * `asdf_find_iter_init_ex`
 *
 * Only iterate over sequences and nested sequences (don't descend into nested
 * mappings).
 */
static inline bool asdf_find_descend_sequence_only(asdf_value_t *value) {
    return asdf_value_is_sequence(value);
}


/**
 * For use as the ``descend_pred`` argument to `asdf_value_find_ex` and
 * `asdf_find_iter_init_ex`
 *
 * Equivalent to passing this argument ``NULL``, meaning always descend
 * into nested mappings and sequences.
 */
#define asdf_find_descend_all ((asdf_value_pred_t)NULL)


/**
 * Instantiate an `asdf_find_iter_t` as a local variable to use with
 * `asdf_value_find_iter`
 *
 * :return: An `asdf_find_iter_t`, the address of which should be passed to
 *   to `asdf_value_find_iter`
 */
ASDF_EXPORT asdf_find_iter_t asdf_find_iter_init(void);


/**
 * Like `asdf_find_iter_init` but allows providing additional options for
 * the tree traversal similar to those accepted by `asdf_value_find_ex`
 *
 * These options only need to be passed to instantiate the iterator, and not
 * in subsequent calls to `asdf_value_find`.
 *
 * :param depth_first: If `true` descend the tree in depth-first order;
 *   otherwise the tree is traversed breadth-first
 * :param descend_pred: Optional predicate (NULL indicates descend into all
 *   nodes) indicating whether a given mapping or sequence should be descended
 *   during traversal
 * :param max_depth: Maximum depth into the tree to descend; -1 indicates no
 *   limit
 * :return: An `asdf_find_iter_t`, the address of which should be passed to
 *   to `asdf_value_find_iter`
 */
ASDF_EXPORT asdf_find_iter_t
asdf_find_iter_init_ex(bool depth_first, asdf_value_pred_t descend_pred, ssize_t max_depth);


/**
 * Release memory resources used by `asdf_find_item_t`
 *
 * If the iterator was run to exhaustion (i.e. `asdf_value_find_iter` called
 * until returning `NULL`) this happens automatically, but in cases where the
 * iteration is stopped early, or when using `asdf_value_find`, it is necessary
 * to free these resources manually once the value is no longer needed.
 *
 * :param item: The `asdf_find_item_t *` to destroy
 */
ASDF_EXPORT void asdf_find_item_destroy(asdf_find_item_t *item);

ASDF_END_DECLS

#endif /* ASDF_VALUE_H */
