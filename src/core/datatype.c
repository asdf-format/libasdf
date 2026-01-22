#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "../error.h"
#include "../extension_util.h"
#include "../log.h"
#include "../util.h"
#include "../value.h"
#include "../yaml.h"
#include "datatype.h"


// NOLINTNEXTLINE(readability-function-cognitive-complexity)
asdf_scalar_datatype_t asdf_scalar_datatype_from_string(const char *dtype) {
    if (strncmp(dtype, "int", 3) == 0) {
        const char *p = dtype + 3;
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

    if (strncmp(dtype, "uint", 4) == 0) {
        const char *p = dtype + 4;
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

    // NOLINTNEXTLINE(readability-magic-numbers)
    if (strncmp(dtype, "float", 5) == 0) {
        // NOLINTNEXTLINE(readability-magic-numbers)
        const char *p = dtype + 5;
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

    // NOLINTNEXTLINE(readability-magic-numbers)
    if (strncmp(dtype, "complex", 7) == 0) {
        // NOLINTNEXTLINE(readability-magic-numbers)
        const char *p = dtype + 7;
        if (*p && strspn(p, "12468") == strlen(p)) {
            if (strcmp(p, "64") == 0)
                return ASDF_DATATYPE_COMPLEX64;
            if (strcmp(p, "128") == 0)
                return ASDF_DATATYPE_COMPLEX128;
        }
        goto unknown;
    }

    if (strcmp(dtype, "bool8") == 0)
        return ASDF_DATATYPE_BOOL8;

unknown:
    return ASDF_DATATYPE_UNKNOWN;
}


const char *asdf_scalar_datatype_to_string(asdf_scalar_datatype_t datatype) {
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
    case ASDF_DATATYPE_STRUCTURED:
        return "<structured>";
    }
    UNREACHABLE();
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
        "unsupported datatype for at %s; please note "
        "that the current version only supports basic scalar numeric (non-string) "
        "datatypes",
        path);
}
#endif


static asdf_value_err_t asdf_string_datatype_parse(
    asdf_sequence_t *value, asdf_byteorder_t byteorder, asdf_datatype_t *datatype) {
    asdf_value_t *type_val = asdf_sequence_get(value, 0);
    const char *type = NULL;
    asdf_value_err_t err = ASDF_VALUE_OK;

    err = asdf_value_as_string0(type_val, &type);
    asdf_value_destroy(type_val);

    if (ASDF_VALUE_OK != err) {
        warn_unsupported_datatype(&value->value);
        return err;
    }

    asdf_value_t *size_val = asdf_sequence_get(value, 1);
    uint64_t size = 0;
    err = asdf_value_as_uint64(size_val, &size);
    asdf_value_destroy(size_val);

    if (ASDF_VALUE_OK != err) {
        warn_unsupported_datatype(&value->value);
        return err;
    }

    datatype->byteorder = byteorder;

    if (strcmp(type, "ascii") == 0) {
        datatype->type = ASDF_DATATYPE_ASCII;
    } else if (strcmp(type, "ucs4") == 0) {
        datatype->type = ASDF_DATATYPE_UCS4;
        size *= 4;
    } else {
        warn_unsupported_datatype(&value->value);
    }

    datatype->size = size;
    return err;
}


static inline asdf_byteorder_t asdf_byteorder_from_string(const char *str) {
    if (!str)
        return ASDF_BYTEORDER_INVALID;

    if (strcmp(str, "little") == 0)
        return ASDF_BYTEORDER_LITTLE;

    if (strcmp(str, "big") == 0)
        return ASDF_BYTEORDER_BIG;

    return ASDF_BYTEORDER_INVALID;
}


static inline const char *asdf_byteorder_to_string(asdf_byteorder_t byteorder) {
    switch (byteorder) {
    case ASDF_BYTEORDER_LITTLE:
        return "little";
    case ASDF_BYTEORDER_BIG:
        return "big";
    case ASDF_BYTEORDER_DEFAULT:
    case ASDF_BYTEORDER_INVALID:
    default:
        return NULL;
    }
}


asdf_value_err_t asdf_datatype_byteorder_parse(
    asdf_mapping_t *parent, const char *path, asdf_byteorder_t *out) {
    const char *byteorder_str = NULL;
    asdf_value_err_t err = asdf_get_optional_property(
        parent, path, ASDF_VALUE_STRING, NULL, (void *)&byteorder_str);

    if (!ASDF_IS_OK(err))
        return err;

    asdf_byteorder_t byteorder = asdf_byteorder_from_string(byteorder_str);

    if (byteorder == ASDF_BYTEORDER_INVALID) {
#ifdef ASDF_LOG_ENABLED
        const char *parent_path = asdf_value_path(&parent->value);
        ASDF_LOG(
            parent->value.file,
            ASDF_LOG_WARN,
            "invalid byteorder at %s/%s; "
            "defaulting to \"little\"",
            parent_path,
            path);
#endif
        byteorder = ASDF_BYTEORDER_LITTLE;
        err = ASDF_VALUE_ERR_PARSE_FAILURE;
    }
    *out = byteorder;
    return err;
}


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


asdf_value_err_t asdf_datatype_shape_parse(asdf_sequence_t *value, asdf_datatype_shape_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    uint64_t *shape = NULL;

    int ndim = asdf_sequence_size(value);

    if (ndim < 0) {
        warn_invalid_shape(&value->value);
        goto failure;
    }

    shape = calloc(ndim, sizeof(uint64_t));

    if (!shape) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *dim_val = NULL;
    size_t dim = 0;
    while ((dim_val = asdf_sequence_iter(value, &iter))) {
        if (ASDF_VALUE_OK != asdf_value_as_uint64(dim_val, &shape[dim++])) {
            warn_invalid_shape(&value->value);
            goto failure;
        }
    }

    out->ndim = ndim;
    out->shape = shape;
    return ASDF_VALUE_OK;
failure:
    free(shape);
    return err;
}


/**
 * Deserialize a complex/named field in a structured datatype like
 *
 * - name: kernel
 *   datatype: float32
 *   byteorder: big
 *   shape: [3, 3]
 *
 */
// NOLINTNEXTLINE(misc-no-recursion)
static asdf_value_err_t asdf_structured_datatype_field_parse(
    asdf_mapping_t *value, asdf_byteorder_t byteorder, asdf_datatype_t *field) {

    asdf_sequence_t *shape_seq = NULL;
    asdf_datatype_shape_t shape = {0};
    asdf_value_err_t err = ASDF_VALUE_OK;
    asdf_value_t *datatype_val = asdf_mapping_get(value, "datatype");

    // Get the datatype of the field, which itself may be a nested
    // core/ndarray#/definitions/datatype
    err = asdf_datatype_parse(datatype_val, byteorder, field);
    asdf_value_destroy(datatype_val);

    if (ASDF_IS_ERR(err))
        return err;

    err = asdf_get_optional_property(value, "name", ASDF_VALUE_STRING, NULL, (void *)&field->name);

#ifdef ASDF_LOG_ENABLED
    if (!ASDF_IS_OPTIONAL_OK(err)) {
        const char *path = asdf_value_path(&value->value);
        ASDF_LOG(value->value.file, ASDF_LOG_WARN, "invalid name field in datatype at %s", path);
    }
#endif

    err = asdf_datatype_byteorder_parse(value, "byteorder", &field->byteorder);

    if (!ASDF_IS_OPTIONAL_OK(err))
        goto cleanup;

    // A datatype field can also be dimensionful in its own right (otherwise .ndim = 0,
    // .shape = NULL)
    err = asdf_get_optional_property(value, "shape", ASDF_VALUE_SEQUENCE, NULL, (void *)&shape_seq);

    if (ASDF_IS_OK(err)) {
        err = asdf_datatype_shape_parse(shape_seq, &shape);
        if (ASDF_IS_OK(err)) {
            field->ndim = shape.ndim;
            field->shape = shape.shape;
            // Multiply the size
            for (uint32_t dim = 0; dim < shape.ndim; dim++)
                field->size *= shape.shape[dim];
        }
    }

    // Last thing we checked for was shape; if it was not found that's OK
    if (ASDF_IS_OPTIONAL_OK(err))
        err = ASDF_VALUE_OK;

cleanup:
    asdf_sequence_destroy(shape_seq);
    return err;
}


// NOLINTNEXTLINE(misc-no-recursion)
static asdf_value_err_t asdf_structured_datatype_parse(
    asdf_sequence_t *value, asdf_byteorder_t byteorder, asdf_datatype_t *datatype) {

    // If the datatype is a structured array, its fields member is an array
    // of the list of fields
    int nfields = asdf_sequence_size(value);
    asdf_datatype_t *fields = calloc(nfields + 1, sizeof(asdf_datatype_t));

    if (!fields)
        return ASDF_VALUE_ERR_OOM;

    datatype->byteorder = byteorder;
    datatype->size = 0;
    datatype->type = ASDF_DATATYPE_STRUCTURED;
    datatype->nfields = (uint32_t)nfields;
    datatype->fields = fields;

    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *item = NULL;
    int field_idx = 0;
    asdf_value_err_t err = ASDF_VALUE_OK;

    while ((item = asdf_sequence_iter(value, &iter)) != NULL) {
        asdf_datatype_t *field = &fields[field_idx];
        asdf_mapping_t *field_map = NULL;

        if (asdf_value_as_mapping(item, &field_map) == ASDF_VALUE_OK)
            err = asdf_structured_datatype_field_parse(field_map, byteorder, field);
        else
            err = asdf_datatype_parse(item, byteorder, field);

        if (UNLIKELY(err != ASDF_VALUE_OK)) {
            // Stop processing and return an error
            return err;
        }

        field_idx++;
        datatype->size += field->size;
    }
    return err;
}


// NOLINTNEXTLINE(misc-no-recursion)
asdf_value_err_t asdf_datatype_parse(
    asdf_value_t *value, asdf_byteorder_t byteorder, asdf_datatype_t *datatype) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    if (UNLIKELY(!value || !datatype))
        return err;

    asdf_sequence_t *datatype_seq = NULL;
    /* Parse string datatypes partially, but we don't currently store the string length; they are
     * not fully supported.  Structured datatypes are not supported at all but are at least
     * indicated as structured.
     */
    if (asdf_value_as_sequence(value, &datatype_seq) == ASDF_VALUE_OK) {
        // A length 2 array where the second element is an integer value should be a string
        // datatype.  Any other array is a structured datatype
        bool is_string_datatype = false;

        if (asdf_sequence_size(datatype_seq) == 2) {
            asdf_value_t *stringlen = asdf_sequence_get(datatype_seq, 1);
            if (stringlen && asdf_value_is_uint64(stringlen)) {
                is_string_datatype = true;
            }
            asdf_value_destroy(stringlen);
        }

        if (is_string_datatype)
            err = asdf_string_datatype_parse(datatype_seq, byteorder, datatype);
        else
            err = asdf_structured_datatype_parse(datatype_seq, byteorder, datatype);

        return err;
    }

    // Initialize an unknown datatype and fill in the type if it's a known scalar type
    // Otherwise the datatype must be a string
    const char *dtype = NULL;

    if (ASDF_VALUE_OK != asdf_value_as_string0(value, &dtype)) {
        warn_unsupported_datatype(value);
        return ASDF_VALUE_ERR_PARSE_FAILURE;
    }

    asdf_scalar_datatype_t type = asdf_scalar_datatype_from_string(dtype);

#ifdef ASDF_LOG_ENABLED
    if (type == ASDF_DATATYPE_UNKNOWN) {
        const char *path = asdf_value_path(value);
        ASDF_LOG(value->file, ASDF_LOG_WARN, "unknown datatype for ndarray at %s: %s", path, dtype);
    }
#endif

    datatype->byteorder = byteorder;
    datatype->size = asdf_scalar_datatype_size(type);
    datatype->type = type;
    return ASDF_VALUE_OK;
}


