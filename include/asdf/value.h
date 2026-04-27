/**
 * Structures and methods pertaining to ASDF tree values
 */

//

#ifndef ASDF_VALUE_H
#define ASDF_VALUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <asdf/util.h>
#include <asdf/yaml.h>

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
     * Error that can be returned when attempting to serialize values that are
     * not in a valid state (e.g. user-defined extension values)
     */
    ASDF_VALUE_ERR_EMIT_FAILURE,

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


/**
 * Clone an `asdf_value_t`
 *
 * This performs a deep-copy of a value and may be used in certain cases
 * (such as container iteration) when the user needs an owned copy of the
 * value.
 *
 * The new cloned value is not initially attached to the YAML tree, so
 * modification to its underlying value is not reflected in the YAML; it
 * may be inserted elsewhere in the tree later.
 */
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
 * Get the parent value, if any, of the given value
 *
 * :param value: The `asdf_value_t *` handle
 * :return: An `asdf_value_t *` of its parent mapping or sequence; if the input value
 *   is the root node, or is not an attached value, the parent is NULL
 */
ASDF_EXPORT asdf_value_t *asdf_value_parent(asdf_value_t *value);

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
ASDF_EXPORT void asdf_mapping_set_style(asdf_mapping_t *mapping, asdf_yaml_node_style_t style);
ASDF_EXPORT asdf_mapping_t *asdf_mapping_clone(asdf_mapping_t *mapping);
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

/**
 * Iterator handle for traversing a mapping value
 *
 * Initialize with `asdf_mapping_iter_init`.  After each successful call to
 * `asdf_mapping_iter_next`, the ``key`` and ``value`` fields hold the current
 * mapping entry and are valid until the next call to `asdf_mapping_iter_next`
 * or `asdf_mapping_iter_destroy`.  If the value must outlive the current
 * iteration step, clone it with `asdf_value_clone`.
 *
 * .. warning::
 *
 *   Modifying the mapping while iterating over it results in undefined
 *   behavior.
 */
typedef struct {
    /** Current key */
    const char *key;
    /** Current value */
    asdf_value_t *value;
} asdf_mapping_iter_t;

/**
 * Create a new iterator over ``mapping``
 *
 * :param mapping: The `asdf_mapping_t *` to iterate over
 * :return: A new `asdf_mapping_iter_t *` handle, or ``NULL`` on allocation failure
 */
ASDF_EXPORT asdf_mapping_iter_t *asdf_mapping_iter_init(asdf_mapping_t *mapping);

/**
 * Advance the iterator to the next mapping entry
 *
 * On success, `asdf_mapping_iter_t.key` and `asdf_mapping_iter_t.value` are
 * updated to the current entry.  When iteration is exhausted the iterator is
 * freed automatically and ``*iter`` is set to ``NULL``.
 *
 * Typical usage::
 *
 *   asdf_mapping_iter_t *iter = asdf_mapping_iter_init(mapping);
 *   while (asdf_mapping_iter_next(&iter)) {
 *       // iter->key and iter->value are valid here
 *   }
 *
 * For early exit call `asdf_mapping_iter_destroy` before breaking::
 *
 *   asdf_mapping_iter_t *iter = asdf_mapping_iter_init(mapping);
 *   while (asdf_mapping_iter_next(&iter)) {
 *       if (done) {
 *           asdf_mapping_iter_destroy(iter);
 *           break;
 *       }
 *   }
 *
 * :param iter: Pointer to the iterator handle; set to ``NULL`` on exhaustion
 * :return: ``true`` if an entry was found; ``false`` when iteration is done
 */
ASDF_EXPORT bool asdf_mapping_iter_next(asdf_mapping_iter_t **iter);

/**
 * Release resources held by an in-progress mapping iterator
 *
 * Call this only when breaking out of iteration early.  Safe to call with
 * ``NULL``.
 *
 * :param iter: The `asdf_mapping_iter_t *` to destroy
 */
ASDF_EXPORT void asdf_mapping_iter_destroy(asdf_mapping_iter_t *iter);


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
 * Remove a value from a mapping and return the removed value
 *
 * :param mapping: The `asdf_mapping_t *` handle
 * :param key: The of the item to remove
 * :return: The generic `asdf_value_t *` of the removed value if any, or NULL if the key did not
 *     exist or on errors
 */
