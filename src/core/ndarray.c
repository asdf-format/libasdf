#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include <asdf/core/asdf.h>
#define ASDF_CORE_NDARRAY_INTERNAL
#include <asdf/core/ndarray.h>
#undef ASDF_CORE_NDARRAY_INTERNAL
#include <asdf/extension.h>

#include "../file.h"
#include "../log.h"
#include "../util.h"
#include "../value.h"
#include "ndarray_convert.h"


/** Internal definition of the asdf_ndarray_t type with extended internal fields */
typedef struct asdf_ndarray {
    size_t source;
    uint32_t ndim;
    uint64_t *shape;
    asdf_datatype_t datatype;
    asdf_byteorder_t byteorder;
    uint64_t offset;
    int64_t *strides;

    // Internal fields
    asdf_block_t *block;
    asdf_file_t *file;
} asdf_ndarray_t;


/* Helper to look up required properties and log a warning if missing */
static asdf_value_t *get_required_property(asdf_value_t *mapping, const char *name) {
    asdf_value_t *prop = asdf_mapping_get(mapping, name);
#ifdef ASDF_LOG_ENABLED
    if (!prop) {
        const char *path = asdf_value_path(prop);
        ASDF_LOG(
            mapping->file,
            ASDF_LOG_WARN,
            "required property %s missing from ndarray at %s",
            name,
            path);
    }
#endif
    return prop;
}


#ifdef ASDF_LOG_ENABLED
static void warn_unsupported_datatype(UNUSED(asdf_value_t *value)) {
}
#else
static void warn_unsupported_datatype(asdf_value_t *value) {
    const char *path = asdf_value_path(value);
    ASDF_LOG(
        value->file,
        ASDF_LOG_WARN,
        "unsupported datatype for ndarray at %s; please note "
        "that the current version only supports basic scalar numeric (non-string) "
        "datatypes",
        path);
}
#endif


asdf_scalar_datatype_t asdf_ndarray_datatype_from_string(const char *s) {
    if (strncmp(s, "int", 3) == 0) {
        const char *p = s + 3;
        if (*p && strspn(p, "123468") == strlen(p)) {
            if (strcmp(p, "8") == 0)
                return ASDF_DATATYPE_INT8;
            if (strcmp(p, "16") == 0)
                return ASDF_DATATYPE_INT16;
            if (strcmp(p, "32") == 0)
                return ASDF_DATATYPE_INT32;
            if (strcmp(p, "64") == 0)
                return ASDF_DATATYPE_INT64;
        }
        goto unknown;
    }

    if (strncmp(s, "uint", 4) == 0) {
        const char *p = s + 4;
        if (*p && strspn(p, "123468") == strlen(p)) {
            if (strcmp(p, "8") == 0)
                return ASDF_DATATYPE_UINT8;
            if (strcmp(p, "16") == 0)
                return ASDF_DATATYPE_UINT16;
            if (strcmp(p, "32") == 0)
                return ASDF_DATATYPE_UINT32;
            if (strcmp(p, "64") == 0)
                return ASDF_DATATYPE_UINT64;
        }
        goto unknown;
    }

    if (strncmp(s, "float", 5) == 0) {
        const char *p = s + 5;
        if (*p && strspn(p, "12346") == strlen(p)) {
            if (strcmp(p, "16") == 0)
                return ASDF_DATATYPE_FLOAT16;
            if (strcmp(p, "32") == 0)
                return ASDF_DATATYPE_FLOAT32;
            if (strcmp(p, "64") == 0)
                return ASDF_DATATYPE_FLOAT64;
        }
        goto unknown;
    }

    if (strncmp(s, "complex", 7) == 0) {
        const char *p = s + 7;
        if (*p && strspn(p, "12468") == strlen(p)) {
            if (strcmp(p, "64") == 0)
                return ASDF_DATATYPE_COMPLEX64;
            if (strcmp(p, "128") == 0)
                return ASDF_DATATYPE_COMPLEX128;
        }
        goto unknown;
    }

    if (strcmp(s, "bool8") == 0)
        return ASDF_DATATYPE_BOOL8;

unknown:
    return ASDF_DATATYPE_UNKNOWN;
}