/**
 * Free resources allocated for an asdf_datatype_t
 *
 * Later, however, we may want users to be able to build datatypes (for writing new files)
 * so we may make this available as part of a more extensive datatype API.
 */
// NOLINTNEXTLINE(misc-no-recursion)
void asdf_datatype_clean(asdf_datatype_t *datatype) {
    if (!datatype)
        return;

    if (datatype->shape)
        free((size_t *)datatype->shape);

    if (datatype->fields) {
        for (uint32_t field_idx = 0; field_idx < datatype->nfields; field_idx++)
            asdf_datatype_clean((asdf_datatype_t *)&datatype->fields[field_idx]);
        free((asdf_datatype_t *)datatype->fields);
    }
    ZERO_MEMORY(datatype, sizeof(asdf_datatype_t));
}


static void asdf_datatype_dealloc(void *datatype) {
    asdf_datatype_clean(datatype);
    free(datatype);
}


/**
 */
static asdf_value_err_t asdf_datatype_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    if (UNLIKELY(!value))
        return ASDF_VALUE_ERR_TYPE_MISMATCH;

    asdf_datatype_t *datatype = calloc(1, sizeof(asdf_datatype_t));

    if (UNLIKELY(!datatype)) {
        ASDF_ERROR_OOM(value->file);
        return ASDF_VALUE_ERR_OOM;
    }

    // WARN: Unless otherwise specified, scalar datatypes are assumed to be
    // in little-endian byteorder; this is an ambiguity / underspecification
    // in the standard;
    // see https://github.com/asdf-format/asdf-standard/issues/501
    asdf_value_err_t err = asdf_datatype_parse(value, ASDF_BYTEORDER_LITTLE, datatype);

    if (err != ASDF_VALUE_OK) {
        asdf_datatype_dealloc(datatype);
    } else if (out) {
        *out = datatype;
    }

    return err;
}