ASDF_EXPORT asdf_value_t *asdf_mapping_pop(asdf_mapping_t *mapping, const char *key);

/**
 * Set values on mappings
 *
 * .. todo::
 *
 *   Document the rest of these.
 */

/**
 * Set a generic `asdf_value_t *` on a mapping at the given key
 *
 * If the key already exists in the mapping its value will be overwritten
 *
 * :params mapping: Handle to the `asdf_mapping_t` to update
 * :params key: The key at which to insert the value
 * :params value: Generic `asdf_value_t *` to insert into the mapping
 * :return: `ASDF_VALUE_OK` on success or another `asdf_value_err_t`
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

/** Return true if ``value`` holds a YAML sequence */
ASDF_EXPORT bool asdf_value_is_sequence(asdf_value_t *value);

/**
 * Return the number of items in ``sequence``
 *
 * :param sequence: The `asdf_sequence_t *` to query
 * :return: The number of items currently in the sequence
 */
ASDF_EXPORT int asdf_sequence_size(asdf_sequence_t *sequence);

/**
 * Return the value at ``index`` in ``sequence``
 *
 * Negative indices are supported (e.g. ``-1`` for the last item).  Returns
 * ``NULL`` if ``index`` is out of range.
 *
 * :param sequence: The `asdf_sequence_t *` to query
 * :param index: Zero-based index; negative indices count from the end
 * :return: The `asdf_value_t *` at that position, or ``NULL``
 */
ASDF_EXPORT asdf_value_t *asdf_sequence_get(asdf_sequence_t *sequence, int index);

/**
 * Obtain a typed `asdf_sequence_t *` view of a generic value
 *
 * On success writes the `asdf_sequence_t *` into ``*out`` and returns
 * ``ASDF_VALUE_OK``.  Returns an error code and leaves ``*out`` unchanged if
 * ``value`` is not a sequence.
 *
 * :param value: The generic `asdf_value_t *` to inspect
 * :param out: Receives the `asdf_sequence_t *` on success
 * :return: ``ASDF_VALUE_OK`` on success, otherwise an `asdf_value_err_t` error
 */
ASDF_EXPORT asdf_value_err_t asdf_value_as_sequence(asdf_value_t *value, asdf_sequence_t **out);

/**
 * Return the generic `asdf_value_t *` view of a sequence
 *
 * The returned pointer shares ownership with ``sequence``; the caller must
 * not free it independently.
 *
 * :param sequence: The `asdf_sequence_t *` to wrap
 * :return: The same object as a generic `asdf_value_t *`
 */
ASDF_EXPORT asdf_value_t *asdf_value_of_sequence(asdf_sequence_t *sequence);

/**
 * Create a new, empty sequence attached to ``file``
 *
 * The caller owns the returned sequence and must eventually release it with
 * `asdf_sequence_destroy`, or transfer ownership by inserting it into a
 * mapping or parent sequence.  Returns ``NULL`` on allocation failure.
 *
 * :param file: The `asdf_file_t *` that will own the sequence
 * :return: A new `asdf_sequence_t *`, or ``NULL`` on failure
 */
ASDF_EXPORT asdf_sequence_t *asdf_sequence_create(asdf_file_t *file);

/**
 * Set the YAML node style used when serializing ``sequence``
 *
 * :param sequence: The `asdf_sequence_t *` to modify
 * :param style: The desired `asdf_yaml_node_style_t` (e.g. block or flow)
 */
ASDF_EXPORT void asdf_sequence_set_style(asdf_sequence_t *sequence, asdf_yaml_node_style_t style);

/**
 * Free a sequence and all values it contains
 *
 * Must not be called on a sequence that has been inserted into a mapping or
 * parent sequence -- ownership transfers at that point.
 *
 * :param sequence: The `asdf_sequence_t *` to free
 */
ASDF_EXPORT void asdf_sequence_destroy(asdf_sequence_t *sequence);


/**
 * Iterator handle for traversing a sequence value
 *
 * Initialize with `asdf_sequence_iter_init`.  After each successful call to
 * `asdf_sequence_iter_next`, the ``index`` and ``value`` fields hold the
 * current element and are valid until the next call to
 * `asdf_sequence_iter_next` or `asdf_sequence_iter_destroy`.
 *
 * .. warning::
 *
 *   Modifying the sequence while iterating over it results in undefined
 *   behavior.
 */