/**
 * Free resources allocated for an asdf_datatype_t
 *
 * This is not meant to be called by users as the `asdf_datatype_t` type, for now, does not
 * exist outside an `asdf_ndarray_t`
 *
 * Later, however, we may want users to be able to build datatypes (for writing new files)
 * so we may make this available as part of a more extensive datatype API.
 */
static void asdf_datatype_clean(asdf_datatype_t *datatype) {
    if (datatype->shape)
        free((size_t *)datatype->shape);

    if (datatype->fields) {
        for (uint32_t field_idx = 0; field_idx < datatype->nfields; field_idx++)
            asdf_datatype_clean((asdf_datatype_t *)&datatype->fields[field_idx]);
        free((asdf_datatype_t *)datatype->fields);
    }
    ZERO_MEMORY(datatype, sizeof(asdf_datatype_t));
}


static asdf_value_err_t asdf_ndarray_parse_string_datatype(
    asdf_value_t *value, asdf_byteorder_t byteorder, asdf_datatype_t *datatype) {
    asdf_value_t *type_val = asdf_sequence_get(value, 0);
    const char *type = NULL;
    asdf_value_err_t err = ASDF_VALUE_OK;

    err = asdf_value_as_string0(type_val, &type);
    asdf_value_destroy(type_val);

    if (ASDF_VALUE_OK != err) {
        warn_unsupported_datatype(value);
        return err;
    }

    asdf_value_t *size_val = asdf_sequence_get(value, 1);
    uint64_t size = 0;
    err = asdf_value_as_uint64(size_val, &size);
    asdf_value_destroy(size_val);

    if (ASDF_VALUE_OK != err) {
        warn_unsupported_datatype(value);
        return err;
    }

    datatype->byteorder = byteorder;

    if (strcmp(type, "ascii") == 0) {
        datatype->type = ASDF_DATATYPE_ASCII;
    } else if (strcmp(type, "ucs4") == 0) {
        datatype->type = ASDF_DATATYPE_UCS4;
        size *= 4;
    } else {
        warn_unsupported_datatype(value);
    }

    datatype->size = size;
    return err;
}


/**
 * Internal structure for representing a shape and number of dimensions
 *
 * Returned by `asdf_ndarray_deserialize_shape`
 */
typedef struct {
    uint32_t ndim;
    uint64_t *shape;
} asdf_shape_t;


#ifdef ASDF_LOG_ENABLED
static void warn_invalid_shape(UNUSED(asdf_value_t *value)) {
}
#else
static void warn_invalid_shape(asdf_value_t *value) {
    const char *path = asdf_value_path(value);
    ASDF_LOG(
        value->file,
        ASDF_LOG_WARN,
        "invalid shape for ndarray at %s; must be an array of"
        "positive integers",
        path);
}
#endif


static asdf_value_err_t asdf_ndarray_deserialize_shape(asdf_value_t *value, asdf_shape_t *out) {
    asdf_value_err_t err = ASDF_VALUE_OK;
    uint64_t *shape = NULL;

    if (!asdf_value_is_sequence(value)) {
        warn_invalid_shape(value);
        goto failure;
    }

    int ndim = asdf_sequence_size(value);

    if (ndim < 0) {
        warn_invalid_shape(value);
        goto failure;
    }

    shape = malloc(ndim * sizeof(uint64_t));

    if (!shape) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *dim_val = NULL;
    size_t dim = 0;
    while ((dim_val = asdf_sequence_iter(value, &iter))) {
        if (ASDF_VALUE_OK != asdf_value_as_uint64(dim_val, &shape[dim++])) {
            warn_invalid_shape(value);
            goto failure;
        }
    }

    out->ndim = ndim;
    out->shape = shape;
    return err;
failure:
    free(shape);
    return err;
}