// TODO: Might be useful to add to public API
static inline bool asdf_datatype_is_structured(const asdf_datatype_t *datatype) {
    return datatype->type == ASDF_DATATYPE_STRUCTURED;
}


static inline bool asdf_datatype_is_scalar(const asdf_datatype_t *datatype) {
    return datatype->type != ASDF_DATATYPE_STRUCTURED;
}


static inline bool asdf_datatype_is_simple_scalar(const asdf_datatype_t *datatype) {
    return (
        datatype->type != ASDF_DATATYPE_STRUCTURED &&
        (datatype->byteorder == ASDF_BYTEORDER_DEFAULT ||
         datatype->byteorder == ASDF_BYTEORDER_LITTLE) &&
        datatype->name == NULL && datatype->ndim == 0 && datatype->nfields == 0);
}


static inline bool asdf_datatype_is_string(const asdf_datatype_t *datatype) {
    return datatype->type == ASDF_DATATYPE_ASCII || datatype->type == ASDF_DATATYPE_UCS4;
}


// Forward declaration
static asdf_value_err_t asdf_datatype_serialize_impl(
    asdf_file_t *file, const asdf_datatype_t *datatype, bool is_field, asdf_value_t **out);


static asdf_value_err_t asdf_datatype_serialize_string(
    asdf_file_t *file, const asdf_datatype_t *datatype, asdf_value_t **out) {
    assert(file);
    assert(out);

    asdf_value_t *value = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_EMIT_FAILURE;
    asdf_sequence_t *datatype_seq = asdf_sequence_create(file);

    if (!datatype_seq) {
        err = ASDF_VALUE_ERR_OOM;
        goto cleanup;
    }

    err = asdf_sequence_append_string0(
        datatype_seq, asdf_scalar_datatype_to_string(datatype->type));

    if (err != ASDF_VALUE_OK)
        goto cleanup;

    uint32_t size = datatype->size;

    if (datatype->type == ASDF_DATATYPE_UCS4) {
        if (size % 4 != 0) {
            ASDF_LOG(
                file,
                ASDF_LOG_ERROR,
                "size of UCS4 datatypes is expected to be a multiple of 4 (got %d); "
                "the datatype will not be serialized",
                size);
            err = ASDF_VALUE_ERR_EMIT_FAILURE;
            goto cleanup;
        }
        size /= 4;
    }

    err = asdf_sequence_append_uint32(datatype_seq, size);

    if (err != ASDF_VALUE_OK)
        goto cleanup;

    asdf_sequence_set_style(datatype_seq, ASDF_YAML_NODE_STYLE_FLOW);
    value = asdf_value_of_sequence(datatype_seq);
cleanup:
    if (err != ASDF_VALUE_OK)
        asdf_sequence_destroy(datatype_seq);
    else
        *out = value;

    return err;
}