typedef struct {
    /** Current value */
    asdf_value_t *value;
    /** Index of the current value */
    int index;
} asdf_sequence_iter_t;

/**
 * Create a new iterator over ``sequence``
 *
 * :param sequence: The `asdf_sequence_t *` to iterate over
 * :return: A new `asdf_sequence_iter_t *` handle, or ``NULL`` on allocation failure
 */
ASDF_EXPORT asdf_sequence_iter_t *asdf_sequence_iter_init(asdf_sequence_t *sequence);

/**
 * Advance the iterator to the next sequence element
 *
 * Typical usage::
 *
 *   asdf_sequence_iter_t *iter = asdf_sequence_iter_init(sequence);
 *   while (asdf_sequence_iter_next(&iter)) {
 *       // iter->index and iter->value are valid here
 *   }
 *
 * :param iter: Pointer to the iterator handle; set to ``NULL`` on exhaustion
 * :return: ``true`` if an element was found; ``false`` when iteration is done
 */
ASDF_EXPORT bool asdf_sequence_iter_next(asdf_sequence_iter_t **iter);

/**
 * Release resources held by an in-progress sequence iterator
 *
 * Call this only when breaking out of iteration early.  Safe to call with
 * ``NULL``.
 *
 * :param iter: The `asdf_sequence_iter_t *` to destroy
 */
ASDF_EXPORT void asdf_sequence_iter_destroy(asdf_sequence_iter_t *iter);


/**
 * Append a value to a sequence
 *
 * ``asdf_sequence_append`` appends an existing generic `asdf_value_t *`.
 * Ownership of ``value`` transfers to ``sequence`` on success.
 *
 * The ``asdf_sequence_append_<type>`` variants construct a new value from a C
 * scalar and append it in one step.  ``asdf_sequence_append_string`` takes an
 * explicit byte length; ``asdf_sequence_append_string0`` expects a
 * NUL-terminated string.  ``asdf_sequence_append_null`` takes no value
 * argument.  All other variants accept the corresponding C type directly.
 *
 * ``asdf_sequence_append_mapping`` and ``asdf_sequence_append_sequence``
 * transfer ownership of the supplied container to the parent sequence on
 * success.
 *
 * All functions return ``ASDF_VALUE_OK`` on success or an `asdf_value_err_t`
 * error code on failure.
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
 * Create a new sequence pre-populated from a C array in a single call
 *
 * Each ``asdf_sequence_of_<type>`` function allocates a new sequence attached
 * to ``file`` and appends ``size`` elements from ``arr``.  On success the
 * caller owns the returned sequence and must eventually release it with
 * `asdf_sequence_destroy` (or transfer ownership by inserting it into a
 * mapping or parent sequence).  Returns ``NULL`` on allocation failure.
 *
 * ``asdf_sequence_of_null`` creates a sequence of ``size`` null values; it
 * takes no array argument.
 *
 * ``asdf_sequence_of_string`` accepts an array of ``(str, len)`` pairs via
 * separate ``arr`` and ``lens`` pointer arguments.
 * ``asdf_sequence_of_string0`` is the null-terminated-string variant.
 *
 * All other variants mirror the corresponding ``asdf_sequence_append_<type>``
 * scalar types.
 */
ASDF_EXPORT asdf_sequence_t *asdf_sequence_of_null(asdf_file_t *file, int size);

ASDF_EXPORT asdf_sequence_t *asdf_sequence_of_string(
    asdf_file_t *file, const char *const *arr, const size_t *lens, int size);
ASDF_EXPORT asdf_sequence_t *asdf_sequence_of_string0(
    asdf_file_t *file, const char *const *arr, int size);

ASDF_EXPORT asdf_sequence_t *asdf_sequence_of_bool(asdf_file_t *file, const bool *arr, int size);

ASDF_EXPORT asdf_sequence_t *asdf_sequence_of_int8(asdf_file_t *file, const int8_t *arr, int size);
ASDF_EXPORT asdf_sequence_t *asdf_sequence_of_int16(
    asdf_file_t *file, const int16_t *arr, int size);
ASDF_EXPORT asdf_sequence_t *asdf_sequence_of_int32(
    asdf_file_t *file, const int32_t *arr, int size);