#ifdef ASDF_LOG_ENABLED
static void warn_invalid_strides(UNUSED(asdf_value_t *value)) {
}
#else
static void warn_invalid_strides(asdf_value_t *value) {
    const char *path = asdf_value_path(value);
    ASDF_LOG(
        value->file,
        ASDF_LOG_WARN,
        "invalid strides for ndarray at %s; must be an array of"
        "non-zero integers with the same length as shape",
        path);
}
#endif


/**
 * Almost the same as asdf_ndarray_deserialize_shape, but it depends on
 * already knowing the *shape* of the ndarray, and the validation is slightly
 * different
 */
static asdf_value_err_t asdf_ndarray_deserialize_strides(
    asdf_value_t *value, uint32_t ndim, int64_t **out) {
    asdf_value_err_t err = ASDF_VALUE_OK;
    int64_t *strides = NULL;

    if (!asdf_value_is_sequence(value)) {
        warn_invalid_strides(value);
        goto failure;
    }

    int nstrides = asdf_sequence_size(value);

    if (nstrides < 0 || (uint32_t)nstrides != ndim) {
        warn_invalid_strides(value);
        goto failure;
    }

    strides = malloc(ndim * sizeof(uint64_t));

    if (!strides) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    asdf_sequence_iter_t stride_iter = asdf_sequence_iter_init();
    asdf_value_t *stride_val = NULL;
    size_t dim = 0;
    while ((stride_val = asdf_sequence_iter(value, &stride_iter))) {
        if (ASDF_VALUE_OK != asdf_value_as_int64(stride_val, &strides[dim])) {
            warn_invalid_strides(value);
            goto failure;
        }

        if (0 == strides[dim]) {
            warn_invalid_strides(value);
            goto failure;
        }
        dim++;
    }

    *out = strides;
    return err;
failure:
    free(strides);
    return err;
}


asdf_byteorder_t asdf_ndarray_deserialize_byteorder(asdf_value_t *value) {
    if (!value) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(value);
        ASDF_LOG(
            value->file,
            ASDF_LOG_WARN,
            "byteorder not specified for ndarray at %s; "
            "defaulting to \"little\"",
            path);
#endif
        return ASDF_BYTEORDER_LITTLE;
    }

    const char *s = NULL;

    if (ASDF_VALUE_OK != asdf_value_as_string0(value, &s)) {
        goto invalid;
    }

    if (s && (strcmp(s, "little") == 0))
        return ASDF_BYTEORDER_LITTLE;
    else if (s && (strcmp(s, "big") == 0))
        return ASDF_BYTEORDER_BIG;

invalid : {
#ifdef ASDF_LOG_ENABLED
    const char *path = asdf_value_path(value);
    ASDF_LOG(
        value->file,
        ASDF_LOG_WARN,
        "invalid byteorder for ndarray at %s; "
        "defaulting to \"little\"",
        path);
#endif
}
    return ASDF_BYTEORDER_LITTLE;
}


// Forward-declaration
static asdf_value_err_t asdf_ndarray_parse_datatype(
    asdf_value_t *value, asdf_byteorder_t byteorder, asdf_datatype_t *datatype);


/**
 * Deserialize a complex/named field in a record datatype like
 *
 * - name: kernel
 *   datatype: float32
 *   byteorder: big
 *   shape: [3, 3]
 *
 */
static asdf_value_err_t asdf_ndarray_parse_field_datatype(
    asdf_value_t *value, asdf_byteorder_t byteorder, asdf_datatype_t *field) {

    if (!asdf_value_is_mapping(value))
        return ASDF_VALUE_ERR_PARSE_FAILURE;

    asdf_value_err_t err = ASDF_VALUE_OK;

    // Get the datatype of the field, which itself may be a nested
    // core/ndarray#/definitions/datatype
    asdf_value_t *datatype_val = asdf_mapping_get(value, "datatype");
    err = asdf_ndarray_parse_datatype(datatype_val, byteorder, field);
    asdf_value_destroy(datatype_val);

    if (UNLIKELY(err != ASDF_VALUE_OK))
        return err;

    // Get the name property, if present / valid
    asdf_value_t *prop = asdf_mapping_get(value, "name");

    if (prop) {
        if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &field->name)) {
#ifdef ASDF_LOG_ENABLED
            const char *path = asdf_value_path(value);
            ASDF_LOG(value->file, ASDF_LOG_WARN, "invalid name field in datatype at %s", path);
        }