static asdf_value_err_t asdf_datatype_serialize_scalar(
    asdf_file_t *file, const asdf_datatype_t *datatype, asdf_value_t **out) {
    assert(file);
    assert(out);

    asdf_value_t *value = NULL;
    asdf_value_err_t err = ASDF_VALUE_OK;

    if (asdf_datatype_is_string(datatype)) {
        err = asdf_datatype_serialize_string(file, datatype, &value);

        if (err != ASDF_VALUE_OK)
            return err;
    } else {
        value = asdf_value_of_string0(file, asdf_scalar_datatype_to_string(datatype->type));

        if (!value)
            return ASDF_VALUE_ERR_OOM;
    }

    *out = value;
    return err;
}


// NOLINTNEXTLINE(misc-no-recursion)
static asdf_value_err_t asdf_datatype_serialize_field(
    asdf_file_t *file, const asdf_datatype_t *field, asdf_value_t **out) {
    assert(file);
    assert(field);
    assert(out);
    asdf_value_t *value = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_EMIT_FAILURE;
    asdf_mapping_t *field_map = asdf_mapping_create(file);
    asdf_sequence_t *shape_seq = NULL;
    asdf_value_t *datatype_val = NULL;

    if (!field_map) {
        err = ASDF_VALUE_ERR_OOM;
        goto cleanup;
    }

    if (field->name) {
        err = asdf_mapping_set_string0(field_map, "name", field->name);

        if (err != ASDF_VALUE_OK)
            goto cleanup;
    }

    if (asdf_datatype_is_scalar(field))
        err = asdf_datatype_serialize_scalar(file, field, &datatype_val);
    else
        err = asdf_datatype_serialize_impl(file, field, false, &datatype_val);

    if (err != ASDF_VALUE_OK)
        goto cleanup;

    err = asdf_mapping_set(field_map, "datatype", datatype_val);

    if (err != ASDF_VALUE_OK)
        goto cleanup;

    if (field->byteorder != ASDF_BYTEORDER_DEFAULT) {
        err = asdf_mapping_set_string0(
            field_map, "byteorder", asdf_byteorder_to_string(field->byteorder));

        if (err != ASDF_VALUE_OK)
            goto cleanup;
    }

    if (field->ndim > 0) {
        shape_seq = asdf_sequence_create(file);

        if (!shape_seq) {
            err = ASDF_VALUE_ERR_OOM;
            goto cleanup;
        }

        for (uint32_t idx = 0; idx < field->ndim; idx++) {
            err = asdf_sequence_append_uint32(shape_seq, field->shape[idx]);

            if (err != ASDF_VALUE_OK)
                goto cleanup;
        }

        asdf_sequence_set_style(shape_seq, ASDF_YAML_NODE_STYLE_FLOW);
        err = asdf_mapping_set_sequence(field_map, "shape", shape_seq);

        if (err != ASDF_VALUE_OK)
            goto cleanup;
    }

    // Determine best node style to use for the field. Would be good to
    // more precisely reverse-engineer how Python asdf determines this (or if
    // it's just using the defaults from the yaml library) but ISTM: if the
    // datatype is a non-string scalar and there isn't a shape, then it writes
    // it in flow style; otherwise in block style
    asdf_yaml_node_style_t style = ASDF_YAML_NODE_STYLE_AUTO;

    if (asdf_datatype_is_scalar(field) && !asdf_datatype_is_string(field) && field->ndim == 0)
        style = ASDF_YAML_NODE_STYLE_FLOW;

    asdf_mapping_set_style(field_map, style);
    value = asdf_value_of_mapping(field_map);
cleanup:
    if (err != ASDF_VALUE_OK) {
        asdf_mapping_destroy(field_map);
        asdf_sequence_destroy(shape_seq);
        asdf_value_destroy(datatype_val);
    } else {
        *out = value;
    }

    return err;
}


