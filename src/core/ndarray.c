#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../context.h"
#include "../error.h"
#include "../extension_util.h"
#include "../file.h"
#include "../log.h"
#include "../util.h"
#include "../value.h"
#include "../yaml.h"

#include "asdf.h"
#include "datatype.h"
#include "ndarray.h"
#include "ndarray_convert.h"


/**
 * Return the ndarray internal data if exists, and/or optionally create and
 * return it if not
 */
static asdf_ndarray_internal_t *asdf_ndarray_internal(asdf_ndarray_t *ndarray, bool create) {
    if (UNLIKELY(!ndarray))
        return NULL;

    if (!ndarray->internal && create) {
        asdf_ndarray_internal_t *internal = calloc(1, sizeof(asdf_ndarray_internal_t));

        if (UNLIKELY(!internal))
            return NULL;

        ndarray->internal = internal;
    }

    return ndarray->internal;
}


const asdf_block_t *asdf_ndarray_block(asdf_ndarray_t *ndarray) {
    if (!ndarray || !ndarray->internal)
        return NULL;

    return ndarray->internal->block;
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
 * Almost the same as asdf_ndarray_parse_shape, but it depends on
 * already knowing the *shape* of the ndarray, and the validation is slightly
 * different
 */
static asdf_value_err_t asdf_ndarray_strides_parse(
    asdf_sequence_t *value, uint32_t ndim, int64_t **out) {
    asdf_value_err_t err = ASDF_VALUE_OK;
    int64_t *strides = NULL;

    int nstrides = asdf_sequence_size(value);

    if (nstrides < 0 || (uint32_t)nstrides != ndim) {
        warn_invalid_strides(&value->value);
        goto failure;
    }

    strides = malloc(ndim * sizeof(int64_t));

    if (!strides) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    asdf_sequence_iter_t *stride_iter = asdf_sequence_iter_init(value);
    size_t dim = 0;
    while (asdf_sequence_iter_next(&stride_iter)) {
        if (ASDF_VALUE_OK != asdf_value_as_int64(stride_iter->value, &strides[dim])) {
            warn_invalid_strides(&value->value);
            asdf_sequence_iter_destroy(stride_iter);
            goto failure;
        }

        if (0 == strides[dim]) {
            warn_invalid_strides(&value->value);
            asdf_sequence_iter_destroy(stride_iter);
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


static asdf_value_err_t asdf_ndarray_parse_block_data(
    asdf_mapping_t *ndarray_map, asdf_ndarray_t *ndarray) {
    asdf_value_err_t err = ASDF_VALUE_OK;
    asdf_sequence_t *shape_seq = NULL;
    asdf_datatype_shape_t shape = {0};
    asdf_byteorder_t byteorder = ASDF_BYTEORDER_LITTLE;
    asdf_sequence_t *strides_seq = NULL;
    int64_t *strides = NULL;
    uint64_t offset = 0;

    /* Parse shape */
    err = asdf_get_required_property(
        ndarray_map, "shape", ASDF_VALUE_SEQUENCE, NULL, (void *)&shape_seq);

    if (ASDF_IS_ERR(err))
        goto cleanup;

    err = asdf_datatype_shape_parse(shape_seq, &shape);

    if (ASDF_IS_ERR(err))
        goto cleanup;

    ndarray->ndim = shape.ndim;
    ndarray->shape = shape.shape;

    /* Parse byteorder */
    err = asdf_datatype_byteorder_parse(ndarray_map, "byteorder", &byteorder);

    if (ASDF_IS_ERR(err))
        goto cleanup;

    ndarray->byteorder = byteorder;

    /* Parse offset */
    err = asdf_get_optional_property(ndarray_map, "offset", ASDF_VALUE_UINT64, NULL, &offset);

    if (!ASDF_IS_OPTIONAL_OK(err))
        goto cleanup;

    ndarray->offset = offset;

    err = asdf_get_optional_property(
        ndarray_map, "strides", ASDF_VALUE_SEQUENCE, NULL, (void *)&strides_seq);

    if (ASDF_IS_OK(err))
        err = asdf_ndarray_strides_parse(strides_seq, shape.ndim, &strides);

    if (!ASDF_IS_OPTIONAL_OK(err))
        goto cleanup;

    ndarray->strides = strides;
    err = ASDF_VALUE_OK;
cleanup:
    asdf_sequence_destroy(shape_seq);
    asdf_sequence_destroy(strides_seq);
    return err;
}


static inline asdf_byteorder_t asdf_host_byteorder() {
    uint16_t one = 1;
    return (*(uint8_t *)&one) == 1 ? ASDF_BYTEORDER_LITTLE : ASDF_BYTEORDER_BIG;
}


/** Type inference for implicit inline ndarrays */

typedef struct {
    bool has_string;
    bool has_float;
    /** any negative integer seen */
    bool has_signed;
    /** any non-negative integer seen */
    bool has_uint;
    int64_t int_min;
    uint64_t uint_max;
    size_t max_string_len;
} ndarray_type_inf_t;


static void update_type_inference(asdf_value_t *elem, ndarray_type_inf_t *inf) {
    asdf_value_type_t vtype = asdf_value_get_type(elem);

    switch (vtype) {
    case ASDF_VALUE_STRING: {
        inf->has_string = true;
        const char *str = NULL;
        size_t len = 0;
        if (ASDF_VALUE_OK == asdf_value_as_string(elem, &str, &len)) {
            if (len > inf->max_string_len)
                inf->max_string_len = len;
        }
        break;
    }
    case ASDF_VALUE_FLOAT:
    case ASDF_VALUE_DOUBLE:
        inf->has_float = true;
        break;
    case ASDF_VALUE_INT8:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT64: {
        int64_t ival = 0;
        if (ASDF_VALUE_OK == asdf_value_as_int64(elem, &ival)) {
            inf->has_signed = true;
            if (ival < inf->int_min)
                inf->int_min = ival;
            /* Track the magnitude for range selection */
            if (ival >= 0 && (uint64_t)ival > inf->uint_max)
                inf->uint_max = (uint64_t)ival;
        }
        break;
    }
    case ASDF_VALUE_UINT8:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT64: {
        uint64_t uval = 0;
        if (ASDF_VALUE_OK == asdf_value_as_uint64(elem, &uval)) {
            inf->has_uint = true;
            if (uval > inf->uint_max)
                inf->uint_max = uval;
        }
        break;
    }
    default:
        /* BOOL, NULL, etc. -- no update needed for range tracking */
        break;
    }
}


static asdf_scalar_datatype_t infer_scalar_datatype(const ndarray_type_inf_t *inf) {
    if (inf->has_string)
        /* UCS4 strings not yet supported for inline data */
        return ASDF_DATATYPE_UNKNOWN;

    if (inf->has_float)
        return ASDF_DATATYPE_FLOAT64;

    if (inf->has_signed || inf->has_uint) {
        if (inf->has_signed) {
            /* Need a signed integer type */
            if (inf->int_min >= INT8_MIN && inf->uint_max <= (uint64_t)INT8_MAX)
                return ASDF_DATATYPE_INT8;
            if (inf->int_min >= INT16_MIN && inf->uint_max <= (uint64_t)INT16_MAX)
                return ASDF_DATATYPE_INT16;
            if (inf->int_min >= INT32_MIN && inf->uint_max <= (uint64_t)INT32_MAX)
                return ASDF_DATATYPE_INT32;
            return ASDF_DATATYPE_INT64;
        }
        /* All non-negative: use unsigned type */
        if (inf->uint_max <= UINT8_MAX)
            return ASDF_DATATYPE_UINT8;
        if (inf->uint_max <= UINT16_MAX)
            return ASDF_DATATYPE_UINT16;
        if (inf->uint_max <= UINT32_MAX)
            return ASDF_DATATYPE_UINT32;
        return ASDF_DATATYPE_UINT64;
    }

    return ASDF_DATATYPE_BOOL8;
}


/**
 * Recursively walk ``seq`` at the given ``depth``, recording shape and
 * accumulating type inference state.
 *
 * ``*shape`` is grown via realloc as new dimensions are discovered.
 * ``*ndim`` is the number of dimensions discovered so far.
 */
static asdf_value_err_t infer_inline_array_datatype(
    asdf_sequence_t *seq,
    uint32_t depth,
    uint64_t **shape,
    uint32_t *ndim,
    ndarray_type_inf_t *inf,
    asdf_file_t *file) {
    asdf_value_err_t err = ASDF_VALUE_OK;

    int size = asdf_sequence_size(seq);

    if (UNLIKELY(size < 0))
        return ASDF_VALUE_ERR_PARSE_FAILURE;

    if (size == 0)
        return ASDF_VALUE_OK;

    if (depth >= *ndim) {
        /* First time we visit this depth: record shape */
        uint64_t *new_shape = realloc(*shape, (depth + 1) * sizeof(uint64_t));
        if (UNLIKELY(!new_shape))
            return ASDF_VALUE_ERR_OOM;
        *shape = new_shape;
        (*shape)[depth] = (uint64_t)size;
        *ndim = depth + 1;
    } else {
        /* Validate that this row has the same length as the first row at
         * this depth (no jagged arrays) */
        if ((uint64_t)size != (*shape)[depth]) {
            ASDF_LOG(file, ASDF_LOG_ERROR, "inline ndarray has a jagged shape at depth %u", depth);
            return ASDF_VALUE_ERR_PARSE_FAILURE;
        }
    }

    /* Peek at the first element to decide if this level holds sequences or
     * scalars; all elements must be of the same kind */
    asdf_value_t *first = asdf_sequence_get(seq, 0);
    if (!first)
        return ASDF_VALUE_ERR_PARSE_FAILURE;

    bool first_is_seq = (asdf_value_get_type(first) == ASDF_VALUE_SEQUENCE);
    asdf_value_destroy(first);

    asdf_sequence_iter_t *iter = asdf_sequence_iter_init(seq);
    while (asdf_sequence_iter_next(&iter)) {
        asdf_value_t *elem = iter->value;
        bool elem_is_seq = (asdf_value_get_type(elem) == ASDF_VALUE_SEQUENCE);

        if (elem_is_seq != first_is_seq) {
            ASDF_LOG(
                file,
                ASDF_LOG_ERROR,
                "inline ndarray has mixed sequence/scalar elements at depth %u",
                depth);
            asdf_sequence_iter_destroy(iter);
            return ASDF_VALUE_ERR_PARSE_FAILURE;
        }

        if (elem_is_seq) {
            err = infer_inline_array_datatype(
                (asdf_sequence_t *)elem, depth + 1, shape, ndim, inf, file);
            if (ASDF_IS_ERR(err)) {
                asdf_sequence_iter_destroy(iter);
                return err;
            }
        } else {
            update_type_inference(elem, inf);
        }
    }

    return ASDF_VALUE_OK;
}


/** Scalar conversion: YAML value -> native C array element */
static asdf_value_err_t convert_scalar_to_native(
    asdf_value_t *elem, asdf_scalar_datatype_t dtype, void *dst, asdf_file_t *file) {
    asdf_value_err_t err = ASDF_VALUE_OK;

    switch (dtype) {
    case ASDF_DATATYPE_BOOL8:
        err = asdf_value_as_bool(elem, (bool *)dst);
        break;
    case ASDF_DATATYPE_INT8:
        err = asdf_value_as_int8(elem, (int8_t *)dst);
        break;
    case ASDF_DATATYPE_INT16:
        err = asdf_value_as_int16(elem, (int16_t *)dst);
        break;
    case ASDF_DATATYPE_INT32:
        err = asdf_value_as_int32(elem, (int32_t *)dst);
        break;
    case ASDF_DATATYPE_INT64:
        err = asdf_value_as_int64(elem, (int64_t *)dst);
        break;
    case ASDF_DATATYPE_UINT8:
        err = asdf_value_as_uint8(elem, (uint8_t *)dst);
        break;
    case ASDF_DATATYPE_UINT16:
        err = asdf_value_as_uint16(elem, (uint16_t *)dst);
        break;
    case ASDF_DATATYPE_UINT32:
        err = asdf_value_as_uint32(elem, (uint32_t *)dst);
        break;
    case ASDF_DATATYPE_UINT64:
        err = asdf_value_as_uint64(elem, (uint64_t *)dst);
        break;
    case ASDF_DATATYPE_FLOAT32:
    case ASDF_DATATYPE_FLOAT64: {
        /* Integers in YAML should be coercible to float targets */
        double dval = 0.0;
        err = asdf_value_as_double(elem, &dval);
        if (err == ASDF_VALUE_ERR_TYPE_MISMATCH) {
            int64_t ival = 0;
            err = asdf_value_as_int64(elem, &ival);
            if (ASDF_VALUE_OK == err) {
                dval = (double)ival;
            } else {
                uint64_t uval = 0;
                err = asdf_value_as_uint64(elem, &uval);
                if (ASDF_VALUE_OK == err)
                    dval = (double)uval;
            }
        }
        if (ASDF_VALUE_OK == err) {
            if (dtype == ASDF_DATATYPE_FLOAT32)
                *(float *)dst = (float)dval;
            else
                *(double *)dst = dval;
        }
        break;
    }
    default:
        ASDF_LOG(
            file,
            ASDF_LOG_ERROR,
            "inline ndarray: unsupported target datatype for scalar conversion");
        return ASDF_VALUE_ERR_PARSE_FAILURE;
    }

    if (ASDF_IS_ERR(err)) {
        ASDF_LOG(
            file,
            ASDF_LOG_ERROR,
            "inline ndarray: failed to convert scalar element to target datatype");
    }
    return err;
}


/**
 * Recursively walk ``seq`` at ``depth``, writing converted scalars into
 * ``buf`` and advancing ``*offset`` (in elements, not bytes).
 */
static asdf_value_err_t parse_inline_array_level(
    asdf_sequence_t *seq,
    const asdf_ndarray_t *ndarray,
    uint8_t *buf,
    size_t *offset,
    uint32_t depth) {
    asdf_value_err_t err = ASDF_VALUE_OK;
    asdf_scalar_datatype_t dtype = ndarray->datatype.type;
    size_t elem_size = asdf_scalar_datatype_size(dtype);
    asdf_file_t *file = ndarray->internal->file;

    asdf_sequence_iter_t *iter = asdf_sequence_iter_init(seq);
    while (asdf_sequence_iter_next(&iter)) {
        asdf_value_t *elem = iter->value;
        if (depth == ndarray->ndim - 1) {
            /* Leaf level: convert scalar to C value */
            err = convert_scalar_to_native(elem, dtype, buf + ((*offset) * elem_size), file);
            if (ASDF_IS_ERR(err)) {
                asdf_sequence_iter_destroy(iter);
                return err;
            }
            (*offset)++;
        } else {
            /* Non-leaf: recurse into sub-sequence */
            err = parse_inline_array_level(
                (asdf_sequence_t *)elem, ndarray, buf, offset, depth + 1);
            if (ASDF_IS_ERR(err)) {
                asdf_sequence_iter_destroy(iter);
                return err;
            }
        }
    }

    return ASDF_VALUE_OK;
}


/**
 * Parse ``ndarray->internal->inline_data`` into a flat C array buffer ``buf``.
 *
 * The caller is responsible for allocating ``buf`` with the correct size
 * (asdf_ndarray_nbytes(ndarray) bytes).
 */
static asdf_value_err_t asdf_ndarray_parse_inline_data(asdf_ndarray_t *ndarray, uint8_t *buf) {
    size_t offset = 0;
    return parse_inline_array_level(ndarray->internal->inline_data, ndarray, buf, &offset, 0);
}


/**
 * Helper routine for handling deserialization of ndarrays with inline data
 *
 * The inline data is not fully parsed at this stage--that occurs lazily when
 * the data is first accessed.  At this stage we store a clone of the inline
 * data sequence and perform basic validations (shape, datatype).
 *
 * ``ndarray_map`` may be NULL in case of a plain inline array, or it
 * may be a mapping containing information such as the explicit datatype (
 * most other ndarray properties are ignored for inline data)
 */
static asdf_value_err_t asdf_ndarray_deserialize_inline(
    asdf_value_t *value,
    asdf_mapping_t *ndarray_map,
    asdf_sequence_t *ndarray_seq,
    asdf_ndarray_t **out) {
    assert(ndarray_seq && out);

    asdf_value_err_t err = ASDF_VALUE_OK;
    asdf_ndarray_t *ndarray = NULL;
    asdf_ndarray_internal_t *internal = NULL;
    asdf_value_t *datatype_val = NULL;
    asdf_sequence_t *shape_seq = NULL;
    asdf_datatype_shape_t shape_hint = {0};
    bool has_shape_hint = false;
    bool has_explicit_datatype = false;
    uint64_t *shape = NULL;
    uint32_t ndim = 0;
    ndarray_type_inf_t inf = {.int_min = INT64_MAX};

    ndarray = calloc(1, sizeof(asdf_ndarray_t));
    if (UNLIKELY(!ndarray)) {
        err = ASDF_VALUE_ERR_OOM;
        goto cleanup;
    }

    internal = asdf_ndarray_internal(ndarray, true);
    if (UNLIKELY(!internal)) {
        err = ASDF_VALUE_ERR_OOM;
        goto cleanup;
    }

    internal->file = value->file;

    if (ndarray_map) {
        /* Optional explicit datatype */
        err = asdf_get_optional_property(
            ndarray_map, "datatype", ASDF_VALUE_UNKNOWN, NULL, (void *)&datatype_val);

        if (!ASDF_IS_OPTIONAL_OK(err))
            goto cleanup;

        if (ASDF_VALUE_OK == err) {
            err = asdf_datatype_parse(datatype_val, ASDF_BYTEORDER_DEFAULT, &ndarray->datatype);
            asdf_value_destroy(datatype_val);
            datatype_val = NULL;
            if (ASDF_IS_ERR(err))
                goto cleanup;
            has_explicit_datatype = true;
        }

        /* Optional shape property (for validation only) */
        err = asdf_get_optional_property(
            ndarray_map, "shape", ASDF_VALUE_SEQUENCE, NULL, (void *)&shape_seq);

        if (!ASDF_IS_OPTIONAL_OK(err))
            goto cleanup;

        if (ASDF_VALUE_OK == err) {
            err = asdf_datatype_shape_parse(shape_seq, &shape_hint);
            asdf_sequence_destroy(shape_seq);
            shape_seq = NULL;
            if (ASDF_IS_ERR(err))
                goto cleanup;
            has_shape_hint = true;
        }
        err = ASDF_VALUE_OK;
    }

    /* Infer shape (and type if implicit) by walking the YAML sequence */
    err = infer_inline_array_datatype(ndarray_seq, 0, &shape, &ndim, &inf, value->file);
    if (ASDF_IS_ERR(err))
        goto cleanup;

    /* Validate against the documented shape if provided */
    if (has_shape_hint && shape) {
        bool shape_ok = (ndim == shape_hint.ndim);
        for (uint32_t idx = 0; shape_ok && idx < ndim; idx++)
            shape_ok = (shape[idx] == shape_hint.shape[idx]);

        if (!shape_ok)
            ASDF_LOG(
                value->file,
                ASDF_LOG_WARN,
                "inline ndarray shape does not match the documented shape property");
    }

    ndarray->ndim = ndim;
    ndarray->shape = shape;
    shape = NULL; /* ndarray now owns shape */

    if (!has_explicit_datatype) {
        asdf_scalar_datatype_t inferred = infer_scalar_datatype(&inf);
        if (inferred == ASDF_DATATYPE_UNKNOWN) {
            ASDF_LOG(
                value->file,
                ASDF_LOG_ERROR,
                "inline ndarray: could not infer a supported datatype from the array contents");
            err = ASDF_VALUE_ERR_PARSE_FAILURE;
            goto cleanup;
        }
        ndarray->datatype.type = inferred;
        ndarray->datatype.size = asdf_scalar_datatype_size(inferred);
    }

    /* Inline data is in native byte order once parsed into C types */
    ndarray->byteorder = asdf_host_byteorder();

    /* Clone and stash the YAML sequence for lazy conversion in data_raw */
    internal->inline_data = (asdf_sequence_t *)asdf_value_clone(&ndarray_seq->value);

    if (UNLIKELY(!internal->inline_data)) {
        err = ASDF_VALUE_ERR_OOM;
        goto cleanup;
    }

    /* Set the write_inline flag implicitly so the same ndarray is
     * re-serialized inline unless otherwise specified later */
    ndarray->internal->write_inline = true;

    *out = ndarray;
    err = ASDF_VALUE_OK;

cleanup:
    asdf_value_destroy(datatype_val);
    asdf_sequence_destroy(shape_seq);
    free(shape_hint.shape);
    if (ASDF_IS_ERR(err)) {
        if (ndarray) {
            free(ndarray->shape);
            asdf_datatype_clean(&ndarray->datatype);
        }
        free(internal);
        free(ndarray);
    }
    free(shape);
    return err;
}


static asdf_value_err_t asdf_ndarray_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    uint64_t source = 0;
    asdf_value_t *datatype = NULL;
    asdf_mapping_t *ndarray_map = NULL;
    asdf_sequence_t *ndarray_seq = NULL;
    asdf_ndarray_t *ndarray = NULL;
    asdf_ndarray_internal_t *internal = NULL;

    err = asdf_value_as_mapping(value, &ndarray_map);

    /* Not a mapping? Check if it's an implicit inline array */
    if (ASDF_VALUE_ERR_TYPE_MISMATCH == err) {
        err = asdf_value_as_sequence(value, &ndarray_seq);

        if (ASDF_IS_OK(err)) // Quick return path for implicit inline
            return asdf_ndarray_deserialize_inline(
                value, NULL, ndarray_seq, (asdf_ndarray_t **)out);
    }

    if (ASDF_IS_ERR(err))
        goto cleanup;

    /* currently only integer sources are allowed */
    err = asdf_get_optional_property(ndarray_map, "source", ASDF_VALUE_UINT64, NULL, &source);

    if (err == ASDF_VALUE_ERR_TYPE_MISMATCH) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(value);
        ASDF_LOG(
            value->file,
            ASDF_LOG_WARN,
            "currently only internal binary block sources are supported; ndarray at "
            "%s has an unsupported source and will not be read",
            path);
#endif
        goto cleanup;
    } else if (err == ASDF_VALUE_ERR_NOT_FOUND) {
        err = asdf_get_optional_property(
            ndarray_map, "data", ASDF_VALUE_SEQUENCE, NULL, (void *)&ndarray_seq);

        if (err == ASDF_VALUE_ERR_NOT_FOUND) {
#ifdef ASDF_LOG_ENABLED
            const char *path = asdf_value_path(value);
            ASDF_LOG(
                value->file,
                ASDF_LOG_ERROR,
                "invalid ndarray at %s: either a source or a data property is required",
                path);
#endif
            err = ASDF_VALUE_ERR_PARSE_FAILURE;
            goto cleanup;
        } else if (err == ASDF_VALUE_OK) {
            err = asdf_ndarray_deserialize_inline(
                value, ndarray_map, ndarray_seq, (asdf_ndarray_t **)out);
            asdf_sequence_destroy(ndarray_seq);
            return err;
        } else {
            goto cleanup;
        }
    }

    if (ASDF_IS_ERR(err))
        goto cleanup;

    ndarray = calloc(1, sizeof(asdf_ndarray_t));

    if (UNLIKELY(!ndarray)) {
        err = ASDF_VALUE_ERR_OOM;
        goto cleanup;
    }

    internal = asdf_ndarray_internal(ndarray, true);

    if (UNLIKELY(!internal)) {
        err = ASDF_VALUE_ERR_OOM;
        goto cleanup;
    }

    err = asdf_ndarray_parse_block_data(ndarray_map, ndarray);

    if (ASDF_IS_ERR(err))
        goto cleanup;

    /* Parse datatype */
    err = asdf_get_required_property(
        ndarray_map, "datatype", ASDF_VALUE_UNKNOWN, NULL, (void *)&datatype);

    if (!ASDF_IS_OK(err))
        goto cleanup;

    err = asdf_datatype_parse(datatype, ndarray->byteorder, &ndarray->datatype);

    if (ASDF_IS_ERR(err))
        goto cleanup;

    ndarray->source = source;
    internal->file = value->file;
    *out = ndarray;
    err = ASDF_VALUE_OK;
cleanup:
    asdf_value_destroy(datatype);

    if (ASDF_IS_ERR(err)) {
        if (LIKELY(ndarray)) {
            free(ndarray->strides);
            free(ndarray->shape);
        }
        free(internal);
        free(ndarray);
    }

    return err;
}


static void asdf_ndarray_dealloc(void *value) {
    if (!value)
        return;

    asdf_ndarray_t *ndarray = value;

    if (ndarray->internal) {
        asdf_block_close(ndarray->internal->block);
        asdf_sequence_destroy(ndarray->internal->inline_data);
        if (ndarray->internal->data_is_inline)
            free(ndarray->internal->data);
    }

    free(ndarray->internal);
    free(ndarray->shape);
    free(ndarray->strides);
    asdf_datatype_clean(&ndarray->datatype);
    ZERO_MEMORY(ndarray, sizeof(asdf_ndarray_t));
    free(ndarray);
}


bool asdf_ndarray_inline(asdf_ndarray_t *ndarray) {
    if (UNLIKELY(!ndarray))
        return false;

    asdf_ndarray_internal_t *internal = asdf_ndarray_internal(ndarray, false);
    return internal && internal->write_inline;
}


void asdf_ndarray_inline_set(asdf_ndarray_t *ndarray, bool is_inline) {
    if (UNLIKELY(!ndarray))
        return;

    asdf_ndarray_internal_t *internal = asdf_ndarray_internal(ndarray, is_inline);

    if (internal)
        internal->write_inline = is_inline;
}


/**
 * Recursively build a nested YAML flow sequence from ndarray data.
 *
 * At the leaf dimension (depth == ndim-1) the appropriate
 * ``asdf_sequence_of_*`` bulk constructor is called.  At all other depths
 * an outer sequence is built by appending the inner sequences returned by
 * recursive calls.  Every sequence is styled as flow (compact inline YAML).
 *
 * .. todo::
 *
 *   Currently only supports basic numeric scalar types.  Should be extended
 *   to support record types and other compound datatypes (e.g. complex) but
 *   these are not yet fully supported by the library in general.
 */
static asdf_sequence_t *asdf_ndarray_serialize_seq_level(
    asdf_file_t *file,
    const asdf_ndarray_t *ndarray,
    const void *data,
    uint32_t depth,
    size_t *elem_offset) {

    uint32_t ndim = ndarray->ndim;
    uint64_t dim_size = ndim ? ndarray->shape[depth] : 0;
    asdf_scalar_datatype_t dtype = ndarray->datatype.type;
    asdf_sequence_t *seq = NULL;

    if (ndim && (depth == ndim - 1)) {
        /* Leaf level: create sequence from the slice of the flat data buffer */
        const void *start = (const uint8_t *)data + (*elem_offset * ndarray->datatype.size);

        switch (dtype) {
        case ASDF_DATATYPE_BOOL8:
            seq = asdf_sequence_of_bool(file, (const bool *)start, (int)dim_size);
            break;
        case ASDF_DATATYPE_INT8:
            seq = asdf_sequence_of_int8(file, (const int8_t *)start, (int)dim_size);
            break;
        case ASDF_DATATYPE_INT16:
            seq = asdf_sequence_of_int16(file, (const int16_t *)start, (int)dim_size);
            break;
        case ASDF_DATATYPE_INT32:
            seq = asdf_sequence_of_int32(file, (const int32_t *)start, (int)dim_size);
            break;
        case ASDF_DATATYPE_INT64:
            seq = asdf_sequence_of_int64(file, (const int64_t *)start, (int)dim_size);
            break;
        case ASDF_DATATYPE_UINT8:
            seq = asdf_sequence_of_uint8(file, (const uint8_t *)start, (int)dim_size);
            break;
        case ASDF_DATATYPE_UINT16:
            seq = asdf_sequence_of_uint16(file, (const uint16_t *)start, (int)dim_size);
            break;
        case ASDF_DATATYPE_UINT32:
            seq = asdf_sequence_of_uint32(file, (const uint32_t *)start, (int)dim_size);
            break;
        case ASDF_DATATYPE_UINT64:
            seq = asdf_sequence_of_uint64(file, (const uint64_t *)start, (int)dim_size);
            break;
        case ASDF_DATATYPE_FLOAT32:
            seq = asdf_sequence_of_float(file, (const float *)start, (int)dim_size);
            break;
        case ASDF_DATATYPE_FLOAT64:
            seq = asdf_sequence_of_double(file, (const double *)start, (int)dim_size);
            break;
        default:
            ASDF_LOG(file, ASDF_LOG_ERROR, "unsupported datatype for inline ndarray serialization");
            return NULL;
        }

        if (seq)
            *elem_offset += dim_size;
    } else {
        /* Non-leaf: build outer sequence by appending inner sequences;
         * in the corner-case of ndim == 0 this will still create an empty
         * sequence */
        seq = asdf_sequence_create(file);

        if (!seq)
            return NULL;

        for (uint64_t idx = 0; idx < dim_size; idx++) {
            asdf_sequence_t *inner = asdf_ndarray_serialize_seq_level(
                file, ndarray, data, depth + 1, elem_offset);

            if (!inner) {
                asdf_sequence_destroy(seq);
                return NULL;
            }

            asdf_value_err_t err = asdf_sequence_append_sequence(seq, inner);

            if (ASDF_IS_ERR(err)) {
                asdf_sequence_destroy(inner);
                asdf_sequence_destroy(seq);
                return NULL;
            }
        }
    }

    if (seq)
        asdf_sequence_set_style(seq, ASDF_YAML_NODE_STYLE_FLOW);

    return seq;
}


static asdf_sequence_t *asdf_ndarray_serialize_inline_data(
    asdf_file_t *file, const asdf_ndarray_t *ndarray) {
    size_t elem_offset = 0;

    return asdf_ndarray_serialize_seq_level(
        file, ndarray, ndarray->internal->data, 0, &elem_offset);
}


/**
 * Helpers to asdf_ndarray_serialize to set fields only relevant when using
 * binary block data
 */


static asdf_value_err_t asdf_ndarray_serialize_strides(
    asdf_file_t *file, const asdf_ndarray_t *ndarray, asdf_mapping_t *ndarray_map) {
    assert(file);
    assert(ndarray);
    assert(ndarray_map);
    assert(ndarray->strides);
    asdf_value_err_t err = ASDF_VALUE_OK;
    asdf_sequence_t *strides_seq = NULL;

    bool trivial = true;

    for (uint32_t idx = 0; idx < ndarray->ndim; idx++) {
        if (ndarray->strides[idx] != 1) {
            trivial = false;
            break;
        }
    }

    if (UNLIKELY(!trivial)) {
        strides_seq = asdf_sequence_create(file);

        if (UNLIKELY(!strides_seq))
            return ASDF_VALUE_ERR_OOM;

        for (uint32_t idx = 0; idx < ndarray->ndim; idx++) {
            err = asdf_sequence_append_int64(strides_seq, ndarray->strides[idx]);

            if (ASDF_IS_ERR(err)) {
                asdf_sequence_destroy(strides_seq);
                return err;
            }
        }

        asdf_sequence_set_style(strides_seq, ASDF_YAML_NODE_STYLE_FLOW);
        err = asdf_mapping_set_sequence(ndarray_map, "strides", strides_seq);

        if (ASDF_IS_ERR(err))
            asdf_sequence_destroy(strides_seq);
    }

    return err;
}


static asdf_value_err_t asdf_ndarray_serialize_block_data(
    asdf_file_t *file, const asdf_ndarray_t *ndarray, asdf_mapping_t *ndarray_map) {
    assert(file);
    assert(ndarray);
    assert(ndarray_map);
    asdf_value_err_t err = ASDF_VALUE_OK;
    asdf_sequence_t *shape_seq = asdf_sequence_create(file);

    if (!shape_seq)
        return ASDF_VALUE_ERR_OOM;

    for (uint32_t idx = 0; idx < ndarray->ndim; idx++) {
        err = asdf_sequence_append_uint64(shape_seq, ndarray->shape[idx]);

        if (ASDF_IS_ERR(err)) {
            asdf_sequence_destroy(shape_seq);
            return err;
        }
    }

    asdf_sequence_set_style(shape_seq, ASDF_YAML_NODE_STYLE_FLOW);
    err = asdf_mapping_set_sequence(ndarray_map, "shape", shape_seq);

    if (ASDF_IS_ERR(err)) {
        asdf_sequence_destroy(shape_seq);
        return err;
    }

    // Byteorder is required so always render "little" if not otherwise
    // specified
    if (ndarray->byteorder == ASDF_BYTEORDER_DEFAULT)
        ASDF_LOG(
            file, ASDF_LOG_DEBUG, "byteorder not specified on ndarray; defaulting to 'little'");

    const char *byteorder = asdf_byteorder_to_string(
        ndarray->byteorder == ASDF_BYTEORDER_DEFAULT ? ASDF_BYTEORDER_LITTLE : ndarray->byteorder);

    if (UNLIKELY(!byteorder))
        return ASDF_VALUE_ERR_EMIT_FAILURE;

    err = asdf_mapping_set_string0(ndarray_map, "byteorder", byteorder);

    if (ASDF_IS_ERR(err))
        return err;

    if (ndarray->offset > 0) {
        err = asdf_mapping_set_uint64(ndarray_map, "offset", ndarray->offset);

        if (ASDF_IS_ERR(err))
            return err;
    }

    // Only writes strides if set to a non-trivial value
    if (ndarray->strides) {
        err = asdf_ndarray_serialize_strides(file, ndarray, ndarray_map);
    }

    return err;
}


/** Helper to `asdf_ndarray_serialize` for handling inline data */
static asdf_value_err_t asdf_ndarray_serialize_inline(
    asdf_file_t *file, const asdf_ndarray_t *ndarray, asdf_mapping_t *ndarray_map) {

    assert(file);
    assert(ndarray);
    assert(ndarray_map);

    asdf_sequence_t *inline_data = NULL;
    asdf_sequence_t *shape_seq = NULL;
    asdf_value_err_t err = ASDF_VALUE_OK;

    if (ndarray->internal->write_compression) {
        ASDF_LOG(
            file,
            ASDF_LOG_WARN,
            "ndarray has compression set but is marked for inline writing; "
            "compression will be ignored");
    }

    size_t thresh = file->config ? file->config->emitter.inline_ndarray_warning_thresh : 0;

    if (thresh > 0 && asdf_ndarray_size(ndarray) > thresh) {
        ASDF_LOG(
            file,
            ASDF_LOG_WARN,
            "inline ndarray has %llu elements, exceeding the threshold of %zu; "
            "consider using binary block storage instead",
            (unsigned long long)asdf_ndarray_size(ndarray),
            thresh);
    }

    inline_data = asdf_ndarray_serialize_inline_data(file, ndarray);

    if (!inline_data) {
        err = ASDF_VALUE_ERR_EMIT_FAILURE;
        goto cleanup;
    }

    err = asdf_mapping_set_sequence(ndarray_map, "data", inline_data);

    if (ASDF_IS_ERR(err))
        goto cleanup;

    /* Special case -- specify the shape as empty if an empty ndarray
     * was serialized */
    if (UNLIKELY(ndarray->ndim == 0)) {
        shape_seq = asdf_sequence_create(file);

        if (UNLIKELY(!shape_seq)) {
            ASDF_ERROR_OOM(file);
            err = ASDF_VALUE_ERR_OOM;
            goto cleanup;
        }

        err = asdf_mapping_set_sequence(ndarray_map, "shape", shape_seq);
    }
cleanup:
    if (err != ASDF_VALUE_OK) {
        asdf_sequence_destroy(inline_data);
        asdf_sequence_destroy(shape_seq);
    }
    return err;
}


/** Helper to `asdf_ndarray_serialize` for handling binary block data */
static asdf_value_err_t asdf_ndarray_serialize_block(
    asdf_file_t *file, const asdf_ndarray_t *ndarray, asdf_mapping_t *ndarray_map) {

    assert(file);
    assert(ndarray);
    assert(ndarray_map);

    asdf_value_err_t err = ASDF_VALUE_OK;
    uint64_t nbytes = asdf_ndarray_nbytes(ndarray);
    ssize_t block_idx = asdf_block_append(file, ndarray->internal->data, nbytes);

    if (block_idx < 0) {
        ASDF_ERROR_OOM(file);
        err = ASDF_VALUE_ERR_OOM;
        goto cleanup;
    }

    if (ndarray->internal && ndarray->internal->write_compression) {
        asdf_block_info_t *info = asdf_block_info_vec_at_mut(&file->blocks, (isize)block_idx);
        const char *compression = ndarray->internal->write_compression;

        if (asdf_block_info_compression_set(file, info, compression) != 0) {
            err = ASDF_VALUE_ERR_EMIT_FAILURE;
            goto cleanup;
        }
    }

    err = asdf_mapping_set_int64(ndarray_map, "source", (int64_t)block_idx);
cleanup:
    return err;
}


static asdf_value_t *asdf_ndarray_serialize(
    asdf_file_t *file,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    const void *obj,
    UNUSED(const void *userdata)) {

    if (UNLIKELY(!file || !obj))
        return NULL;

    const asdf_ndarray_t *ndarray = obj;
    asdf_value_t *value = NULL;
    asdf_value_t *datatype_val = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_EMIT_FAILURE;
    const asdf_extension_t *datatype_ext = NULL;
    bool is_inline = false;
    asdf_sequence_t *inline_data = NULL;
    asdf_mapping_t *ndarray_map = asdf_mapping_create(file);

    if (UNLIKELY(!ndarray_map)) {
        err = ASDF_VALUE_ERR_OOM;
        goto cleanup;
    }

    bool has_data = ndarray->internal && ndarray->internal->data;
    bool write_inline_flag = ndarray->internal && ndarray->internal->write_inline;

    if (!has_data) {
        ASDF_LOG(
            file,
            ASDF_LOG_WARN,
            "no data was assigned to the ndarray; it will still be written but with an "
            "empty inline data array");

        is_inline = true;
        inline_data = asdf_sequence_create(file);

        if (!inline_data) {
            err = ASDF_VALUE_ERR_OOM;
            goto cleanup;
        }

        err = asdf_mapping_set_sequence(ndarray_map, "data", inline_data);

        if (ASDF_IS_ERR(err)) {
            asdf_sequence_destroy(inline_data);
            goto cleanup;
        }
    } else if (write_inline_flag) {
        err = asdf_ndarray_serialize_inline(file, ndarray, ndarray_map);
        if (err != ASDF_VALUE_OK)
            goto cleanup;

        is_inline = true;
    } else {
        err = asdf_ndarray_serialize_block(file, ndarray, ndarray_map);
    }

    if (ASDF_IS_ERR(err))
        goto cleanup;

    datatype_ext = asdf_extension_get(file, ASDF_CORE_DATATYPE_TAG);

    if (UNLIKELY(!datatype_ext)) {
        ASDF_LOG(
            file,
            ASDF_LOG_ERROR,
            "no extension registered for the datatype tag: %s; the ndarray cannot be "
            "written",
            ASDF_CORE_DATATYPE_TAG);
        goto cleanup;
    }

    datatype_val = asdf_value_of_extension_type(file, &ndarray->datatype, datatype_ext);

    if (UNLIKELY(!datatype_val))
        goto cleanup;

    // Hack to remove the tag from the output; should have a utility in the
    // public API for setting a value's tag as implict, but for now only used
    // here
    fy_node_remove_tag(datatype_val->node);

    err = asdf_mapping_set(ndarray_map, "datatype", datatype_val);

    if (ASDF_IS_ERR(err)) {
        asdf_value_destroy(datatype_val);
        goto cleanup;
    }

    if (!is_inline) {
        err = asdf_ndarray_serialize_block_data(file, ndarray, ndarray_map);
    }
cleanup:
    if (ASDF_IS_ERR(err))
        asdf_mapping_destroy(ndarray_map);
    else
        value = asdf_value_of_mapping(ndarray_map);

    return value;
}


/*
 * Define the extension for the core/ndarray-1.1.0 schema
 *
 * TODO: Also support ndarray-1.0.0
 */
ASDF_REGISTER_EXTENSION(
    ndarray,
    ASDF_CORE_NDARRAY_TAG,
    asdf_ndarray_t,
    &libasdf_software,
    asdf_ndarray_serialize,
    asdf_ndarray_deserialize,
    NULL, /* TODO: copy */
    asdf_ndarray_dealloc,
    NULL);


/* ndarray methods */
const void *asdf_ndarray_data_raw(asdf_ndarray_t *ndarray, size_t *size) {
    if (!ndarray || !ndarray->internal)
        return NULL;

    // Return the user-allocated data (or previously parsed inline data)
    if (ndarray->internal->data) {
        *size = asdf_ndarray_nbytes(ndarray);
        return ndarray->internal->data;
    }

    // Lazily parse inline YAML data into a C array on first access
    if (ndarray->internal->inline_data) {
        size_t nbytes = (size_t)asdf_ndarray_nbytes(ndarray);
        void *buf = malloc(nbytes);
        if (UNLIKELY(!buf))
            return NULL;

        if (ASDF_IS_ERR(asdf_ndarray_parse_inline_data(ndarray, buf))) {
            free(buf);
            return NULL;
        }

        ndarray->internal->data = buf;
        ndarray->internal->data_is_inline = true;
        *size = nbytes;
        return buf;
    }

    // Return block data read from the file
    if (!ndarray->internal->block) {
        asdf_block_t *block = asdf_block_open(ndarray->internal->file, ndarray->source);

        if (!block)
            return NULL;

        ndarray->internal->block = block;
    }

    return asdf_block_data(ndarray->internal->block, size);
}


uint64_t asdf_ndarray_size(const asdf_ndarray_t *ndarray) {
    if (UNLIKELY(!ndarray || ndarray->ndim == 0))
        return 0;

    uint64_t size = 1;

    for (uint32_t idx = 0; idx < ndarray->ndim; idx++)
        size *= ndarray->shape[idx];

    return size;
}


uint64_t asdf_ndarray_nbytes(const asdf_ndarray_t *ndarray) {
    uint64_t size = asdf_ndarray_size(ndarray);

    if (UNLIKELY(size == 0))
        return size;

    return size * asdf_datatype_size((asdf_datatype_t *)&ndarray->datatype);
}


void *asdf_ndarray_data_alloc(asdf_ndarray_t *ndarray) {
    if (UNLIKELY(!ndarray))
        return NULL;

    asdf_ndarray_internal_t *internal = asdf_ndarray_internal(ndarray, true);

    if (internal && internal->data)
        return internal->data;

    if (!internal) {
        ASDF_ERROR_OOM(NULL);
        return NULL;
    }

    uint64_t size = asdf_ndarray_nbytes(ndarray);

    if (UNLIKELY(size == 0)) {
        /* Special case for empty array; just return NULL, but don't free
         * internal structures */
        internal->data_is_empty = true;
        return NULL;
    }

    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (data == MAP_FAILED || !data) {
        free(internal);
        ndarray->internal = NULL;
        return NULL;
    }

    internal->data = data;
    return data;
}


static void ndarray_write_data_cleanup(void *userdata) {
    asdf_ndarray_internal_t *internal = userdata;
    free(internal->data);
    free(internal);
}


void *asdf_ndarray_data_alloc_temp(asdf_file_t *file, asdf_ndarray_t *ndarray) {
    if (UNLIKELY(!file || !ndarray))
        return NULL;

    asdf_ndarray_internal_t *internal = calloc(1, sizeof(asdf_ndarray_internal_t));

    if (UNLIKELY(!internal)) {
        ASDF_ERROR_OOM(file);
        return NULL;
    }

    uint64_t nbytes = asdf_ndarray_nbytes(ndarray);
    void *data = calloc(1, (size_t)nbytes);

    if (UNLIKELY(!data)) {
        free(internal);
        ASDF_ERROR_OOM(file);
        return NULL;
    }

    internal->data = data;
    ndarray->internal = internal;
    asdf_file_write_cleanup_add(file, ndarray_write_data_cleanup, internal);
    return data;
}


void asdf_ndarray_data_dealloc(asdf_ndarray_t *ndarray) {
    if (UNLIKELY(!ndarray))
        return;

    asdf_ndarray_internal_t *internal = asdf_ndarray_internal(ndarray, false);

    /* Inline data is managed by asdf_ndarray_destroy; this function only
     * handles mmap'd data allocated by asdf_ndarray_data_alloc */
    if (internal && internal->data_is_inline)
        return;

    if (UNLIKELY(!internal || (!internal->data && !internal->data_is_empty))) {
        asdf_context_t *ctx = NULL;
        if (ndarray->internal)
            ctx = asdf_get_context_helper(ndarray->internal->file);
        else
            ctx = asdf_get_context_helper(NULL);

        ASDF_LOG_CTX(
            ctx,
            ASDF_LOG_WARN,
            "asdf_ndarray_data_dealloc called without asdf_ndarray_data_alloc on "
            "asdf_ndarray_t at 0x%px",
            ndarray);
        return;
    }

    uint64_t size = asdf_ndarray_nbytes(ndarray);
    munmap(internal->data, size);
    asdf_block_close(internal->block);
    free(internal);
    ndarray->internal = NULL;
}


int asdf_ndarray_compression_set(asdf_ndarray_t *ndarray, const char *compression) {
    asdf_ndarray_internal_t *internal = asdf_ndarray_internal(ndarray, true);

    if (!internal) {
        ASDF_ERROR_OOM(NULL);
        return -1;
    }

    internal->write_compression = compression;
    return 0;
}


/** Helpers for asdf_ndarray_read_tile */
// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static inline bool should_byteswap(size_t elsize, asdf_byteorder_t byteorder) {
    if (elsize <= 1)
        return false;

    asdf_byteorder_t host_byteorder = asdf_host_byteorder();
    return host_byteorder != byteorder;
}


static asdf_ndarray_err_t asdf_ndarray_read_tile_init_strides(
    const uint64_t *shape, uint32_t ndim, int64_t **strides_out) {
    assert(shape);
    assert(strides_out);
    asdf_ndarray_err_t err = ASDF_NDARRAY_OK;
    uint32_t inner_dim = ndim - 1;
    int64_t *strides = malloc(sizeof(int64_t) * ndim);

    if (!strides) {
        err = ASDF_NDARRAY_ERR_OOM;
        goto cleanup;
    }

    strides[inner_dim] = 1;

    if (ndim > 1) {
        for (uint32_t dim = inner_dim; dim > 0; dim--) {
            uint64_t extent = shape[dim];
            int64_t stride = strides[dim];
            // Bounds checking
            if (extent > (uint64_t)INT64_MAX / llabs(stride)) {
                err = ASDF_NDARRAY_ERR_OUT_OF_BOUNDS;
                goto cleanup;
            }
            strides[dim - 1] = stride * (int64_t)extent;
        }
    }

    *strides_out = strides;
cleanup:
    if (err != ASDF_NDARRAY_OK)
        free(strides);

    return err;
}


static inline asdf_ndarray_err_t asdf_ndarray_read_tile_main_loop(
    void *dst,
    size_t dst_elsize,
    const void *src,
    size_t src_elsize,
    const uint64_t *shape,
    const int64_t *strides,
    const uint64_t *origin,
    uint64_t *odometer,
    uint32_t ndim,
    asdf_ndarray_convert_fn_t convert) {
    bool done = false;
    bool overflow = false;
    uint32_t inner_dim = ndim - 1;
    uint64_t inner_nelem = shape[inner_dim];
    size_t inner_size = inner_nelem * dst_elsize;
    void *dst_tmp = dst;

    while (!done) {
        overflow = convert(dst_tmp, src, inner_nelem, dst_elsize);
        dst_tmp += inner_size;

        uint32_t dim = inner_dim - 1;
        do {
            odometer[dim]++;
            src += strides[dim] * src_elsize;

            if (odometer[dim] < origin[dim] + shape[dim])
                break;

            if (dim == 0) {
                done = true;
                break;
            }

            odometer[dim] = origin[dim];
            // Back up
            src -= shape[dim] * strides[dim] * src_elsize;
        } while (dim-- > 0);
    }

    return overflow ? ASDF_NDARRAY_ERR_OVERFLOW : ASDF_NDARRAY_OK;
}


static inline bool check_bounds(
    const asdf_ndarray_t *ndarray, const uint64_t *origin, const uint64_t *shape) {
    // TODO: (Maybe? allow option for edge cases with fill values for out-of-bound pixels?
    uint64_t *array_shape = ndarray->shape;
    uint32_t ndim = ndarray->ndim;

    for (uint32_t idx = 0; idx < ndim; idx++) {
        if (origin[idx] + shape[idx] > array_shape[idx])
            return false;
    }

    return true;
}


asdf_ndarray_err_t asdf_ndarray_read_tile_ndim(
    asdf_ndarray_t *ndarray,
    const uint64_t *origin,
    const uint64_t *shape,
    asdf_scalar_datatype_t dst_t,
    void **dst) {

    if (UNLIKELY(!dst || !ndarray || !origin || !shape))
        // Invalid argument, must be non-NULL
        return ASDF_NDARRAY_ERR_INVAL;

    void *new_buf = NULL;
    int64_t *strides = NULL;
    uint64_t *odometer = NULL;
    uint32_t ndim = ndarray->ndim;
    asdf_scalar_datatype_t src_t = ndarray->datatype.type;
    asdf_ndarray_err_t err = ASDF_NDARRAY_ERR_INVAL;

    if (dst_t == ASDF_DATATYPE_SOURCE)
        dst_t = src_t;

    size_t src_elsize = asdf_scalar_datatype_size(src_t);
    size_t dst_elsize = asdf_scalar_datatype_size(dst_t);

    // For not-yet-supported datatypes return ERR_INVAL
    if (src_elsize < 1 || dst_elsize < 1)
        return ASDF_NDARRAY_ERR_INVAL;

    // Check bounds
    if (!check_bounds(ndarray, origin, shape))
        return ASDF_NDARRAY_ERR_OUT_OF_BOUNDS;

    size_t tile_nelems = ndim > 0 ? 1 : 0;

    for (uint32_t dim = 0; dim < ndim; dim++) {
        tile_nelems *= shape[dim];
    }

    size_t src_tile_size = src_elsize * tile_nelems;
    size_t tile_size = dst_elsize * tile_nelems;
    size_t data_size = 0;
    const void *data = asdf_ndarray_data_raw(ndarray, &data_size);

    if (data_size < src_tile_size)
        return ASDF_NDARRAY_ERR_OUT_OF_BOUNDS;

    // If the function is passed a null pointer, allocate memory for the tile ourselves
    // User is responsible for freeing it.
    void *tile = *dst;

    if (!tile) {
        // NOLINTNEXTLINE(clang-analyzer-optin.portability.UnixAPI)
        tile = malloc(tile_size);
        new_buf = tile;
    }

    if (UNLIKELY(!tile))
        return ASDF_NDARRAY_ERR_OOM;

    // Special case, if size of the array is 0 just return now.  We do still malloc though even if
    // it's a bit pointless, just to ensure that the returned pointer can be freed successfully
    if (UNLIKELY(0 == ndim || 0 == tile_size)) {
        *dst = tile;
        return ASDF_NDARRAY_OK;
    }

    // Determine the copy strategy to use; right now this just handles whether-or-not byteswap
    // is needed, may have others depending on alignment, vectorization etc.
    bool byteswap = should_byteswap(src_elsize, ndarray->byteorder);
    asdf_ndarray_convert_fn_t convert = asdf_ndarray_get_convert_fn(src_t, dst_t, byteswap);

    if (convert == NULL) {
        const char *src_datatype = asdf_scalar_datatype_to_string(src_t);
        const char *dst_datatype = asdf_scalar_datatype_to_string(dst_t);
        ASDF_LOG(
            ndarray->internal->file,
            ASDF_LOG_WARN,
            "datatype conversion from \"%s\" to \"%s\" not supported for ndarray tile copy; "
            "source bytes will be copied without conversion",
            src_datatype,
            dst_datatype);
        memcpy(tile, data, src_tile_size);
        *dst = tile;
        return ASDF_NDARRAY_ERR_CONVERSION;
    }

    bool overflow = false;
    // Determine element strides (assume C-order for now; ndarray->strides is not used yet)
    err = asdf_ndarray_read_tile_init_strides(ndarray->shape, ndim, &strides);

    if (err != ASDF_NDARRAY_OK)
        goto cleanup;

    uint32_t inner_dim = ndim - 1;
    uint64_t offset = origin[inner_dim];
    bool is_1d = true;

    if (ndim > 1) {
        for (uint32_t dim = 0; dim < inner_dim; dim++) {
            offset += origin[dim] * strides[dim];
            // If any of the outer dimensions are >1 than it's not a 1d tile
            is_1d &= (shape[dim] == 1);
        }
        offset *= src_elsize;
    } else {
        offset = origin[0] * src_elsize;
    }

    // Special case if the "tile" is one-dimensional, C-contiguous
    if (is_1d) {
        const void *src = data + offset;
        // If convert() returns non-zero it means an overflow occurred
        // while copying; this does not necessarily have to be treated as an error depending
        // on the application.
        overflow = convert(tile, src, tile_nelems, dst_elsize);
        err = overflow ? ASDF_NDARRAY_ERR_OVERFLOW : ASDF_NDARRAY_OK;
        *dst = tile;
        goto cleanup;
    }

    odometer = malloc(sizeof(uint64_t) * inner_dim);

    if (!odometer) {
        err = ASDF_NDARRAY_ERR_OOM;
        goto cleanup;
    }

    memcpy(odometer, origin, sizeof(uint64_t) * inner_dim);
    const void *src = data + offset;

    err = asdf_ndarray_read_tile_main_loop(
        tile, dst_elsize, src, src_elsize, shape, strides, origin, odometer, ndim, convert);
    *dst = tile;
cleanup:
    if (!overflow && err != ASDF_NDARRAY_OK)
        free(new_buf);

    free(strides);
    free(odometer);
    return err;
}


asdf_ndarray_err_t asdf_ndarray_read_all(
    asdf_ndarray_t *ndarray, asdf_scalar_datatype_t dst_t, void **dst) {
    if (UNLIKELY(!ndarray))
        // Invalid argument, must be non-NULL
        return ASDF_NDARRAY_ERR_INVAL;

    const uint64_t *origin = calloc(ndarray->ndim, sizeof(uint64_t));

    if (!origin)
        return ASDF_NDARRAY_ERR_OOM;

    asdf_ndarray_err_t err = asdf_ndarray_read_tile_ndim(
        ndarray, origin, ndarray->shape, dst_t, dst);

    free((void *)origin);
    return err;
}


asdf_ndarray_err_t asdf_ndarray_read_tile_2d(
    asdf_ndarray_t *ndarray,
    uint64_t x, // NOLINT(readability-identifier-length,bugprone-easily-swappable-parameters)
    uint64_t y, // NOLINT(readability-identifier-length)
    uint64_t width,
    uint64_t height,
    const uint64_t *plane_origin,
    asdf_scalar_datatype_t dst_t,
    void **dst) {

    uint64_t *origin = NULL;
    uint64_t *shape = NULL;
    uint32_t ndim = ndarray->ndim;
    asdf_ndarray_err_t err = ASDF_NDARRAY_ERR_OUT_OF_BOUNDS;

    if (ndim < 2)
        goto cleanup;

    origin = calloc(ndim, sizeof(uint64_t));
    shape = calloc(ndim, sizeof(uint64_t));

    if (UNLIKELY(!origin || !shape)) {
        err = ASDF_NDARRAY_ERR_OOM;
        goto cleanup;
    }

    uint32_t leading_ndim = ndim - 2;
    for (uint32_t dim = 0; dim < leading_ndim; dim++) {
        origin[dim] = plane_origin ? plane_origin[dim] : 0;
        shape[dim] = 1;
    }
    origin[ndim - 2] = y;
    origin[ndim - 1] = x;
    shape[ndim - 2] = height;
    shape[ndim - 1] = width;

    err = asdf_ndarray_read_tile_ndim(ndarray, origin, shape, dst_t, dst);
cleanup:
    free(origin);
    free(shape);
    return err;
}
