#ifndef _GNU_SOURCE
#define _GNU_SOURCE  /* for memmem */
#endif
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asdf/core/ndarray.h"
#include "asdf/extension.h"
#include "asdf/extension_util.h"
#include "asdf/file.h"
#include "asdf/value.h"

#include "compat/numeric.h"

#include "munit.h"
#include "util.h"


/*
 * A stripped-down "affine"-like transform extension with a single "matrix"
 * ndarray property, modeled on the affine transform in libasdf-gwcs.  It
 * exercises the pattern where an extension's serialize callback builds an
 * ndarray on the stack, allocates its data, and embeds it, without any
 * explicit data_dealloc, so that ndarray_extension_embedded_ndarray can
 * verify writing such an extension does not leak the matrix data.
 */
typedef struct {
    double *matrix; /* n x n, row-major */
    uint32_t n;
} test_affine_t;


static asdf_version_t test_affine_version = {.version = "1.0.0", .minor = 1};


static asdf_software_t test_affine_software = {
    .name = "asdf-tests",
    .author = "STScI",
    .homepage = "https://stsci.edu",
    .version = &test_affine_version};


static asdf_value_t *test_affine_serialize(
    asdf_file_t *file, const void *obj, UNUSED(const void *userdata)) {
    const test_affine_t *affine = obj;

    if (!affine->matrix || affine->n == 0)
        return NULL;

    asdf_mapping_t *map = asdf_mapping_create(file);

    if (!map)
        return NULL;

    uint64_t shape[2] = {affine->n, affine->n};
    asdf_ndarray_t mat = {
        .ndim = 2,
        .shape = shape,
        .datatype = {.type = ASDF_DATATYPE_FLOAT64},
        .byteorder = ASDF_BYTEORDER_LITTLE,
    };
    asdf_ndarray_storage_set(&mat, ASDF_ARRAY_STORAGE_INLINE);

    if (asdf_ndarray_data_copy(&mat, affine->matrix) != ASDF_NDARRAY_OK)
        goto err;

    /* Assigning the ndarray transfers its data to the file; no data_dealloc */
    asdf_value_t *mat_val = asdf_value_of_ndarray(file, &mat);

    if (!mat_val)
        goto err;

    if (ASDF_IS_ERR(asdf_mapping_set(map, "matrix", mat_val))) {
        asdf_value_destroy(mat_val);
        goto err;
    }

    return asdf_value_of_mapping(map);
err:
    asdf_mapping_destroy(map);
    return NULL;
}


static asdf_value_err_t test_affine_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_mapping_t *map = NULL;
    asdf_ndarray_t *mat_arr = NULL;
    test_affine_t *affine = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;

    if (asdf_value_as_mapping(value, &map) != ASDF_VALUE_OK)
        goto cleanup;

    err = asdf_get_required_property(
        map, "matrix", ASDF_VALUE_EXTENSION, ASDF_CORE_NDARRAY_TAG, (void *)&mat_arr);

    if (ASDF_IS_ERR(err))
        goto cleanup;

    if (mat_arr->ndim != 2 || mat_arr->shape[0] != mat_arr->shape[1]) {
        err = ASDF_VALUE_ERR_PARSE_FAILURE;
        goto cleanup;
    }

    affine = calloc(1, sizeof(test_affine_t));

    if (!affine) {
        err = ASDF_VALUE_ERR_OOM;
        goto cleanup;
    }

    affine->n = (uint32_t)mat_arr->shape[0];

    if (asdf_ndarray_read_all(mat_arr, ASDF_DATATYPE_FLOAT64, (void **)&affine->matrix) !=
        ASDF_NDARRAY_OK) {
        free(affine);
        affine = NULL;
        err = ASDF_VALUE_ERR_PARSE_FAILURE;
        goto cleanup;
    }

    *out = affine;
    err = ASDF_VALUE_OK;
cleanup:
    /* map is a reinterpretation of `value`, owned by the caller; do not destroy */
    asdf_ndarray_destroy(mat_arr);
    return err;
}


static void test_affine_deinit_impl(void *value) {
    test_affine_t *affine = value;

    if (affine) {
        free(affine->matrix);
        affine->matrix = NULL;
    }
}


static const asdf_extension_vtab_t test_affine_vtab = {
    .serialize = test_affine_serialize,
    .deserialize = test_affine_deserialize,
    .deinit = test_affine_deinit_impl,
};


// clang-format off
ASDF_REGISTER_EXTENSION(
    test_affine,
    test_affine_t,
    &test_affine_software,
    &test_affine_vtab,
    NULL,
    "stsci.edu:asdf/tests/affine-1.0.0")
// clang-format on