// TODO: Since this uses recursion it would not be a bad idea to set a
// maximum nesting level, though I'm not sure the standard actually defines one

// NOLINTNEXTLINE(misc-no-recursion)
static asdf_value_err_t asdf_datatype_serialize_impl(
    asdf_file_t *file, const asdf_datatype_t *datatype, bool is_field, asdf_value_t **out) {
    assert(file);
    assert(datatype);
    assert(out);
    asdf_value_t *value = NULL;
    asdf_value_t *field = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_EMIT_FAILURE;
    asdf_sequence_t *datatype_seq = NULL;

    if (asdf_datatype_is_simple_scalar(datatype)) {
        err = asdf_datatype_serialize_scalar(file, datatype, &value);
    } else if (is_field) {
        err = asdf_datatype_serialize_field(file, datatype, &value);
    } else if (asdf_datatype_is_structured(datatype)) {
        datatype_seq = asdf_sequence_create(file);

        if (!datatype_seq) {
            err = ASDF_VALUE_ERR_OOM;
            goto cleanup;
        }

        if (datatype->nfields == 0)
            err = ASDF_VALUE_OK;

        for (uint32_t idx = 0; idx < datatype->nfields; idx++) {
            err = asdf_datatype_serialize_impl(file, &datatype->fields[idx], true, &field);

            if (err != ASDF_VALUE_OK)
                goto cleanup;

            err = asdf_sequence_append(datatype_seq, field);

            if (err != ASDF_VALUE_OK)
                goto cleanup;
        }

        value = asdf_value_of_sequence(datatype_seq);
    } else {
        ASDF_LOG(
            file,
            ASDF_LOG_ERROR,
            "non-trivial datatype fields are not allowed at this level of nesting (it must "
            "appear in an array datatype); the datatype will not be written");
    }

cleanup:
    if (err != ASDF_VALUE_OK) {
        asdf_sequence_destroy(datatype_seq);
        asdf_value_destroy(field);
    } else {
        *out = value;
    }

    return err;
}


static asdf_value_t *asdf_datatype_serialize(
    asdf_file_t *file,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    const void *obj,
    UNUSED(const void *userdata)) {

    if (UNLIKELY(!file || !obj))
        return NULL;

    asdf_value_t *value = NULL;
    asdf_value_err_t err = asdf_datatype_serialize_impl(
        file, (const asdf_datatype_t *)obj, false, &value);

    if (err != ASDF_VALUE_OK)
        return NULL;

    return value;
}


// Declare the extension for datatype-1.0.0
ASDF_REGISTER_EXTENSION(
    datatype,
    ASDF_CORE_DATATYPE_TAG,
    asdf_datatype_t,
    &libasdf_software,
    asdf_datatype_serialize,
    asdf_datatype_deserialize,
    asdf_datatype_dealloc,
    NULL);