#endif
        asdf_value_destroy(prop);
    }

    // Get the byteorder property, if present (otherwise keep the ndarray's byteorder
    // already passed through)
    if ((prop = asdf_mapping_get(value, "byteorder"))) {
        byteorder = asdf_ndarray_deserialize_byteorder(prop);
        asdf_value_destroy(prop);
    }

    field->byteorder = byteorder;

    // A datatype field can also be dimensionful in its own right (otherwise .ndim = 0,
    // .shape = NULL)
    if ((prop = asdf_mapping_get(value, "shape"))) {
        asdf_shape_t shape = {0};
        if (asdf_ndarray_deserialize_shape(prop, &shape) == ASDF_VALUE_OK) {
            field->ndim = shape.ndim;
            field->shape = shape.shape;
            // Multiply the size
            for (uint32_t dim = 0; dim < shape.ndim; dim++)
                field->size *= shape.shape[dim];
        }
        asdf_value_destroy(prop);
    }

    return err;
}


static asdf_value_err_t asdf_ndarray_parse_record_datatype(
    asdf_value_t *value, asdf_byteorder_t byteorder, asdf_datatype_t *datatype) {
    if (!asdf_value_is_sequence(value))
        return ASDF_VALUE_ERR_PARSE_FAILURE;

    // If the datatype is a record array, its fields member is an array
    // of the list of fields
    int nfields = asdf_sequence_size(value);
    asdf_datatype_t *fields = calloc(nfields + 1, sizeof(asdf_datatype_t));

    if (!fields)
        return ASDF_VALUE_ERR_OOM;

    datatype->byteorder = byteorder;
    datatype->size = 0;
    datatype->type = ASDF_DATATYPE_RECORD;
    datatype->nfields = (uint32_t)nfields;
    datatype->fields = fields;

    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *item = NULL;
    int field_idx = 0;
    asdf_value_err_t err = ASDF_VALUE_OK;

    while ((item = asdf_sequence_iter(value, &iter)) != NULL) {
        asdf_datatype_t *field = &fields[field_idx];
        if (asdf_value_is_mapping(item))
            err = asdf_ndarray_parse_field_datatype(item, byteorder, field);
        else
            err = asdf_ndarray_parse_datatype(item, byteorder, field);

        if (UNLIKELY(err != ASDF_VALUE_OK)) {
            // Stop processing and return an error
            return err;
        }

        field_idx++;
        datatype->size += field->size;
    }
    return err;
}


static asdf_value_err_t asdf_ndarray_parse_datatype(
    asdf_value_t *value, asdf_byteorder_t byteorder, asdf_datatype_t *datatype) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    if (UNLIKELY(!value || !datatype))
        return err;

    /* Parse string datatypes partially, but we don't currently store the string length; they are
     * not fully supported.  Structured datatypes are not supported at all but are at least
     * indicated as structured.
     */
    if (asdf_value_is_sequence(value)) {
        // A length 2 array where the second element is an integer value should be a string
        // datatype.  Any other array is a record datatype
        bool is_string_datatype = false;

        if (asdf_sequence_size(value) == 2) {
            asdf_value_t *stringlen = asdf_sequence_get(value, 1);
            if (stringlen && asdf_value_is_uint64(stringlen)) {
                is_string_datatype = true;
            }
            asdf_value_destroy(stringlen);
        }

        if (is_string_datatype)
            err = asdf_ndarray_parse_string_datatype(value, byteorder, datatype);
        else
            err = asdf_ndarray_parse_record_datatype(value, byteorder, datatype);

        return err;
    }

    // Initialize an unknown datatype and fill in the type if it's a known scalar type
    // Otherwise the datatype must be a string
    const char *s = NULL;

    if (ASDF_VALUE_OK != asdf_value_as_string0(value, &s)) {
        warn_unsupported_datatype(value);
        return ASDF_VALUE_ERR_PARSE_FAILURE;
    }

    asdf_scalar_datatype_t type = asdf_ndarray_datatype_from_string(s);