ASDF_EXPORT asdf_sequence_t *asdf_sequence_of_int64(
    asdf_file_t *file, const int64_t *arr, int size);

ASDF_EXPORT asdf_sequence_t *asdf_sequence_of_uint8(
    asdf_file_t *file, const uint8_t *arr, int size);
ASDF_EXPORT asdf_sequence_t *asdf_sequence_of_uint16(
    asdf_file_t *file, const uint16_t *arr, int size);
ASDF_EXPORT asdf_sequence_t *asdf_sequence_of_uint32(
    asdf_file_t *file, const uint32_t *arr, int size);
ASDF_EXPORT asdf_sequence_t *asdf_sequence_of_uint64(
    asdf_file_t *file, const uint64_t *arr, int size);

ASDF_EXPORT asdf_sequence_t *asdf_sequence_of_float(asdf_file_t *file, const float *arr, int size);
ASDF_EXPORT asdf_sequence_t *asdf_sequence_of_double(
    asdf_file_t *file, const double *arr, int size);


/**
 * Remove a value from a sequence and return the removed value
 *
 * If the index is greater than the size of the sequence, nothing is changed and returns
 * ``NULL``.  Items following the removed item in the sequence are shifted down.
 *
 * :param sequence: The `asdf_sequence_t *` handle
 * :param index: The index of the item to remove; negative indices are also supported (e.g.
 *     ``asdf_sequence_pop(sequence, -1)`` removes the last item
 * :return: The generic `asdf_value_t *` of the removed value if any, or NULL
 */
ASDF_EXPORT asdf_value_t *asdf_sequence_pop(asdf_sequence_t *sequence, int index);


/**
 * Iterator handle for traversing either a mapping or a sequence
 *
 * When iterating over a mapping: ``key`` holds the current key though index
 * also tracks the position in the mapping (taken as a sequence of
 * key/value pairs).  When iterating over a sequence: ``index`` holds the
 * position and ``key`` is ``NULL``.  ``value`` is always valid after a
 * successful `asdf_container_iter_next`.
 *
 * .. warning::
 *
 *   Modifying the container while iterating over it results in undefined
 *   behavior.
 */
typedef struct {
    /** Current key (mappings only; NULL for sequences) */
    const char *key;
    /** Current index */
    int index;
    /** Current value */
    asdf_value_t *value;
} asdf_container_iter_t;

/**
 * Create a new iterator over ``container`` (mapping or sequence)
 *
 * Returns ``NULL`` if ``container`` is neither a mapping nor a sequence.
 *
 * :param container: `asdf_value_t *` of mapping or sequence type
 * :return: A new `asdf_container_iter_t *` handle, or ``NULL`` on failure
 */
ASDF_EXPORT asdf_container_iter_t *asdf_container_iter_init(asdf_value_t *container);

/**
 * Advance the generic container iterator to the next element
 *
 * In most cases `asdf_mapping_iter_next` or `asdf_sequence_iter_next` are
 * preferred, but this is useful when the container type is not known at the
 * call site.
 *
 * :param iter: Pointer to the iterator handle; set to ``NULL`` on exhaustion
 * :return: ``true`` if an element was found; ``false`` when iteration is done
 */
ASDF_EXPORT bool asdf_container_iter_next(asdf_container_iter_t **iter);

/**
 * Release resources held by an in-progress container iterator
 *
 * Call this only when breaking out of iteration early.  Safe to call with
 * ``NULL``.
 *
 * :param iter: The `asdf_container_iter_t *` to destroy
 */
ASDF_EXPORT void asdf_container_iter_destroy(asdf_container_iter_t *iter);

ASDF_EXPORT bool asdf_value_is_container(asdf_value_t *value);


/**
 * Generic container size
 *
 * :param value: `asdf_value_t *` containing a mapping or a sequence
 * :return: The size of the mapping or container, or -1 if the value is not a container type
 */
ASDF_EXPORT int asdf_container_size(asdf_value_t *container);


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
 * Iterator handle for depth/breadth-first tree search
 *
 * After each successful call to `asdf_value_find_iter_next`, the ``value``
 * field holds the matching value and is valid until the next call to
 * `asdf_value_find_iter_next` or `asdf_find_iter_destroy`.  If the value must
 * persist further, clone it with `asdf_value_clone`.
 */