/* Read contiguous 1-D "tiles" from arrays of different shapes */
MU_TEST(ndarray_read_1d_tile_contiguous) {
    const char *path = get_fixture_file_path("tiles.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_ndarray_t *ndarray = NULL;

    /* Read tile from a 1-D array */
    assert_int(asdf_get_ndarray(file, "1d", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    uint64_t origin1[] = {1};
    uint64_t shape1[] = {2};
    uint8_t expected1[] = {2, 3};
    void* tile = NULL;
    asdf_ndarray_err_t err = asdf_ndarray_read_tile_ndim(
        ndarray, origin1, shape1, ASDF_DATATYPE_SOURCE, &tile);
    assert_int(err, ==, ASDF_NDARRAY_OK);
    assert_not_null(tile);
    assert_memory_equal(2 * sizeof(uint8_t), tile, expected1);
    asdf_free(tile);
    asdf_ndarray_destroy(ndarray);

    /* Read tile from a 2-D array */
    assert_int(asdf_get_ndarray(file, "2d", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    uint64_t origin2[] = {1, 1};
    uint64_t shape2[] = {1, 2};
    uint16_t expected2[] = {22, 23};
    tile = NULL;
    err = asdf_ndarray_read_tile_ndim(ndarray, origin2, shape2, ASDF_DATATYPE_SOURCE, &tile);
    assert_int(err, ==, ASDF_NDARRAY_OK);
    assert_not_null(tile);
    assert_memory_equal(2 * sizeof(uint16_t), tile, expected2);
    asdf_free(tile);
    asdf_ndarray_destroy(ndarray);

    /* Read tile from a 3-D array */
    assert_int(asdf_get_ndarray(file, "3d", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    uint64_t origin3[] = {1, 1, 1};
    uint64_t shape3[] = {1, 1, 2};
    int32_t expected3[] = {222, 223};
    tile = NULL;
    err = asdf_ndarray_read_tile_ndim(ndarray, origin3, shape3, ASDF_DATATYPE_SOURCE, &tile);
    assert_int(err, ==, ASDF_NDARRAY_OK);
    assert_not_null(tile);
    assert_memory_equal(2 * sizeof(int32_t), tile, expected3);
    asdf_free(tile);
    asdf_ndarray_destroy(ndarray);

    asdf_close(file);
    return MUNIT_OK;
}


/* Read 2-D tiles from 2-D and 3-D arrays */
MU_TEST(test_asdf_ndarray_read_tile_2d) {
    const char *path = get_fixture_file_path("tiles.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_ndarray_t *ndarray = NULL;

    /* Read tile from a 2-D array */
    assert_int(asdf_get_ndarray(file, "2d", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    uint16_t expected2[2][2] = {{22, 23}, {32, 33}};
    void* tile = NULL;
    asdf_ndarray_err_t err = asdf_ndarray_read_tile_2d(
        ndarray, 1, 1, 2, 2, NULL, ASDF_DATATYPE_SOURCE, &tile);
    assert_int(err, ==, ASDF_NDARRAY_OK);
    assert_not_null(tile);
    assert_memory_equal(4 * sizeof(uint16_t), tile, expected2);
    asdf_free(tile);
    asdf_ndarray_destroy(ndarray);

    /* Read 2-D tile from the 1th layer of a 3-D array */
    assert_int(asdf_get_ndarray(file, "3d", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    uint64_t origin3[] = {1};
    int32_t expected3[2][2] = {{222, 223}, {232, 233}};
    tile = NULL;
    err = asdf_ndarray_read_tile_2d(ndarray, 1, 1, 2, 2, origin3, ASDF_DATATYPE_SOURCE, &tile);
    assert_int(err, ==, ASDF_NDARRAY_OK);
    assert_not_null(tile);
    assert_memory_equal(4 * sizeof(int32_t), tile, expected3);
    asdf_free(tile);
    asdf_ndarray_destroy(ndarray);

    asdf_close(file);
    return MUNIT_OK;
}


/* Read a 3-D cube from a a 3-D array */
MU_TEST(ndarray_read_3d_tile) {
    const char *path = get_fixture_file_path("tiles.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_ndarray_t *ndarray = NULL;
    assert_int(asdf_get_ndarray(file, "3d", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    uint64_t origin3[] = {1, 1, 1};
    uint64_t shape3[] = {2, 2, 2};
    int32_t expected3[2][2][2] = {{{222, 223}, {232, 233}}, {{322, 323}, {332, 333}}};
    void *tile = NULL;
    asdf_ndarray_err_t err = asdf_ndarray_read_tile_ndim(
        ndarray, origin3, shape3, ASDF_DATATYPE_SOURCE, &tile);
    assert_int(err, ==, ASDF_NDARRAY_OK);
    assert_not_null(tile);
    assert_memory_equal(8 * sizeof(int32_t), tile, expected3);
    asdf_free(tile);
    asdf_ndarray_destroy(ndarray);

    asdf_close(file);
    return MUNIT_OK;
}


/* Helper for ndarray_read_tile_byteswap
 *
 * Each array in byteorder.asdf just contains 0...7 in different int types, different
 * endianness
 */
#define CHECK_BYTEORDER_ARRAY(dtype, endian) do { \
    asdf_ndarray_t *ndarray = NULL; \
    const char *array_name = NULL; \
    if (ASDF_BYTEORDER_LITTLE == (endian)) \
        array_name = #dtype "-little"; \
    else \
        array_name = #dtype "-big"; \
    assert_int(asdf_get_ndarray(file, array_name, &ndarray), ==, ASDF_VALUE_OK); \
    assert_not_null(ndarray); \
    assert_int(ndarray->byteorder, ==, (endian)); \
    void *tile = NULL; \
    uint64_t origin[] = {0}; \
    asdf_ndarray_err_t err = asdf_ndarray_read_tile_ndim(ndarray, origin, ndarray->shape, ASDF_DATATYPE_SOURCE, &tile); \
    assert_int(err, ==, ASDF_NDARRAY_OK); \
    assert_not_null(tile); \
    dtype##_t expected[] = {0, 1, 2, 3, 4, 5, 6, 7}; \
    assert_memory_equal(8 * sizeof(dtype##_t), tile, expected); \
    asdf_free(tile); \
    asdf_ndarray_destroy(ndarray); \
} while (0)


/* Test reading from (1-D) arrays with different byte orders */
MU_TEST(ndarray_read_tile_byteswap) {
    const char *path = get_fixture_file_path("byteorder.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);
    CHECK_BYTEORDER_ARRAY(uint16, ASDF_BYTEORDER_LITTLE);
    CHECK_BYTEORDER_ARRAY(uint16, ASDF_BYTEORDER_BIG);
    CHECK_BYTEORDER_ARRAY(uint32, ASDF_BYTEORDER_LITTLE);
    CHECK_BYTEORDER_ARRAY(uint32, ASDF_BYTEORDER_BIG);
    CHECK_BYTEORDER_ARRAY(uint64, ASDF_BYTEORDER_LITTLE);
    CHECK_BYTEORDER_ARRAY(uint64, ASDF_BYTEORDER_BIG);
    asdf_close(file);
    return MUNIT_OK;
}


static char *supported_numeric_dtypes[] = {
    "int8",
    "uint8",
    "int16",
    "uint16",
    "int32",
    "uint32",
    "int64",
    "uint64",
    "float16",
    "float32",
    "float64",
    NULL};

static char *endianness[] = {"<", ">", NULL};

static MunitParameterEnum test_numeric_conversion_params[] = {
    {"src_dtype", supported_numeric_dtypes},
    {"dst_dtype", supported_numeric_dtypes},
    {"src_byteorder", endianness},
    {NULL, NULL}
};


/** Helper functions for `ndarray_numeric_conversion` */

/**
 * For the purposes of `ndarray_numeric_conversion` will the result
 * overflow for a given source and destination datatype
 *
 * This is not a guaranteed overflow in general (as it depends on the data in
 * the arrays, but this test always has boundary values that will overflow in
 * certain cases).
 *
 * For now this is just a big dumb switch statement--in issue #50 we will
 * refactor asdf_scalar_datatype_t to contain more information that can be used to
 * determine this.
 *
 * .. todo::
 *
 *  This was not, in fact, handled as part of issue #50 but it would be worth
 *  having some standard APIs to determine datatype info / limits similar to
 *  np.iinfo and np.finfo for example.
 */
static bool should_overflow(asdf_scalar_datatype_t src_t, asdf_scalar_datatype_t dst_t) {
    if (src_t == dst_t)
        return false;

    switch (src_t) {
    case ASDF_DATATYPE_INT8:
        switch(dst_t) {
        case ASDF_DATATYPE_UINT8:
        case ASDF_DATATYPE_UINT16:
        case ASDF_DATATYPE_UINT32:
        case ASDF_DATATYPE_UINT64:
            return true;
        default:
            return false;
        }
    case ASDF_DATATYPE_UINT8:
        switch(dst_t) {
        case ASDF_DATATYPE_INT8:
            return true;
        default:
            return false;
        }
    case ASDF_DATATYPE_INT16:
        switch(dst_t) {
        case ASDF_DATATYPE_INT8:
        case ASDF_DATATYPE_UINT8:
        case ASDF_DATATYPE_UINT16:
        case ASDF_DATATYPE_UINT32:
        case ASDF_DATATYPE_UINT64:
            return true;
        default:
            return false;
        }
    case ASDF_DATATYPE_UINT16:
        switch(dst_t) {
        case ASDF_DATATYPE_INT8:
        case ASDF_DATATYPE_UINT8:
        case ASDF_DATATYPE_INT16:
        case ASDF_DATATYPE_FLOAT16:
            return true;
        default:
            return false;
        }
    case ASDF_DATATYPE_INT32:
        switch(dst_t) {
        case ASDF_DATATYPE_INT8:
        case ASDF_DATATYPE_INT16:
        case ASDF_DATATYPE_UINT8:
        case ASDF_DATATYPE_UINT16:
        case ASDF_DATATYPE_UINT32:
        case ASDF_DATATYPE_UINT64:
        case ASDF_DATATYPE_FLOAT16:
            return true;
        default:
            return false;
        }
    case ASDF_DATATYPE_UINT32:
        switch(dst_t) {
        case ASDF_DATATYPE_INT8:
        case ASDF_DATATYPE_UINT8:
        case ASDF_DATATYPE_INT16:
        case ASDF_DATATYPE_UINT16:
        case ASDF_DATATYPE_INT32:
        case ASDF_DATATYPE_FLOAT16:
            return true;
        default:
            return false;
        }
    case ASDF_DATATYPE_INT64:
        switch(dst_t) {
        case ASDF_DATATYPE_INT8:
        case ASDF_DATATYPE_UINT8:
        case ASDF_DATATYPE_INT16:
        case ASDF_DATATYPE_UINT16:
        case ASDF_DATATYPE_INT32:
        case ASDF_DATATYPE_UINT32:
        case ASDF_DATATYPE_UINT64:
        case ASDF_DATATYPE_FLOAT16:
            return true;
        default:
            return false;
        }
    case ASDF_DATATYPE_UINT64:
        switch(dst_t) {
        case ASDF_DATATYPE_INT8:
        case ASDF_DATATYPE_UINT8:
        case ASDF_DATATYPE_INT16:
        case ASDF_DATATYPE_UINT16:
        case ASDF_DATATYPE_INT32:
        case ASDF_DATATYPE_UINT32:
        case ASDF_DATATYPE_INT64:
        case ASDF_DATATYPE_FLOAT16:
            return true;
        default:
            return false;
        }
    case ASDF_DATATYPE_FLOAT16:
        switch(dst_t) {
        case ASDF_DATATYPE_FLOAT32:
        case ASDF_DATATYPE_FLOAT64:
            return false;
        default:
            return true;
        }
    case ASDF_DATATYPE_FLOAT32:
        switch(dst_t) {
        case ASDF_DATATYPE_FLOAT64:
            return false;
        default:
            return true;
        }
    case ASDF_DATATYPE_FLOAT64:
        return true;
    default:
        return true;
    }
    return true;
}


static double normalize_to_double(const void *src, asdf_scalar_datatype_t src_type, size_t idx) {
    switch (src_type) {
        case ASDF_DATATYPE_INT8:    return ((const int8_t*)src)[idx];
        case ASDF_DATATYPE_UINT8:   return ((const uint8_t*)src)[idx];
        case ASDF_DATATYPE_INT16:   return ((const int16_t*)src)[idx];
        case ASDF_DATATYPE_UINT16:  return ((const uint16_t*)src)[idx];
        case ASDF_DATATYPE_INT32:   return ((const int32_t*)src)[idx];
        case ASDF_DATATYPE_UINT32:  return ((const uint32_t*)src)[idx];
        case ASDF_DATATYPE_INT64:   return ((const int64_t*)src)[idx];
        case ASDF_DATATYPE_UINT64:  return ((const uint64_t*)src)[idx];
#ifdef HAVE_FLOAT16
        case ASDF_DATATYPE_FLOAT16: return ((const half*)src)[idx];
#endif
        case ASDF_DATATYPE_FLOAT32: return ((const float*)src)[idx];
        case ASDF_DATATYPE_FLOAT64: return ((const double*)src)[idx];
        default: return 0; // or handle error
    }
}


typedef struct {
    double min;
    double max;
} dtype_limits_t;


static inline dtype_limits_t get_dtype_limits(asdf_scalar_datatype_t t) {
    switch (t) {
    case ASDF_DATATYPE_INT8:   return (dtype_limits_t){ INT8_MIN,  INT8_MAX };
    case ASDF_DATATYPE_INT16:  return (dtype_limits_t){ INT16_MIN, INT16_MAX };
    case ASDF_DATATYPE_INT32:  return (dtype_limits_t){ INT32_MIN, INT32_MAX };
    // NOTE: Casting (U)INT64_MAX to double loses the exact value, but for this test
    // it's OK.
    case ASDF_DATATYPE_INT64:  return (dtype_limits_t){ INT64_MIN, (double)INT64_MAX };
    case ASDF_DATATYPE_UINT8:  return (dtype_limits_t){ 0,         UINT8_MAX };
    case ASDF_DATATYPE_UINT16: return (dtype_limits_t){ 0,         UINT16_MAX };
    case ASDF_DATATYPE_UINT32: return (dtype_limits_t){ 0,         UINT32_MAX };
    case ASDF_DATATYPE_UINT64: return (dtype_limits_t){ 0,         (double)UINT64_MAX };
#ifdef HAVE_FLOAT16
    case ASDF_DATATYPE_FLOAT16: return (dtype_limits_t){ -FLT16_MAX, FLT16_MAX };
#endif
    case ASDF_DATATYPE_FLOAT32:return (dtype_limits_t){ -FLT_MAX,  FLT_MAX };
    case ASDF_DATATYPE_FLOAT64:return (dtype_limits_t){ -DBL_MAX,  DBL_MAX };
    default:
        break;
    }
    return (dtype_limits_t){0, 0};
}


static double clamp_value(double val, double min, double max) {
    if (val < min)
        return min;
    else if (val > max)
        return max;

    return val;
}


static inline bool is_float_dtype(asdf_scalar_datatype_t t) {
    switch (t) {
    case ASDF_DATATYPE_FLOAT16:
    case ASDF_DATATYPE_FLOAT32:
    case ASDF_DATATYPE_FLOAT64:
        return true;
    default:
        return false;
    }
}


/**
 * Expected result of narrowing ``val`` into ``dst_t``.
 *
 * For a floating-point destination a finite value beyond the destination's
 * finite range overflows to +/-infinity (IEC 60559 / NumPy semantics); for an
 * integer destination it saturates to the destination min/max.
 */
static double expected_narrowed(double val, asdf_scalar_datatype_t dst_t, dtype_limits_t dst_limits) {
    if (is_float_dtype(dst_t)) {
        if (val > dst_limits.max)
            return INFINITY;
        if (val < dst_limits.min)
            return -INFINITY;
        return val;
    }
    return clamp_value(val, dst_limits.min, dst_limits.max);
}


static void check_expected_values(void *arr, asdf_scalar_datatype_t src_t, asdf_scalar_datatype_t dst_t) {
    dtype_limits_t src_limits = get_dtype_limits(src_t);
    dtype_limits_t dst_limits = get_dtype_limits(dst_t);

    if (is_float_dtype(src_t)) {
        double minval = normalize_to_double(arr, dst_t, 0);
        assert_double(minval, ==, expected_narrowed(src_limits.min, dst_t, dst_limits));
        double neg_one = normalize_to_double(arr, dst_t, 1);
        assert_double(neg_one, ==, clamp_value(clamp_value(-1, src_limits.min, src_limits.max),
                                               dst_limits.min, dst_limits.max));
        double zero = normalize_to_double(arr, dst_t, 3);
        assert_double(zero, ==, 0);
        double one = normalize_to_double(arr, dst_t, 5);
        assert_double(one, ==, 1);
        double maxval = normalize_to_double(arr, dst_t, 6);
        double ulp = 1.0;
        double expected_maxval = expected_narrowed(src_limits.max, dst_t, dst_limits);
        if (isinf(expected_maxval))
            assert_double(maxval, ==, expected_maxval);
        else
            assert_double(fabs(maxval - expected_maxval), <=, ulp);

        double nan = normalize_to_double(arr, dst_t, 7);

        if (is_float_dtype(dst_t)) {
            assert_true(isnan(nan));
            // Conversion of NaNs to integers is not well-defined; see issue #58
        }

        double inf = normalize_to_double(arr, dst_t, 8);
        double neg_inf = normalize_to_double(arr, dst_t, 9);

        if (is_float_dtype(dst_t)) {
            assert_true(isinf(inf));
            assert_true(isinf(neg_inf));
            assert_true(neg_inf < 0);
        } else {
            assert_double(inf, ==, clamp_value(INFINITY, dst_limits.min, dst_limits.max));
            assert_double(neg_inf, ==, clamp_value(-INFINITY, dst_limits.min, dst_limits.max));
        }
    } else {
        double minval = normalize_to_double(arr, dst_t, 0);
        assert_double(minval, ==, expected_narrowed(src_limits.min, dst_t, dst_limits));
        double neg_one = normalize_to_double(arr, dst_t, 1);
        assert_double(neg_one, ==, clamp_value(clamp_value(-1, src_limits.min, src_limits.max),
                                               dst_limits.min, dst_limits.max));
        double zero = normalize_to_double(arr, dst_t, 2);
        assert_double(zero, ==, 0);
        double one = normalize_to_double(arr, dst_t, 3);
        assert_double(one, ==, 1);
        // Here we can run into some trouble with float rounding
        double maxval = normalize_to_double(arr, dst_t, 4);
        double ulp = 1.0;
        double expected_maxval = expected_narrowed(src_limits.max, dst_t, dst_limits);
        if (isinf(expected_maxval))
            assert_double(maxval, ==, expected_maxval);
        else
            assert_double(fabs(maxval - expected_maxval), <=, ulp);
    }
}


static char *append_char(const char *src, char c) {
    if (!src) return NULL;

    size_t len = strlen(src);
    char *buf = malloc(len + 2);
    if (!buf) return NULL;

    memcpy(buf, src, len);
    buf[len] = c;
    buf[len + 1] = '\0';
    return buf;
}


MU_TEST(ndarray_numeric_conversion) {
    const char *src_dtype = munit_parameters_get(params, "src_dtype");
    const char *dst_dtype = munit_parameters_get(params, "dst_dtype");
    const char *src_byteorder = munit_parameters_get(params, "src_byteorder");
    asdf_scalar_datatype_t src_t = asdf_scalar_datatype_from_string(src_dtype);
    asdf_scalar_datatype_t dst_t = asdf_scalar_datatype_from_string(dst_dtype);
    assert_int(src_t, !=, ASDF_DATATYPE_UNKNOWN);
    assert_int(dst_t, !=, ASDF_DATATYPE_UNKNOWN);
    const char *path = get_fixture_file_path("numeric.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);
    char *key = append_char(src_dtype, src_byteorder[0]);
    asdf_ndarray_t *ndarray = NULL;
    asdf_value_err_t err = asdf_get_ndarray(file, key, &ndarray);
    free(key);
    assert_int(err, ==, ASDF_VALUE_OK);
    void *array = NULL;
    asdf_ndarray_err_t n_err = asdf_ndarray_read_all(ndarray, dst_t, &array);

#ifndef HAVE_FLOAT16
    /* If float16 is not supported the correct behavior
     * is to return an error
     */
    if (src_t == ASDF_DATATYPE_FLOAT16 || dst_t == ASDF_DATATYPE_FLOAT16) {
        munit_log(
            MUNIT_LOG_INFO, "src or destination datatype is float16 but "
            "libasdf compiled without float16 support; test should pass "
            "only if ASDF_NDARRAY_ERR_CONVERSION was returned");
        assert_int(n_err, ==, ASDF_NDARRAY_ERR_CONVERSION);
        goto cleanup;
    }
#endif

    if (should_overflow(src_t, dst_t))
        assert_int(n_err, ==, ASDF_NDARRAY_ERR_OVERFLOW);
    else
        assert_int(n_err, ==, ASDF_NDARRAY_OK);

    assert_not_null(array);
    check_expected_values(array, src_t, dst_t);
#ifndef HAVE_FLOAT16
cleanup:
#endif
    asdf_free(array);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(ndarray_structured_datatype) {
    const char *path = get_fixture_file_path("datatypes.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    asdf_value_err_t err = asdf_get_ndarray(file, "structured", &ndarray);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    asdf_datatype_t *datatype = &ndarray->datatype;
    assert_int(datatype->type, ==, ASDF_DATATYPE_STRUCTURED);
    // sizeof(S4) + sizeof(U4) + sizeof(int16) + 3 * 3 * sizeof(float)
    assert_int(datatype->size, ==, 58);
    assert_null(datatype->name);
    assert_int(datatype->byteorder, ==, ASDF_BYTEORDER_BIG);
    assert_int(datatype->ndim, ==, 0);
    assert_null(datatype->shape);
    assert_int(datatype->nfields, ==, 4);
    assert_not_null(datatype->fields);

    // Test each field
    // S4
    const asdf_datatype_t *field = &datatype->fields[0];
    assert_int(field->type, ==, ASDF_DATATYPE_ASCII);
    assert_int(field->size, ==, 4);
    assert_not_null(field->name);
    assert_string_equal(field->name, "string");
    assert_int(field->byteorder, ==, ASDF_BYTEORDER_BIG);
    assert_int(field->ndim, ==, 0);
    assert_null(field->shape);
    assert_int(field->nfields, ==, 0);
    assert_null(field->fields);

    // U4
    field = &datatype->fields[1];
    assert_int(field->type, ==, ASDF_DATATYPE_UCS4);
    assert_int(field->size, ==, 16);
    assert_not_null(field->name);
    assert_string_equal(field->name, "unicode");
    assert_int(field->byteorder, ==, ASDF_BYTEORDER_LITTLE);
    assert_int(field->ndim, ==, 0);
    assert_null(field->shape);
    assert_int(field->nfields, ==, 0);
    assert_null(field->fields);

    // int16
    field = &datatype->fields[2];
    assert_int(field->type, ==, ASDF_DATATYPE_INT16);
    assert_int(field->size, ==, 2);
    assert_not_null(field->name);
    assert_string_equal(field->name, "int");
    assert_int(field->byteorder, ==, ASDF_BYTEORDER_BIG);
    assert_int(field->ndim, ==, 0);
    assert_null(field->shape);
    assert_int(field->nfields, ==, 0);
    assert_null(field->fields);

    // (3x3) float32
    field = &datatype->fields[3];
    assert_int(field->type, ==, ASDF_DATATYPE_FLOAT32);
    assert_int(field->size, ==, 36);
    assert_not_null(field->name);
    assert_string_equal(field->name, "matrix");
    assert_int(field->byteorder, ==, ASDF_BYTEORDER_LITTLE);
    assert_int(field->ndim, ==, 2);
    uint64_t expected_shape[] = {3, 3};
    assert_not_null(field->shape);
    assert_memory_equal(2 * sizeof(uint64_t), field->shape, expected_shape);
    assert_int(field->nfields, ==, 0);
    assert_null(field->fields);

    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(ndarray_read_inline_data) {
    const char *path = get_fixture_file_path("ndarray-inline.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    asdf_value_err_t err = asdf_get_ndarray(file, "implicit", &ndarray);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);

    uint64_t expected_shape[2] = {3, 3};
    assert_int(ndarray->ndim, ==, 2);

    for (uint32_t idx = 0; idx < ndarray->ndim; idx++)
        assert_int(ndarray->shape[idx], ==, expected_shape[idx]);

    asdf_datatype_t *datatype = &ndarray->datatype;
    assert_int(datatype->type, ==, ASDF_DATATYPE_UINT8);
    size_t size = 0;
    const void *data = asdf_ndarray_data(ndarray, &size);
    assert_not_null(data);
    assert_int(size, ==, 3 * 3 * (int)sizeof(uint8_t));

    // Expected array data ; just 0 to 8
    for (int idx = 0; idx < 3 * 3; idx++)
        assert_int(((uint8_t *)data)[idx], ==, idx);

    asdf_ndarray_destroy(ndarray);
    ndarray = NULL;

    // Test the ndarray with explicit datatype. This should convert the raw
    // values in the inline data that are formatted as ints to doubles
    err = asdf_get_ndarray(file, "explicit", &ndarray);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);

    assert_int(ndarray->ndim, ==, 2);

    for (uint32_t idx = 0; idx < ndarray->ndim; idx++)
        assert_int(ndarray->shape[idx], ==, expected_shape[idx]);

    datatype = &ndarray->datatype;
    assert_int(datatype->type, ==, ASDF_DATATYPE_FLOAT64);
    size = 0;
    data = asdf_ndarray_data(ndarray, &size);
    assert_not_null(data);
    assert_int(size, ==, 3 * 3 * (int)sizeof(double));

    // Expected array data ; just 0 to 8
    for (int idx = 0; idx < 3 * 3; idx++)
        assert_double(((double *)data)[idx], ==, (double)idx);

    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(ndarray_write_empty_inline_data) {
    const char *out_path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_ndarray_t ndarray = {
        .datatype = {.type = ASDF_DATATYPE_UINT8},
        .byteorder = ASDF_BYTEORDER_LITTLE,
        .ndim = 0,
        .shape = NULL,
    };
    // For now it's still necessary to do a data_alloc even for empty ndarrays;
    // although not strictly the case it's particlarly needed for use with a
    // parallel data_dealloc or will result in memory leaks;
    (void)asdf_ndarray_data_alloc(&ndarray);
    asdf_ndarray_storage_set(&ndarray, ASDF_ARRAY_STORAGE_INLINE);

    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);

    assert_int(asdf_set_ndarray(file, "empty", &ndarray), ==, ASDF_VALUE_OK);
    assert_int(asdf_write_to(file, out_path), ==, 0);
    asdf_ndarray_data_dealloc(&ndarray);
    asdf_close(file);

    asdf_ndarray_t *ndarray_in = NULL;
    file = asdf_open(out_path, "r");
    assert_not_null(file);
    assert_int(asdf_get_ndarray(file, "empty", &ndarray_in), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray_in);
    assert_int(ndarray_in->ndim, ==, 0);
    assert_int(asdf_ndarray_storage(ndarray_in), ==, ASDF_ARRAY_STORAGE_INLINE);
    asdf_ndarray_destroy(ndarray_in);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(ndarray_write_inline_data) {
    const char *out_path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    uint64_t shape[2] = {3, 3};

    /* Build uint8 3x3 ndarray with values 0-8 for the "implicit" key */
    asdf_ndarray_t implicit_nd = {
        .datatype = {.type = ASDF_DATATYPE_UINT8, .size = 1},
        .byteorder = ASDF_BYTEORDER_LITTLE,
        .ndim = 2,
        .shape = shape,
    };
    uint8_t *impl_data = asdf_ndarray_data_alloc(&implicit_nd);
    assert_not_null(impl_data);
    for (int idx = 0; idx < 9; idx++)
        impl_data[idx] = (uint8_t)idx;
    asdf_ndarray_storage_set(&implicit_nd, ASDF_ARRAY_STORAGE_INLINE);

    /* Build float64 3x3 ndarray with values 0.0-8.0 for the "explicit" key */
    asdf_ndarray_t explicit_nd = {
        .datatype = {.type = ASDF_DATATYPE_FLOAT64, .size = sizeof(double)},
        .byteorder = ASDF_BYTEORDER_LITTLE,
        .ndim = 2,
        .shape = shape,
    };
    double *expl_data = asdf_ndarray_data_alloc(&explicit_nd);
    assert_not_null(expl_data);
    for (int idx = 0; idx < 9; idx++)
        expl_data[idx] = (double)idx;
    asdf_ndarray_storage_set(&explicit_nd, ASDF_ARRAY_STORAGE_INLINE);

    /* Write both ndarrays to a temp file */
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);

    asdf_value_t *implicit_val = asdf_value_of_ndarray(file, &implicit_nd);
    assert_not_null(implicit_val);
    assert_int(asdf_set_value(file, "implicit", implicit_val), ==, ASDF_VALUE_OK);

    asdf_value_t *explicit_val = asdf_value_of_ndarray(file, &explicit_nd);
    assert_not_null(explicit_val);
    assert_int(asdf_set_value(file, "explicit", explicit_val), ==, ASDF_VALUE_OK);

    assert_int(asdf_write_to(file, out_path), ==, 0);
    asdf_close(file);
    asdf_ndarray_data_dealloc(&implicit_nd);
    asdf_ndarray_data_dealloc(&explicit_nd);

    /* Read back and verify both ndarrays */
    file = asdf_open(out_path, "r");
    assert_not_null(file);

    asdf_ndarray_t *ndarray = NULL;
    assert_int(asdf_get_ndarray(file, "implicit", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    assert_int(ndarray->ndim, ==, 2);
    assert_int((int)ndarray->shape[0], ==, 3);
    assert_int((int)ndarray->shape[1], ==, 3);
    assert_int(ndarray->datatype.type, ==, ASDF_DATATYPE_UINT8);
    assert_int(asdf_ndarray_storage(ndarray), ==, ASDF_ARRAY_STORAGE_INLINE);

    size_t size = 0;
    const void *data = asdf_ndarray_data(ndarray, &size);
    assert_not_null(data);
    assert_int((int)size, ==, 9 * (int)sizeof(uint8_t));
    for (int idx = 0; idx < 9; idx++)
        assert_int(((const uint8_t *)data)[idx], ==, idx);
    asdf_ndarray_destroy(ndarray);
    ndarray = NULL;

    assert_int(asdf_get_ndarray(file, "explicit", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    assert_int(ndarray->ndim, ==, 2);
    assert_int((int)ndarray->shape[0], ==, 3);
    assert_int((int)ndarray->shape[1], ==, 3);
    assert_int(ndarray->datatype.type, ==, ASDF_DATATYPE_FLOAT64);
    assert_int(asdf_ndarray_storage(ndarray), ==, ASDF_ARRAY_STORAGE_INLINE);

    size = 0;
    data = asdf_ndarray_data(ndarray, &size);
    assert_not_null(data);
    assert_int((int)size, ==, 9 * (int)sizeof(double));
    for (int idx = 0; idx < 9; idx++)
        assert_double(((const double *)data)[idx], ==, (double)idx);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(ndarray_inline_warning_thresh) {
    uint64_t shape[1] = {100};
    asdf_ndarray_t ndarray = {
        .datatype = {.type = ASDF_DATATYPE_UINT8},
        .byteorder = ASDF_BYTEORDER_LITTLE,
        .ndim = 1,
        .shape = shape,
    };
    uint8_t *data = asdf_ndarray_data_alloc(&ndarray);
    assert_not_null(data);
    memset(data, 0, (size_t)shape[0]);
    asdf_ndarray_storage_set(&ndarray, ASDF_ARRAY_STORAGE_INLINE);

    log_capture_t cap = {0};
    asdf_config_t config = {
        .log = LOG_CAPTURE_CFG(&cap, ASDF_LOG_WARN),
        .emitter = {.inline_ndarray_warning_thresh = 99}
    };
    asdf_file_t *file = asdf_open_mem_ex(NULL, 0, &config);
    assert_not_null(file);

    asdf_value_t *val = asdf_value_of_ndarray(file, &ndarray);
    assert_not_null(val);
    assert_int(asdf_set_value(file, "data", val), ==, ASDF_VALUE_OK);

    const char *out_path = get_temp_file_path(fixture->tempfile_prefix, "-thresh.asdf");
    assert_int(asdf_write_to(file, out_path), ==, 0);
    asdf_close(file);
    asdf_ndarray_data_dealloc(&ndarray);

    /* Verify that the warning was logged */
    const asdf_log_rec_t *rec = log_capture_find(&cap, "exceeding the threshold of 99");
    assert_not_null(rec);
    assert_int(rec->level, ==, ASDF_LOG_WARN);
    log_capture_clear(&cap);
    return MUNIT_OK;
}


MU_TEST(heap_use_after_free_issue_63) {
    const char *path = get_fixture_file_path("multiple_hdu.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    asdf_value_err_t err = asdf_get_ndarray(file, "TABLE", &ndarray);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);

    asdf_datatype_t *datatype = &ndarray->datatype;
    assert_int(datatype->type, ==, ASDF_DATATYPE_STRUCTURED);
    // sizeof(S21) + sizeof(S21)
    assert_int(datatype->size, ==, 42);
    assert_null(datatype->name);
    assert_int(datatype->byteorder, ==, ASDF_BYTEORDER_BIG);
    assert_int(datatype->ndim, ==, 0);
    assert_null(datatype->shape);
    assert_int(datatype->nfields, ==, 2);

    const asdf_datatype_t *field = &datatype->fields[0];
    assert_int(field->type, ==, ASDF_DATATYPE_ASCII);
    assert_int(field->size, ==, 21);
    assert_not_null(field->name);
    assert_string_equal(field->name, "ID");
    assert_int(field->byteorder, ==, ASDF_BYTEORDER_BIG);
    assert_int(field->ndim, ==, 0);
    assert_null(field->shape);
    assert_int(field->nfields, ==, 0);
    assert_null(field->fields);

    field = &datatype->fields[1];
    assert_int(field->type, ==, ASDF_DATATYPE_ASCII);
    assert_int(field->size, ==, 21);
    assert_not_null(field->name);
    assert_string_equal(field->name, "X");
    assert_int(field->byteorder, ==, ASDF_BYTEORDER_BIG);
    assert_int(field->ndim, ==, 0);
    assert_null(field->shape);
    assert_int(field->nfields, ==, 0);
    assert_null(field->fields);

    assert_not_null(datatype->fields);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}


static char *array_storage_param_values[] = {"inline", "internal", NULL};
static MunitParameterEnum ndarray_array_storage_params[] = {
    {"storage", array_storage_param_values},
    {NULL, NULL}
};


/* Round-trip test for the file-level array_storage emitter override.
 *
 * Sets the per-array storage to the opposite of the file-level setting to
 * verify that the file-level config wins in both directions. */
MU_TEST(ndarray_array_storage_override) {
    const char *storage_str = munit_parameters_get(params, "storage");
    bool is_inline = strcmp(storage_str, "inline") == 0;
    asdf_array_storage_t storage =
        is_inline ? ASDF_ARRAY_STORAGE_INLINE : ASDF_ARRAY_STORAGE_INTERNAL;
    asdf_array_storage_t opposite =
        is_inline ? ASDF_ARRAY_STORAGE_INTERNAL : ASDF_ARRAY_STORAGE_INLINE;

    const char *out_path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    uint64_t shape[1] = {4};

    asdf_ndarray_t nd = {
        .datatype = {.type = ASDF_DATATYPE_UINT8, .size = 1},
        .byteorder = ASDF_BYTEORDER_LITTLE,
        .ndim = 1,
        .shape = shape,
    };
    uint8_t *data = asdf_ndarray_data_alloc(&nd);
    assert_not_null(data);
    for (int idx = 0; idx < 4; idx++)
        data[idx] = (uint8_t)(idx + 1);
    asdf_ndarray_storage_set(&nd, opposite);

    asdf_config_t cfg = {.emitter = {.array_storage = storage}};
    asdf_file_t *file = asdf_open_mem_ex(NULL, 0, &cfg);
    assert_not_null(file);

    asdf_value_t *val = asdf_value_of_ndarray(file, &nd);
    assert_not_null(val);
    assert_int(asdf_set_value(file, "arr", val), ==, ASDF_VALUE_OK);
    assert_int(asdf_write_to(file, out_path), ==, 0);
    asdf_close(file);
    asdf_ndarray_data_dealloc(&nd);

    file = asdf_open(out_path, "r");
    assert_not_null(file);
    assert_size(asdf_block_count(file), ==, is_inline ? 0 : 1);

    asdf_ndarray_t *ndarray_in = NULL;
    assert_int(asdf_get_ndarray(file, "arr", &ndarray_in), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray_in);
    assert_int(asdf_ndarray_storage(ndarray_in), ==, storage);

    size_t size = 0;
    const void *raw = asdf_ndarray_data(ndarray_in, &size);
    assert_not_null(raw);
    assert_size(size, ==, 4);
    for (int idx = 0; idx < 4; idx++)
        assert_int(((const uint8_t *)raw)[idx], ==, idx + 1);

    asdf_ndarray_destroy(ndarray_in);
    asdf_close(file);
    return MUNIT_OK;
}


/* Regression test for the bug where allocating a buffer leaked the
 * asdf_ndarray_internal_t created by a prior asdf_ndarray_storage_set call
 * (and silently dropped the array_storage setting): the natural authoring
 * order of storage_set() before data_alloc() must preserve both. */
MU_TEST(ndarray_data_alloc_storage_set_ordering) {
    uint64_t shape[1] = {4};
    asdf_ndarray_t nd = {
        .datatype = {.type = ASDF_DATATYPE_UINT8, .size = 1},
        .byteorder = ASDF_BYTEORDER_LITTLE,
        .ndim = 1,
        .shape = shape,
    };

    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);

    /* Call storage_set BEFORE data_alloc -- the natural authoring order.  The
     * allocation must reuse the internal created here rather than leaking it
     * and discarding the array_storage setting. */
    asdf_ndarray_storage_set(&nd, ASDF_ARRAY_STORAGE_INLINE);
    uint8_t *data = asdf_ndarray_data_alloc(&nd);
    assert_not_null(data);
    for (int idx = 0; idx < 4; idx++)
        data[idx] = (uint8_t)(idx + 1);

    assert_int(asdf_ndarray_storage(&nd), ==, ASDF_ARRAY_STORAGE_INLINE);

    const char *out_path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_value_t *val = asdf_value_of_ndarray(file, &nd);
    assert_not_null(val);
    assert_int(asdf_set_value(file, "arr", val), ==, ASDF_VALUE_OK);
    assert_int(asdf_write_to(file, out_path), ==, 0);
    asdf_close(file);
    asdf_ndarray_data_dealloc(&nd);

    file = asdf_open(out_path, "r");
    assert_not_null(file);
    assert_size(asdf_block_count(file), ==, 0);

    asdf_ndarray_t *nd_in = NULL;
    assert_int(asdf_get_ndarray(file, "arr", &nd_in), ==, ASDF_VALUE_OK);
    assert_not_null(nd_in);
    assert_int(asdf_ndarray_storage(nd_in), ==, ASDF_ARRAY_STORAGE_INLINE);

    size_t size = 0;
    const void *raw = asdf_ndarray_data(nd_in, &size);
    assert_not_null(raw);
    assert_size(size, ==, 4);
    for (int idx = 0; idx < 4; idx++)
        assert_int(((const uint8_t *)raw)[idx], ==, idx + 1);

    asdf_ndarray_destroy(nd_in);
    asdf_close(file);
    return MUNIT_OK;
}


/* Reading single elements with asdf_ndarray_at and friends */
MU_TEST(ndarray_read_at) {
    const char *path = get_fixture_file_path("tiles.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_ndarray_t *nd1 = NULL;
    asdf_ndarray_t *nd2 = NULL;
    assert_int(asdf_get_ndarray(file, "1d", &nd1), ==, ASDF_VALUE_OK);
    assert_int(asdf_get_ndarray(file, "2d", &nd2), ==, ASDF_VALUE_OK);

    asdf_ndarray_err_t err = ASDF_NDARRAY_ERR_INVAL;

    /* The 1-D array holds 1, 2, 3, 4 */
    assert_int(asdf_ndarray_at_err(nd1, uint8_t, &err, 1), ==, 2);
    assert_int(err, ==, ASDF_NDARRAY_OK);

    /* The element is converted to the requested type */
    assert_double(asdf_ndarray_at_err(nd1, double, &err, 2), ==, 3.0);
    assert_int(err, ==, ASDF_NDARRAY_OK);

    assert_int(asdf_ndarray_at_err(nd2, uint16_t, &err, 1, 1), ==, 22);
    assert_int(err, ==, ASDF_NDARRAY_OK);

    /* An index beyond the array's shape is out of bounds, not invalid */
    assert_int(asdf_ndarray_at_err(nd1, uint8_t, &err, 4), ==, 0);
    assert_int(err, ==, ASDF_NDARRAY_ERR_OUT_OF_BOUNDS);

    /* Too few indices for the array's ndim */
    assert_int(asdf_ndarray_at_err(nd2, uint16_t, &err, 1), ==, 0);
    assert_int(err, ==, ASDF_NDARRAY_ERR_INVAL);

    /* Too many indices for the array's ndim */
    assert_int(asdf_ndarray_at_err(nd1, uint8_t, &err, 1, 1), ==, 0);
    assert_int(err, ==, ASDF_NDARRAY_ERR_INVAL);

    /* The same conditions through the underlying functions */
    const uint64_t indices[] = {1, 1};
    assert_int(asdf_ndarray_read_uint16_at(nd2, indices, &err), ==, 22);
    assert_int(err, ==, ASDF_NDARRAY_OK);

    uint16_t value = 0;
    assert_int(
        asdf_ndarray_read_at(nd2, indices, ASDF_DATATYPE_UINT16, &value), ==, ASDF_NDARRAY_OK);
    assert_int(value, ==, 22);

    /* NULL indices, as the at() macros pass when given the wrong number */
    assert_int(
        asdf_ndarray_read_at(nd2, NULL, ASDF_DATATYPE_UINT16, &value), ==,
        ASDF_NDARRAY_ERR_INVAL);
    assert_int(asdf_ndarray_read_uint16_at(nd2, NULL, &err), ==, 0);
    assert_int(err, ==, ASDF_NDARRAY_ERR_INVAL);

    /* Errors are silent without the _err variant; the value is zero */
    assert_int(asdf_ndarray_at(nd2, uint16_t, 1), ==, 0);

    asdf_ndarray_destroy(nd1);
    asdf_ndarray_destroy(nd2);
    asdf_close(file);
    return MUNIT_OK;
}


/*
 * Read arrays that are contiguous views at an offset into a larger base
 * array's block (offset.asdf).  The offset is applied to the pointer
 * returned by asdf_ndarray_data (and asdf_ndarray_data_raw, since none of
 * these are compressed), and to all the read routines.
 *
 * For compressed arrays with offsets see ``read_compressed_ndarray_offset``
 * in ``test-compression.c`` which is similar, though this checks a wider
 * range of cases.  See the next test ``ndarray_offset_out_of_bounds`` for
 * OOB error handling; this test is all sane cases.
 */
MU_TEST(ndarray_offset) {
    const char *path = get_fixture_file_path("offset.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_ndarray_t *ndarray = NULL;
    asdf_ndarray_err_t err = ASDF_NDARRAY_ERR_INVAL;
    size_t size = 0;
    size_t raw_size = 0;
    void *buf = NULL;

    /* u1: arange(256)[16:] */
    assert_int(asdf_get_ndarray(file, "u1", &ndarray), ==, ASDF_VALUE_OK);
    assert_int(ndarray->offset, ==, 16);
    const uint8_t *u1_data = asdf_ndarray_data(ndarray, &size);
    assert_not_null(u1_data);
    assert_size(size, ==, 256 - 16);
    assert_uint8(u1_data[0], ==, 16);
    assert_uint8(u1_data[size - 1], ==, 255);
    assert_ptr_equal(asdf_ndarray_data_raw(ndarray, &raw_size), u1_data);
    assert_size(raw_size, ==, size);

    assert_int(asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_SOURCE, &buf), ==, ASDF_NDARRAY_OK);

    for (size_t idx = 0; idx < 240; idx++)
        assert_uint8(((uint8_t *)buf)[idx], ==, idx + 16);

    free(buf);
    buf = NULL;
    assert_uint8(asdf_ndarray_at_err(ndarray, uint8_t, &err, 0), ==, 16);
    assert_int(err, ==, ASDF_NDARRAY_OK);
    assert_uint8(asdf_ndarray_at_err(ndarray, uint8_t, &err, 239), ==, 255);
    assert_int(err, ==, ASDF_NDARRAY_OK);
    asdf_ndarray_destroy(ndarray);

    /* i4_2d: arange(100).reshape(10, 10)[3:] i.e. offset by 3 rows */
    assert_int(asdf_get_ndarray(file, "i4_2d", &ndarray), ==, ASDF_VALUE_OK);
    assert_int(ndarray->offset, ==, 3 * 10 * sizeof(int32_t));
    const int32_t *i4_data = asdf_ndarray_data(ndarray, &size);
    assert_not_null(i4_data);
    assert_size(size, ==, 7 * 10 * sizeof(int32_t));
    assert_int32(i4_data[0], ==, 30);

    assert_int(asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_INT32, &buf), ==, ASDF_NDARRAY_OK);

    for (int32_t idx = 0; idx < 70; idx++)
        assert_int32(((int32_t *)buf)[idx], ==, idx + 30);

    free(buf);
    buf = NULL;
    assert_int32(asdf_ndarray_at_err(ndarray, int32_t, &err, 0, 0), ==, 30);
    assert_int(err, ==, ASDF_NDARRAY_OK);
    assert_int32(asdf_ndarray_at_err(ndarray, int32_t, &err, 6, 9), ==, 99);
    assert_int(err, ==, ASDF_NDARRAY_OK);

    /* 2x2 tile at row 1, column 2 of the view */
    int32_t expected_tile[] = {42, 43, 52, 53};
    err = asdf_ndarray_read_tile_2d(ndarray, 2, 1, 2, 2, NULL, ASDF_DATATYPE_INT32, &buf);
    assert_int(err, ==, ASDF_NDARRAY_OK);
    assert_memory_equal(sizeof(expected_tile), buf, expected_tile);
    free(buf);
    buf = NULL;
    asdf_ndarray_destroy(ndarray);

    /* u2_be: big-endian arange(64)[5:], read with byteswapping */
    assert_int(asdf_get_ndarray(file, "u2_be", &ndarray), ==, ASDF_VALUE_OK);
    assert_int(ndarray->offset, ==, 5 * sizeof(uint16_t));
    const uint8_t *u2_data = asdf_ndarray_data(ndarray, &size);
    assert_not_null(u2_data);
    assert_size(size, ==, 59 * sizeof(uint16_t));
    assert_uint8(u2_data[0], ==, 0);
    assert_uint8(u2_data[1], ==, 5);

    assert_int(asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_UINT16, &buf), ==, ASDF_NDARRAY_OK);

    for (uint16_t idx = 0; idx < 59; idx++)
        assert_uint16(((uint16_t *)buf)[idx], ==, idx + 5);

    free(buf);
    buf = NULL;
    assert_uint16(asdf_ndarray_at_err(ndarray, uint16_t, &err, 58), ==, 63);
    assert_int(err, ==, ASDF_NDARRAY_OK);
    asdf_ndarray_destroy(ndarray);

    asdf_close(file);
    return MUNIT_OK;
}


/*
 * An offset that leaves too little (or no) block data for the array, or that
 * lies beyond the end of the block entirely
 */
MU_TEST(ndarray_offset_out_of_bounds) {
    const char *path = get_fixture_file_path("255.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_ndarray_t *ndarray = NULL;
    size_t size = 0;
    void *buf = NULL;
    assert_int(asdf_get_ndarray(file, "data", &ndarray), ==, ASDF_VALUE_OK);
    assert_int(ndarray->shape[0], ==, 256);

    /* Leaves 1 byte of the 256-byte block for a 256-element array
     * (shape is still set to [256] which is why this is OOB) */
    ndarray->offset = 255;
    const uint8_t *data = asdf_ndarray_data(ndarray, &size);
    assert_not_null(data);
    assert_size(size, ==, 1);
    assert_uint8(data[0], ==, 255);
    assert_int(
        asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_SOURCE, &buf), ==,
        ASDF_NDARRAY_ERR_OUT_OF_BOUNDS);
    assert_null(buf);

    /* Exactly at the end of the block: an empty, but valid, pointer */
    ndarray->offset = 256;
    assert_not_null(asdf_ndarray_data(ndarray, &size));
    assert_size(size, ==, 0);
    assert_int(
        asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_SOURCE, &buf), ==,
        ASDF_NDARRAY_ERR_OUT_OF_BOUNDS);
    assert_null(buf);

    /* Beyond the end of the block */
    ndarray->offset = 300;
    size = 1;
    assert_null(asdf_ndarray_data(ndarray, &size));
    assert_size(size, ==, 0);
    size = 1;
    assert_null(asdf_ndarray_data_raw(ndarray, &size));
    assert_size(size, ==, 0);
    assert_int(
        asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_SOURCE, &buf), ==,
        ASDF_NDARRAY_ERR_OUT_OF_BOUNDS);
    assert_null(buf);

    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}

/* Strided ndarray tests */

/*
 * Expected contents (in C order) of a strided test array, given by its
 * logical values in the numpy expression that generated it
 */
typedef struct {
    const char *name;
    uint32_t ndim;
    uint64_t shape[3];
    const double *expected;
} strided_case_t;

/* Expected values (see ``make_strides_asdf`` in ``make_test_fixtures.py``) */

/* arange(32, dtype='<u2')[::2] */
static const double every_other_expected[] = {
    0, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30};

/* asfortranarray(arange(12, dtype='<i4').reshape(3, 4)) */
static const double fortran_expected[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

/* arange(12, dtype='u1').reshape(3, 4).T */
static const double transpose_expected[] = {0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11};

/* arange(20, dtype='<i4').reshape(4, 5)[:, :2] */
static const double cols_expected[] = {0, 1, 5, 6, 10, 11, 15, 16};

/* arange(60, dtype='>u2').reshape(3, 4, 5).transpose(2, 0, 1) */
static const double cube_perm_expected[] = {
    0,  5,  10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 1,  6,  11, 16, 21, 26, 31, 36,
    41, 46, 51, 56, 2,  7,  12, 17, 22, 27, 32, 37, 42, 47, 52, 57, 3,  8,  13, 18,
    23, 28, 33, 38, 43, 48, 53, 58, 4,  9,  14, 19, 24, 29, 34, 39, 44, 49, 54, 59};

/* arange(10, dtype='u1').reshape(10, 1)[::3] */
static const double unit_dim_expected[] = {0, 3, 6, 9};


static const strided_case_t strided_cases[] = {
    {"every_other", 1, {16}, every_other_expected},
    {"fortran", 2, {3, 4}, fortran_expected},
    {"transpose", 2, {4, 3}, transpose_expected},
    {"cols", 2, {4, 2}, cols_expected},
    {"cube_perm", 3, {5, 3, 4}, cube_perm_expected},
    {"unit_dim", 2, {4, 1}, unit_dim_expected},
};


/* Convert a C-order flat index into a multi-dimensional index */
static void unravel_index(uint64_t flat, const uint64_t *shape, uint32_t ndim, uint64_t *index) {
    for (uint32_t dim = ndim; dim-- > 0;) {
        index[dim] = flat % shape[dim];
        flat /= shape[dim];
    }
}


/* Convert a multi-dimensional index into a C-order flat index */
static uint64_t ravel_index(const uint64_t *index, const uint64_t *shape, uint32_t ndim) {
    uint64_t flat = 0;

    for (uint32_t dim = 0; dim < ndim; dim++)
        flat = (flat * shape[dim]) + index[dim];

    return flat;
}


/*
 * Check that a strided array reads back as expected: all at once, element by
 * element, and as a sub-tile (everything but the first row/column/etc. along
 * each dimension of length > 1)
 */
static void check_strided_case(asdf_file_t *file, const strided_case_t *tcase) {
    asdf_ndarray_t *ndarray = NULL;
    asdf_ndarray_err_t err = ASDF_NDARRAY_ERR_INVAL;
    uint64_t nelem = 1;
    uint64_t index[3] = {0};
    uint64_t origin[3] = {0};
    uint64_t tile_shape[3] = {0};
    uint64_t tile_nelem = 1;
    double *buf = NULL;

    munit_logf(MUNIT_LOG_INFO, "checking strided array %s", tcase->name);
    assert_int(asdf_get_ndarray(file, tcase->name, &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray->strides);
    assert_uint32(ndarray->ndim, ==, tcase->ndim);

    for (uint32_t dim = 0; dim < tcase->ndim; dim++) {
        assert_uint64(ndarray->shape[dim], ==, tcase->shape[dim]);
        nelem *= tcase->shape[dim];
        origin[dim] = tcase->shape[dim] > 1 ? 1 : 0;
        tile_shape[dim] = tcase->shape[dim] - origin[dim];
        tile_nelem *= tile_shape[dim];
    }

    /* The whole array, converted to double (element-wise for strided rows) */
    err = asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_FLOAT64, (void **)&buf);
    assert_int(err, ==, ASDF_NDARRAY_OK);

    for (uint64_t idx = 0; idx < nelem; idx++)
        assert_double(buf[idx], ==, tcase->expected[idx]);

    free(buf);
    buf = NULL;

    /* Each element individually */
    for (uint64_t idx = 0; idx < nelem; idx++) {
        unravel_index(idx, tcase->shape, tcase->ndim, index);
        uint64_t value = asdf_ndarray_read_uint64_at(ndarray, index, &err);
        assert_int(err, ==, ASDF_NDARRAY_OK);
        assert_double((double)value, ==, tcase->expected[idx]);
    }

    /* A sub-tile not starting at the origin */
    err = asdf_ndarray_read_tile_ndim(
        ndarray, origin, tile_shape, ASDF_DATATYPE_FLOAT64, (void **)&buf);
    assert_int(err, ==, ASDF_NDARRAY_OK);

    for (uint64_t idx = 0; idx < tile_nelem; idx++) {
        unravel_index(idx, tile_shape, tcase->ndim, index);

        for (uint32_t dim = 0; dim < tcase->ndim; dim++)
            index[dim] += origin[dim];

        uint64_t flat = ravel_index(index, tcase->shape, tcase->ndim);
        assert_double(buf[idx], ==, tcase->expected[flat]);
    }

    free(buf);
    asdf_ndarray_destroy(ndarray);
}


/* Read arrays with various non-trivial strides (strides.asdf) */
MU_TEST(ndarray_strides) {
    const char *path = get_fixture_file_path("strides.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    for (size_t idx = 0; idx < sizeof(strided_cases) / sizeof(strided_cases[0]); idx++)
        check_strided_case(file, &strided_cases[idx]);

    /* Reads in the source datatype (no conversion) for a contiguous inner
     * dimension with byteswapping, and a non-contiguous one without */
    asdf_ndarray_t *ndarray = NULL;
    void *buf = NULL;
    assert_int(asdf_get_ndarray(file, "cube_perm", &ndarray), ==, ASDF_VALUE_OK);
    assert_int(asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_SOURCE, &buf), ==, ASDF_NDARRAY_OK);

    for (size_t idx = 0; idx < 60; idx++)
        assert_uint16(((uint16_t *)buf)[idx], ==, (uint16_t)cube_perm_expected[idx]);

    free(buf);
    buf = NULL;
    asdf_ndarray_destroy(ndarray);

    assert_int(asdf_get_ndarray(file, "cols", &ndarray), ==, ASDF_VALUE_OK);
    assert_int(asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_SOURCE, &buf), ==, ASDF_NDARRAY_OK);

    for (size_t idx = 0; idx < 8; idx++)
        assert_int32(((int32_t *)buf)[idx], ==, (int32_t)cols_expected[idx]);

    free(buf);
    buf = NULL;

    /* A single row of a 2-D array with a padded outer stride */
    int32_t expected_row[] = {10, 11};
    asdf_ndarray_err_t err = asdf_ndarray_read_tile_2d(
        ndarray, 0, 2, 2, 1, NULL, ASDF_DATATYPE_INT32, &buf);
    assert_int(err, ==, ASDF_NDARRAY_OK);
    assert_memory_equal(sizeof(expected_row), buf, expected_row);
    free(buf);
    asdf_ndarray_destroy(ndarray);

    asdf_close(file);
    return MUNIT_OK;
}


/*
 * Strides that are just the C-contiguous strides for the array's shape are
 * ignored, both when reading and writing (they are not written out)
 */
MU_TEST(ndarray_strides_trivial) {
    const char *path = get_fixture_file_path("tiles.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_ndarray_t *ndarray = NULL;
    void *expected = NULL;
    void *buf = NULL;
    void *out = NULL;
    size_t out_size = 0;

    /* 4x4x4 little-endian int32 */
    assert_int(asdf_get_ndarray(file, "3d", &ndarray), ==, ASDF_VALUE_OK);
    assert_null(ndarray->strides);
    assert_int(
        asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_SOURCE, &expected), ==, ASDF_NDARRAY_OK);

    int64_t *strides = malloc(3 * sizeof(int64_t));
    assert_not_null(strides);
    /* The default C-contiguous strides, usually left null */
    strides[0] = 64;
    strides[1] = 16;
    strides[2] = 4;
    ndarray->strides = strides;

    assert_int(asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_SOURCE, &buf), ==, ASDF_NDARRAY_OK);
    assert_memory_equal(64 * sizeof(int32_t), buf, expected);
    free(buf);
    buf = NULL;

    uint64_t indices[] = {3, 2, 1};
    asdf_ndarray_err_t err = ASDF_NDARRAY_ERR_INVAL;
    assert_int32(
        asdf_ndarray_read_int32_at(ndarray, indices, &err), ==, 432);
    assert_int(err, ==, ASDF_NDARRAY_OK);

    /* Written to a new file the trivial strides are dropped */
    asdf_file_t *out_file = asdf_open(NULL);
    assert_not_null(out_file);
    asdf_ndarray_t *copy = asdf_ndarray_copy(out_file, ndarray);
    assert_not_null(copy);
    assert_int(asdf_set_ndarray(out_file, "data", copy), ==, ASDF_VALUE_OK);
    assert_int(asdf_write_to(out_file, &out, &out_size), ==, 0);
    asdf_ndarray_destroy(copy);
    asdf_close(out_file);
    assert_null(memmem(out, out_size, "strides", strlen("strides")));
    free(out);
    out = NULL;

    free(expected);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);

    /* Whereas non-trivial strides are written */
    path = get_fixture_file_path("strides.asdf");
    file = asdf_open(path, "r");
    assert_not_null(file);
    assert_int(asdf_get_ndarray(file, "fortran", &ndarray), ==, ASDF_VALUE_OK);
    out_file = asdf_open(NULL);
    assert_not_null(out_file);
    copy = asdf_ndarray_copy(out_file, ndarray);
    assert_not_null(copy);
    assert_int(asdf_set_ndarray(out_file, "data", copy), ==, ASDF_VALUE_OK);
    assert_int(asdf_write_to(out_file, &out, &out_size), ==, 0);
    asdf_ndarray_destroy(copy);
    asdf_close(out_file);
    assert_not_null(memmem(out, out_size, "strides:", strlen("strides:")));
    free(out);

    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}


/* Strides that would read past the end of the block data */
MU_TEST(ndarray_strides_out_of_bounds) {
    const char *path = get_fixture_file_path("strides.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_ndarray_t *ndarray = NULL;
    asdf_ndarray_err_t err = ASDF_NDARRAY_ERR_INVAL;
    void *buf = NULL;

    /* every_other spans 15 * 4 + 2 = 62 of its 64-byte block; offsetting it
     * by one element still fits, shifting its values by one */
    assert_int(asdf_get_ndarray(file, "every_other", &ndarray), ==, ASDF_VALUE_OK);
    ndarray->offset = 2;
    assert_uint16(asdf_ndarray_at_err(ndarray, uint16_t, &err, 0), ==, 1);
    assert_int(err, ==, ASDF_NDARRAY_OK);
    assert_uint16(asdf_ndarray_at_err(ndarray, uint16_t, &err, 15), ==, 31);
    assert_int(err, ==, ASDF_NDARRAY_OK);

    /* But by two elements the last element is beyond the block, so any read
     * is out of bounds, not just of that element */
    ndarray->offset = 4;
    assert_uint16(asdf_ndarray_at_err(ndarray, uint16_t, &err, 0), ==, 0);
    assert_int(err, ==, ASDF_NDARRAY_ERR_OUT_OF_BOUNDS);
    assert_int(
        asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_SOURCE, &buf), ==,
        ASDF_NDARRAY_ERR_OUT_OF_BOUNDS);
    assert_null(buf);

    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}


/*
 * A strided array written with inline storage is written as contiguous
 * (C-order) inline data without strides
 */
MU_TEST(ndarray_strides_inline_roundtrip) {
    const char *path = get_fixture_file_path("strides.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    const char *names[] = {"fortran", "transpose", "cols"};
    const double *expected[] = {fortran_expected, transpose_expected, cols_expected};
    const size_t nelems[] = {12, 12, 8};

    for (size_t idx = 0; idx < 3; idx++) {
        asdf_ndarray_t *ndarray = NULL;
        void *out = NULL;
        size_t out_size = 0;
        double *buf = NULL;

        assert_int(asdf_get_ndarray(file, names[idx], &ndarray), ==, ASDF_VALUE_OK);
        asdf_file_t *out_file = asdf_open(NULL);
        assert_not_null(out_file);
        asdf_ndarray_t *copy = asdf_ndarray_copy(out_file, ndarray);
        assert_not_null(copy);
        asdf_ndarray_storage_set(copy, ASDF_ARRAY_STORAGE_INLINE);
        assert_int(asdf_set_ndarray(out_file, "data", copy), ==, ASDF_VALUE_OK);
        assert_int(asdf_write_to(out_file, &out, &out_size), ==, 0);
        asdf_ndarray_destroy(copy);
        asdf_close(out_file);
        asdf_ndarray_destroy(ndarray);
        assert_null(memmem(out, out_size, "strides", strlen("strides")));

        asdf_file_t *in_file = asdf_open_mem(out, out_size);
        assert_not_null(in_file);
        assert_size(asdf_block_count(in_file), ==, 0);
        assert_int(asdf_get_ndarray(in_file, "data", &ndarray), ==, ASDF_VALUE_OK);
        assert_null(ndarray->strides);
        assert_int(
            asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_FLOAT64, (void **)&buf), ==,
            ASDF_NDARRAY_OK);

        for (size_t jdx = 0; jdx < nelems[idx]; jdx++)
            assert_double(buf[jdx], ==, expected[idx][jdx]);

        free(buf);
        asdf_ndarray_destroy(ndarray);
        asdf_close(in_file);
        free(out);
    }

    asdf_close(file);
    return MUNIT_OK;
}


/* Regression tests */


/*
 * Write and read back an extension that embeds an ndarray as one of its
 * properties (the test_affine extension above).  The extension's serialize
 * callback builds the matrix ndarray on the stack and allocates its data but
 * never calls asdf_ndarray_data_dealloc. Assigning the ndarray to the file
 * transfers ownership of its data to the file, which frees it after the write.
 *
 * This demonstrates use cases exercised in libasdf-gwcs, and possibly other
 * extensions as well.
 *
 * Run under ASan this verifies the embedded ndarray's data is not leaked.
 */
MU_TEST(ndarray_extension_embedded_ndarray) {
    double matrix[4] = {1.0, 2.0, 3.0, 4.0};
    test_affine_t affine = {.matrix = matrix, .n = 2};

    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);

    asdf_value_t *val = asdf_value_of_test_affine(file, &affine);
    assert_not_null(val);
    assert_int(asdf_set_value(file, "transform", val), ==, ASDF_VALUE_OK);

    void *buf = NULL;
    size_t size = 0;
    assert_int(asdf_write_to(file, &buf, &size), ==, 0);
    asdf_close(file);

    /* Read the transform back and verify the matrix round-tripped */
    file = asdf_open_mem(buf, size);
    assert_not_null(file);

    test_affine_t *affine_in = NULL;
    assert_int(asdf_get_test_affine(file, "transform", &affine_in), ==, ASDF_VALUE_OK);
    assert_not_null(affine_in);
    assert_int(affine_in->n, ==, 2);
    assert_not_null(affine_in->matrix);
    for (int idx = 0; idx < 4; idx++)
        assert_double(affine_in->matrix[idx], ==, matrix[idx]);

    asdf_test_affine_destroy(affine_in);
    asdf_close(file);
    asdf_free(buf);
    return MUNIT_OK;
}


/* A shape whose byte size overflows 64 bits is refused rather than read into
 * an undersized buffer */
MU_TEST(ndarray_read_size_overflow) {
    const char *path = get_fixture_file_path("ndarray-size-overflow.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_ndarray_t *ndarray = NULL;
    assert_int(asdf_get_ndarray(file, "arr", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    assert_uint64(asdf_ndarray_nbytes(ndarray), ==, UINT64_MAX);

    void *data = NULL;
    assert_int(
        asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_INT32, &data),
        ==,
        ASDF_NDARRAY_ERR_OUT_OF_BOUNDS);
    assert_null(data);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}


/* A tile whose origin plus shape overflows is out of bounds */
MU_TEST(ndarray_read_tile_bounds_overflow) {
    const char *path = get_fixture_file_path("tiles.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_ndarray_t *ndarray = NULL;
    assert_int(asdf_get_ndarray(file, "1d", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    uint64_t origin[] = {UINT64_MAX};
    uint64_t shape[] = {2};
    void *tile = NULL;
    assert_int(
        asdf_ndarray_read_tile_ndim(ndarray, origin, shape, ASDF_DATATYPE_SOURCE, &tile),
        ==,
        ASDF_NDARRAY_ERR_OUT_OF_BOUNDS);
    assert_null(tile);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    ndarray,
    MU_RUN_TEST(ndarray_read_1d_tile_contiguous),
    MU_RUN_TEST(test_asdf_ndarray_read_tile_2d),
    MU_RUN_TEST(ndarray_read_3d_tile),
    MU_RUN_TEST(ndarray_read_tile_byteswap),
    MU_RUN_TEST(ndarray_numeric_conversion, test_numeric_conversion_params),
    MU_RUN_TEST(ndarray_structured_datatype),
    MU_RUN_TEST(ndarray_read_inline_data),
    MU_RUN_TEST(ndarray_write_empty_inline_data),
    MU_RUN_TEST(ndarray_write_inline_data),
    MU_RUN_TEST(ndarray_inline_warning_thresh),
    MU_RUN_TEST(ndarray_array_storage_override, ndarray_array_storage_params),
    MU_RUN_TEST(heap_use_after_free_issue_63),
    MU_RUN_TEST(ndarray_data_alloc_storage_set_ordering),
    MU_RUN_TEST(ndarray_read_at),
    MU_RUN_TEST(ndarray_offset),
    MU_RUN_TEST(ndarray_offset_out_of_bounds),
    MU_RUN_TEST(ndarray_strides),
    MU_RUN_TEST(ndarray_strides_trivial),
    MU_RUN_TEST(ndarray_strides_out_of_bounds),
    MU_RUN_TEST(ndarray_strides_inline_roundtrip),
    MU_RUN_TEST(ndarray_extension_embedded_ndarray),
    MU_RUN_TEST(ndarray_read_size_overflow),
    MU_RUN_TEST(ndarray_read_tile_bounds_overflow)
);


MU_RUN_SUITE(ndarray);