#ifdef ASDF_LOG_ENABLED
    if (type == ASDF_DATATYPE_UNKNOWN) {
        const char *path = asdf_value_path(value);
        ASDF_LOG(value->file, ASDF_LOG_WARN, "unknown datatype for ndarray at %s: %s", path, s);
    }
#endif

    datatype->byteorder = byteorder;
    datatype->size = asdf_ndarray_scalar_datatype_size(type);
    datatype->type = type;
    return ASDF_VALUE_OK;
}


const char *asdf_ndarray_datatype_to_string(asdf_scalar_datatype_t datatype) {
    switch (datatype) {
    case ASDF_DATATYPE_UNKNOWN:
        return "<unknown>";
    case ASDF_DATATYPE_INT8:
        return "int8";
    case ASDF_DATATYPE_UINT8:
        return "uint8";
    case ASDF_DATATYPE_INT16:
        return "int16";
    case ASDF_DATATYPE_UINT16:
        return "uint16";
    case ASDF_DATATYPE_INT32:
        return "int32";
    case ASDF_DATATYPE_UINT32:
        return "uint32";
    case ASDF_DATATYPE_INT64:
        return "int64";
    case ASDF_DATATYPE_UINT64:
        return "uint64";
    case ASDF_DATATYPE_FLOAT16:
        return "float16";
    case ASDF_DATATYPE_FLOAT32:
        return "float32";
    case ASDF_DATATYPE_FLOAT64:
        return "float64";
    case ASDF_DATATYPE_COMPLEX64:
        return "complex64";
    case ASDF_DATATYPE_COMPLEX128:
        return "complex128";
    case ASDF_DATATYPE_BOOL8:
        return "bool8";
    // TODO: The remaining cases will be more usefully stringified
    // From an asdf_datatype_t object that contains their additional data...
    // Should probably refactor before first release to avoid ABI incompatibility
    // See issue #50
    case ASDF_DATATYPE_ASCII:
        return "ascii";
    case ASDF_DATATYPE_UCS4:
        return "ucs4";
    case ASDF_DATATYPE_RECORD:
        return "<record>";
    }
    UNREACHABLE();
}


static asdf_value_err_t asdf_ndarray_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;
    uint64_t source = 0;
    asdf_shape_t shape = {0};
    asdf_byteorder_t byteorder = ASDF_BYTEORDER_LITTLE;
    uint64_t offset = 0;
    int64_t *strides = NULL;

    if (!asdf_value_is_mapping(value))
        goto failure;

    /* The source field is required; currently only integer sources are allowed */
    if (!(prop = get_required_property(value, "source")))
        goto failure;

    if (ASDF_VALUE_OK != asdf_value_as_uint64(prop, &source)) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(prop);
        const char *raw = NULL;
        asdf_value_as_scalar0(prop, &raw);
        ASDF_LOG(
            value->file,
            ASDF_LOG_WARN,
            "invalid or unsupported source for ndarray at %s: "
            "\"%s\"; only positive integers are supported",
            path,
            raw);