typedef struct {
    /** Current matching value */
    asdf_value_t *value;
} asdf_find_iter_t;

/**
 * Traverse the tree breadth-first starting from ``root`` and return the first
 * value matching ``pred``
 *
 * The caller owns the returned `asdf_value_t *` and must eventually destroy it
 * with `asdf_value_destroy`.  Returns ``NULL`` if no matching value was found.
 *
 * :param root: `asdf_value_t *` handle for the root node to search from
 * :param pred: A predicate function to match the value to return; see
 *   `asdf_value_pred_t`
 * :return: The first matching `asdf_value_t *`, or ``NULL`` if not found
 */
ASDF_EXPORT asdf_value_t *asdf_value_find(asdf_value_t *root, asdf_value_pred_t pred);

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
 * Extended version of `asdf_value_find` with additional traversal options
 *
 * Like `asdf_value_find` but allows controlling traversal order, which
 * container types to descend into, and the maximum search depth.
 *
 * :param root: `asdf_value_t *` handle for the root node to search from
 * :param pred: A predicate function to match the value to return; see
 *   `asdf_value_pred_t`
 * :param depth_first: If ``true`` descend the tree in depth-first order;
 *   otherwise the tree is traversed breadth-first
 * :param descend_pred: Optional predicate (``NULL`` means descend into all
 *   containers) controlling which containers are descended into
 * :param max_depth: Maximum depth to descend; ``-1`` means no limit
 * :return: The first matching `asdf_value_t *`, or ``NULL`` if not found
 */
ASDF_EXPORT asdf_value_t *asdf_value_find_ex(
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
 * Equivalent to passing ``NULL``, meaning always descend into nested mappings
 * and sequences.
 */
#define asdf_find_descend_all ((asdf_value_pred_t)NULL)


/**
 * Create an iterator that traverses the tree from ``root`` breadth-first,
 * yielding values matching ``pred``
 *
 * The ``root`` must be a mapping or sequence; for scalar values use
 * `asdf_value_find` directly.
 *
 * :param root: Container `asdf_value_t *` to search from
 * :param pred: Predicate selecting which values to yield; see `asdf_value_pred_t`
 * :return: A new `asdf_find_iter_t *`, or ``NULL`` on failure
 */
ASDF_EXPORT asdf_find_iter_t *asdf_find_iter_init(asdf_value_t *root, asdf_value_pred_t pred);


/**
 * Like `asdf_find_iter_init` with additional traversal options
 *
 * :param root: Container `asdf_value_t *` to search from
 * :param pred: Predicate selecting which values to yield
 * :param depth_first: If ``true`` traverse depth-first; otherwise breadth-first
 * :param descend_pred: Optional predicate controlling which containers to descend
 * :param max_depth: Maximum search depth; ``-1`` means no limit
 * :return: A new `asdf_find_iter_t *`, or ``NULL`` on failure
 */
ASDF_EXPORT asdf_find_iter_t *asdf_find_iter_init_ex(
    asdf_value_t *root,
    asdf_value_pred_t pred,
    bool depth_first,
    asdf_value_pred_t descend_pred,
    ssize_t max_depth);


/**
 * Advance the find iterator to the next matching value
 *
 * On success, `asdf_find_iter_t.value` is updated.  When iteration is
 * exhausted the iterator is freed and ``*iter`` is set to ``NULL``.
 *
 * Typical usage:
 *
 * .. code:: c
 *
 *   asdf_find_iter_t *iter = asdf_find_iter_init(root, pred);
 *   while (asdf_value_find_iter_next(&iter)) {
 *       // iter->value is valid here; use asdf_value_clone if it must persist
 *   }
 *
 * :param iter: Pointer to the iterator handle; set to ``NULL`` on exhaustion
 * :return: ``true`` if a matching value was found; ``false`` when done
 */
ASDF_EXPORT bool asdf_value_find_iter_next(asdf_find_iter_t **iter);


/**
 * Release resources held by an in-progress find iterator
 *
 * Call this only when breaking out of iteration early.  Safe to call with
 * ``NULL``.
 *
 * :param iter: The `asdf_find_iter_t *` to destroy
 */
ASDF_EXPORT void asdf_find_iter_destroy(asdf_find_iter_t *iter);

ASDF_END_DECLS

#endif /* ASDF_VALUE_H */
