/*
 * Structures and methods pertaining to ASDF tree values
 */
#ifndef ASDF_VALUE_H
#define ASDF_VALUE_H

#include <stdbool.h>
#include <stdint.h>

#include <asdf/util.h>

ASDF_BEGIN_DECLS


typedef enum {
    ASDF_VALUE_UNKNOWN,
    ASDF_VALUE_SEQUENCE,
    ASDF_VALUE_MAPPING,
    // Generic scalar, before coercion
    ASDF_VALUE_SCALAR,

    // The rest are all possible scalar types
    ASDF_VALUE_STRING,
    ASDF_VALUE_BOOL,
    ASDF_VALUE_NULL,

    // Signed integers
    ASDF_VALUE_INT8,
    ASDF_VALUE_INT16,
    ASDF_VALUE_INT32,
    ASDF_VALUE_INT64,

    // Unsigned integers
    ASDF_VALUE_UINT8,
    ASDF_VALUE_UINT16,
    ASDF_VALUE_UINT32,
    ASDF_VALUE_UINT64,

    // Floating point
    ASDF_VALUE_FLOAT,
    ASDF_VALUE_DOUBLE,

    // Extension types (to be defined in more detail later)
    ASDF_VALUE_EXTENSION
} asdf_value_type_t;


typedef enum {
    ASDF_VALUE_ERR_UNKNOWN = -2,
    ASDF_VALUE_ERR_NOT_FOUND = -1,
    ASDF_VALUE_OK = 0,
    ASDF_VALUE_ERR_TYPE_MISMATCH,
    ASDF_VALUE_ERR_PARSE_FAILURE,
    ASDF_VALUE_ERR_OVERFLOW,
    ASDF_VALUE_ERR_TAG_MISMATCH,
    ASDF_VALUE_ERR_OOM,
} asdf_value_err_t;


/* Basic value-related definitions */
typedef struct asdf_value asdf_value_t;

ASDF_EXPORT void asdf_value_destroy(asdf_value_t *value);
ASDF_EXPORT asdf_value_t *asdf_value_clone(asdf_value_t *value);
ASDF_EXPORT asdf_value_type_t asdf_value_get_type(asdf_value_t *value);
ASDF_EXPORT const char *asdf_value_path(asdf_value_t *value);

/* Return the value's tag if it has an *explicit* tag (implict tags are not returned) */
ASDF_EXPORT const char *asdf_value_tag(asdf_value_t *value);

/* Mapping-related definitions */
ASDF_EXPORT bool asdf_value_is_mapping(asdf_value_t *value);
ASDF_EXPORT int asdf_mapping_size(asdf_value_t *value);
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
typedef struct asdf_extension asdf_extension_t;
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