#endif
        goto failure;
    }

    asdf_value_destroy(prop);

    /* Parse shape */
    if (!(prop = get_required_property(value, "shape")))
        goto failure;

    if ((err = asdf_ndarray_deserialize_shape(prop, &shape)) != ASDF_VALUE_OK)
        goto failure;

    asdf_value_destroy(prop);

    /* Parse byteorder */
    if (!(prop = get_required_property(value, "byteorder"))) {
        goto failure;
    }
    byteorder = asdf_ndarray_deserialize_byteorder(prop);
    asdf_value_destroy(prop);

    asdf_ndarray_t *ndarray = calloc(1, sizeof(asdf_ndarray_t));

    if (!ndarray)
        return ASDF_VALUE_ERR_OOM;

    /* Parse datatype */
    if (!(prop = get_required_property(value, "datatype")))
        goto failure;

    err = asdf_ndarray_parse_datatype(prop, byteorder, &ndarray->datatype);

    if (UNLIKELY(err != ASDF_VALUE_OK))
        goto failure;

    asdf_value_destroy(prop);

    /* Parse offset */
    if ((prop = asdf_mapping_get(value, "offset"))) {
        if (ASDF_VALUE_OK != asdf_value_as_uint64(prop, &offset)) {
            offset = 0;
#ifdef ASDF_LOG_ENABLED
            const char *path = asdf_value_path(prop);
            ASDF_LOG(value->file, ASDF_LOG_WARN, "ignoring invalid offset in ndarray at %s", path);
#endif
        }
        asdf_value_destroy(prop);
    }

    /* Parse strides */
    if ((prop = asdf_mapping_get(value, "strides"))) {
        if ((err = asdf_ndarray_deserialize_strides(prop, shape.ndim, &strides)) != ASDF_VALUE_OK) {
            goto failure;
        }
    }

    ndarray->source = source;
    ndarray->ndim = shape.ndim;
    ndarray->shape = shape.shape;
    ndarray->byteorder = byteorder;
    ndarray->offset = offset;
    ndarray->strides = strides;
    ndarray->file = value->file;
    *out = ndarray;
    return ASDF_VALUE_OK;
failure:
    asdf_value_destroy(prop);
    free(strides);
    return err;
}


static void asdf_ndarray_dealloc(void *value) {
    if (!value)
        return;

    asdf_ndarray_t *ndarray = value;
    asdf_block_close(ndarray->block);
    free(ndarray->shape);
    free(ndarray->strides);
    asdf_datatype_clean(&ndarray->datatype);
    ZERO_MEMORY(ndarray, sizeof(asdf_ndarray_t));
    free(ndarray);
}


/*
 * Define the extension for the core/ndarray-1.1.0 schema
 *
 * TODO: Also support ndarray-1.0.0
 */
ASDF_REGISTER_EXTENSION(
    ndarray,
    ASDF_CORE_TAG_PREFIX "ndarray-1.1.0",
    asdf_ndarray_t,
    &libasdf_software,
    asdf_ndarray_deserialize,
    asdf_ndarray_dealloc,
    NULL);


static inline asdf_byteorder_t asdf_host_byteorder() {
    uint16_t x = 1;
    return (*(uint8_t *)&x) == 1 ? ASDF_BYTEORDER_LITTLE : ASDF_BYTEORDER_BIG;
}


/* ndarray methods */
void *asdf_ndarray_data_raw(asdf_ndarray_t *ndarray, size_t *size) {
    if (!ndarray)
        return NULL;

    if (!ndarray->block) {
        asdf_block_t *block = asdf_block_open(ndarray->file, ndarray->source);

        if (!block)
            return NULL;

        ndarray->block = block;
    }

    return asdf_block_data(ndarray->block, size);
}

/*
 * Same as asdf_ndarray_data_raw except it optionally returns information required to decompress the data buffer
 */
void *asdf_ndarray_data_raw_ex(asdf_ndarray_t *ndarray, const char **compression, size_t *compressed_size,
                               size_t *uncompressed_size) {
    if (!ndarray)
        return NULL;

    if (!ndarray->block) {
        asdf_block_t *block = asdf_block_open(ndarray->file, ndarray->source);

        if (!block)
            return NULL;

        ndarray->block = block;
        if (compression) {
            *compression = block->info.header.compression;
        }
        if (uncompressed_size) {
            *uncompressed_size = block->info.header.data_size;
        }
    }

    return asdf_block_data(ndarray->block, compressed_size);
}

size_t asdf_ndarray_size(asdf_ndarray_t *ndarray) {
    if (UNLIKELY(!ndarray || ndarray->ndim == 0))
        return 0;

    size_t size = 1;

    for (size_t idx = 0; idx < ndarray->ndim; idx++)
        size *= ndarray->shape[idx];

    return size;
}


