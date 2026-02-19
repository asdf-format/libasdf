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

    asdf_sequence_iter_t stride_iter = asdf_sequence_iter_init();
    asdf_value_t *stride_val = NULL;
    size_t dim = 0;
    while ((stride_val = asdf_sequence_iter(value, &stride_iter))) {
        if (ASDF_VALUE_OK != asdf_value_as_int64(stride_val, &strides[dim])) {
            warn_invalid_strides(&value->value);
            goto failure;
        }

        if (0 == strides[dim]) {
            warn_invalid_strides(&value->value);
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


static asdf_value_err_t asdf_ndarray_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    uint64_t source = 0;
    asdf_value_t *datatype = NULL;
    asdf_mapping_t *ndarray_map = NULL;
    asdf_ndarray_t *ndarray = NULL;
    asdf_ndarray_internal_t *internal = NULL;
    bool is_inline = false;

    err = asdf_value_as_mapping(value, &ndarray_map);

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
        err = asdf_get_optional_property(ndarray_map, "data", ASDF_VALUE_SEQUENCE, NULL, NULL);

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
#ifdef ASDF_LOG_ENABLED
            const char *path = asdf_value_path(value);
            ASDF_LOG(
                value->file,
                ASDF_LOG_WARN,
                "ndarray at %s has inline data, but " PACKAGE_NAME " does not support "
                "inline data arrays yet",
                path);
#endif
            is_inline = true;
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

    internal = calloc(1, sizeof(asdf_ndarray_internal_t));

    if (UNLIKELY(!internal)) {
        err = ASDF_VALUE_ERR_OOM;
        goto cleanup;
    }

    ndarray->internal = internal;

    if (!is_inline) {
        err = asdf_ndarray_parse_block_data(ndarray_map, ndarray);

        if (ASDF_IS_ERR(err))
            goto cleanup;
    }

    /* Parse datatype */
    err = asdf_get_required_property(
        ndarray_map, "datatype", ASDF_VALUE_UNKNOWN, NULL, (void *)&datatype);

    if (!ASDF_IS_OK(err))
        goto cleanup;

    err = asdf_datatype_parse(datatype, ndarray->byteorder, &ndarray->datatype);

    if (ASDF_IS_ERR(err))
        goto cleanup;

    ndarray->source = source;
    ndarray->internal->file = value->file;
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

    if (ndarray->internal)
        asdf_block_close(ndarray->internal->block);

    free(ndarray->internal);
    free(ndarray->shape);
    free(ndarray->strides);
    asdf_datatype_clean(&ndarray->datatype);
    ZERO_MEMORY(ndarray, sizeof(asdf_ndarray_t));
    free(ndarray);
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

    if (!ndarray->internal || !ndarray->internal->data) {
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
    } else {
        uint64_t nbytes = asdf_ndarray_nbytes(ndarray);
        ssize_t block_idx = asdf_block_append(file, ndarray->internal->data, nbytes);

        if (block_idx < 0) {
            err = ASDF_VALUE_ERR_OOM;
            goto cleanup;
        }

        err = asdf_mapping_set_int64(ndarray_map, "source", (int64_t)block_idx);
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


static inline asdf_byteorder_t asdf_host_byteorder() {
    uint16_t one = 1;
    return (*(uint8_t *)&one) == 1 ? ASDF_BYTEORDER_LITTLE : ASDF_BYTEORDER_BIG;
}


/* ndarray methods */
const void *asdf_ndarray_data_raw(asdf_ndarray_t *ndarray, size_t *size) {
    if (!ndarray || !ndarray->internal)
        return NULL;

    // Return the user-allocated data
    if (ndarray->internal->data) {
        *size = asdf_ndarray_nbytes(ndarray);
        return ndarray->internal->data;
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

    if (ndarray->internal && ndarray->internal->data)
        return ndarray->internal->data;

    if (!ndarray->internal) {
        asdf_ndarray_internal_t *internal = calloc(1, sizeof(asdf_ndarray_internal_t));

        if (!internal)
            return NULL;

        ndarray->internal = internal;
    }

    uint64_t size = asdf_ndarray_nbytes(ndarray);
    void *data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (data == MAP_FAILED || !data) {
        free(ndarray->internal);
        ndarray->internal = NULL;
        return NULL;
    }

    ndarray->internal->data = data;
    return data;
}


void asdf_ndarray_data_dealloc(asdf_ndarray_t *ndarray) {
    if (UNLIKELY(!ndarray))
        return;

    if (UNLIKELY(!ndarray->internal || !ndarray->internal->data)) {
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
    munmap(ndarray->internal->data, size);
    asdf_block_close(ndarray->internal->block);
    free(ndarray->internal);
    ndarray->internal = NULL;
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