asdf_ndarray_err_t asdf_ndarray_read_tile_ndim(
    asdf_ndarray_t *ndarray,
    const uint64_t *origin,
    const uint64_t *shape,
    asdf_scalar_datatype_t dst_t,
    void **dst) {
    uint32_t ndim = ndarray->ndim;

    if (UNLIKELY(!dst || !ndarray || !origin || !shape))
        // Invalid argument, must be non-NULL
        return ASDF_NDARRAY_ERR_INVAL;

    asdf_scalar_datatype_t src_t = ndarray->datatype.type;

    if (dst_t == ASDF_DATATYPE_SOURCE)
        dst_t = src_t;

    ssize_t src_elsize = asdf_ndarray_scalar_datatype_size(src_t);
    ssize_t dst_elsize = asdf_ndarray_scalar_datatype_size(dst_t);

    // For not-yet-supported datatypes return ERR_INVAL
    if (src_elsize < 1 || dst_elsize < 1)
        return ASDF_NDARRAY_ERR_INVAL;

    // Check bounds
    // TODO: (Maybe? allow option for edge cases with fill values for out-of-bound pixels?
    uint64_t *array_shape = ndarray->shape;

    for (uint32_t idx = 0; idx < ndim; idx++) {
        if (origin[idx] + shape[idx] > array_shape[idx])
            return ASDF_NDARRAY_ERR_OUT_OF_BOUNDS;
    }

    size_t tile_nelems = 0;

    if (ndim > 0) {
        tile_nelems = 1;

        for (uint32_t dim = 0; dim < ndim; dim++) {
            tile_nelems *= shape[dim];
        }
    }

    size_t src_tile_size = src_elsize * tile_nelems;
    size_t tile_size = dst_elsize * tile_nelems;
    size_t data_size = 0;
    void *data = asdf_ndarray_data_raw(ndarray, &data_size);

    if (data_size < src_tile_size)
        return ASDF_NDARRAY_ERR_OUT_OF_BOUNDS;
    //
    // If the function is passed a null pointer, allocate memory for the tile ourselves
    // User is responsible for freeing it.
    void *tile = *dst;
    void *new_buf = NULL;

    if (!tile) {
        tile = malloc(tile_size);

        if (!tile)
            return ASDF_NDARRAY_ERR_OOM;

        new_buf = tile;
    }

    // Special case, if size of the array is 0 just return now.  We do still malloc though even if
    // it's a bit pointless, just to ensure that the returned pointer can be freed successfully
    if (UNLIKELY(0 == ndim || 0 == tile_size)) {
        *dst = tile;
        return ASDF_NDARRAY_OK;
    }

    // Determine element strides (assume C-order for now; ndarray->strides is not used yet)
    uint32_t inner_dim = ndim - 1;
    int64_t *strides = malloc(sizeof(int64_t) * ndim);

    if (!strides) {
        free(new_buf);
        return ASDF_NDARRAY_ERR_OOM;
    }

    strides[inner_dim] = 1;

    if (ndim > 1) {
        for (uint32_t dim = inner_dim; dim > 0; dim--)
            strides[dim - 1] = strides[dim] * array_shape[dim];
    }

    // Determine the copy strategy to use; right now this just handles whether-or-not byteswap
    // is needed, may have others depending on alignment, vectorization etc.
    bool byteswap = false;

    if (src_elsize > 1) {
        asdf_byteorder_t host_byteorder = asdf_host_byteorder();

        if (host_byteorder != ndarray->byteorder)
            byteswap = true;
    }

    asdf_ndarray_convert_fn_t convert = asdf_ndarray_get_convert_fn(src_t, dst_t, byteswap);

    if (convert == NULL) {
        const char *src_datatype = asdf_ndarray_datatype_to_string(src_t);
        const char *dst_datatype = asdf_ndarray_datatype_to_string(dst_t);
        ASDF_LOG(
            ndarray->file,
            ASDF_LOG_WARN,
            "datatype conversion from \"%s\" to \"%s\" not supported for ndarray tile copy; "
            "source bytes will be copied without conversion",
            src_datatype,
            dst_datatype);
    }

    size_t offset = origin[inner_dim];
    bool is_1d = true;

    if (ndim > 1) {
        for (uint32_t dim = 0; dim < inner_dim; dim++) {
            offset += origin[dim] * strides[dim];
            if (shape[dim] != 1) {
                // If any of the outer dimensions are >1 than it's not a 1d tile
                is_1d = false;
            }
        }
        offset *= src_elsize;
    } else {
        offset = origin[0] * src_elsize;
    }

    bool overflow = false;

    // Special case if the "tile" is one-dimensional, C-contiguous
    if (is_1d) {
        const void *src = data + offset;
        // If convert() returns non-zero it means an overflow occurred
        // while copying; this does not necessarily have to be treated as an error depending
        // on the application.
        overflow = convert(tile, src, tile_nelems, dst_elsize);
        free(strides);
        *dst = tile;

        if (overflow)
            return ASDF_NDARRAY_ERR_OVERFLOW;

        return ASDF_NDARRAY_OK;
    }

    uint64_t *odometer = malloc(sizeof(uint64_t) * inner_dim);

    if (!odometer) {
        free(strides);
        free(new_buf);
        return ASDF_NDARRAY_ERR_OOM;
    }

    memcpy(odometer, origin, sizeof(uint64_t) * inner_dim);
    bool done = false;
    uint64_t inner_nelem = shape[inner_dim];
    size_t inner_size = inner_nelem * dst_elsize;
    const void *src = data + offset;
    void *dst_tmp = tile;

    while (!done) {
        overflow = convert(dst_tmp, src, inner_nelem, dst_elsize);
        dst_tmp += inner_size;

        uint32_t dim = inner_dim - 1;
        do {
            odometer[dim]++;
            src += strides[dim] * src_elsize;

            if (odometer[dim] < origin[dim] + shape[dim]) {
                break;
            } else {
                if (dim == 0) {
                    done = true;
                    break;
                }

                odometer[dim] = origin[dim];
                // Back up
                src -= shape[dim] * strides[dim] * src_elsize;
            }
        } while (dim-- > 0);
    }

    free(odometer);
    free(strides);
    *dst = tile;

    if (overflow)
        return ASDF_NDARRAY_ERR_OVERFLOW;

    return ASDF_NDARRAY_OK;
}


asdf_ndarray_err_t asdf_ndarray_read_all(
    asdf_ndarray_t *ndarray, asdf_scalar_datatype_t dst_t, void **dst) {
    if (UNLIKELY(!ndarray))
        // Invalid argument, must be non-NULL
        return ASDF_NDARRAY_ERR_INVAL;

    const uint64_t *origin = calloc(ndarray->ndim, sizeof(uint64_t));

    if (!origin)
        return ASDF_NDARRAY_ERR_OOM;

    asdf_ndarray_err_t err =
        asdf_ndarray_read_tile_ndim(ndarray, origin, ndarray->shape, dst_t, dst);

    free((void *)origin);
    return err;
}


asdf_ndarray_err_t asdf_ndarray_read_tile_2d(
    asdf_ndarray_t *ndarray,
    uint64_t x,
    uint64_t y,
    uint64_t width,
    uint64_t height,
    const uint64_t *plane_origin,
    asdf_scalar_datatype_t dst_t,
    void **dst) {
    uint32_t ndim = ndarray->ndim;

    if (ndim < 2)
        return ASDF_NDARRAY_ERR_OUT_OF_BOUNDS;

    uint64_t *origin = calloc(ndim, sizeof(uint64_t));
    uint64_t *shape = calloc(ndim, sizeof(uint64_t));

    if (!origin || !shape)
        return ASDF_NDARRAY_ERR_OOM;

    uint32_t leading_ndim = ndim - 2;
    for (uint32_t dim = 0; dim < leading_ndim; dim++) {
        origin[dim] = plane_origin ? plane_origin[dim] : 0;
        shape[dim] = 1;
    }
    origin[ndim - 2] = y;
    origin[ndim - 1] = x;
    shape[ndim - 2] = height;
    shape[ndim - 1] = width;

    asdf_ndarray_err_t err = asdf_ndarray_read_tile_ndim(ndarray, origin, shape, dst_t, dst);
    free(origin);
    free(shape);
    return err;
}
