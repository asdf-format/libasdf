#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "asdf/core/ndarray.h"
#include "asdf/file.h"

#include "munit.h"
#include "util.h"


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
    free(tile);
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
    free(tile);
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
    free(tile);
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
    free(tile);
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
    free(tile);
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
    free(tile);
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
    free(tile); \
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
    "int8", "uint8", "int16", "uint16", "int32", "uint32", "int64", "uint64",
    "float32", "float64", NULL};

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
 * certain cases.
 *
 * For now this is just a big dumb switch statement--in issue #50 we will
 * refactor asdf_scalar_datatype_t to contain more information that can be used to
 * determine this.
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
            return true;
        default:
            return false;
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


static void check_expected_values(void *arr, asdf_scalar_datatype_t src_t, asdf_scalar_datatype_t dst_t) {
    dtype_limits_t src_limits = get_dtype_limits(src_t);
    dtype_limits_t dst_limits = get_dtype_limits(dst_t);
    switch (src_t) {
    case ASDF_DATATYPE_FLOAT32:
    case ASDF_DATATYPE_FLOAT64: {
        double minval = normalize_to_double(arr, dst_t, 0);
        assert_double(minval, ==, clamp_value(src_limits.min, dst_limits.min, dst_limits.max));
        double neg_one = normalize_to_double(arr, dst_t, 1);
        assert_double(neg_one, ==, clamp_value(clamp_value(-1, src_limits.min, src_limits.max),
                                               dst_limits.min, dst_limits.max));
        double zero = normalize_to_double(arr, dst_t, 3);
        assert_double(zero, ==, 0);
        double one = normalize_to_double(arr, dst_t, 5);
        assert_double(one, ==, 1);
        double maxval = normalize_to_double(arr, dst_t, 6);
        double ulp = 1.0;
        double expected_maxval = clamp_value(src_limits.max, dst_limits.min, dst_limits.max);
        assert_double(fabs(maxval - expected_maxval), <=, ulp);
        double nan = normalize_to_double(arr, dst_t, 7);
        switch (dst_t) {
        case ASDF_DATATYPE_FLOAT32:
        case ASDF_DATATYPE_FLOAT64:
            assert_true(isnan(nan));
            break;
        default:
            // Conversion of NaNs to integers is not well-defined; see issue #58
            break;
        }
        double inf = normalize_to_double(arr, dst_t, 8);
        double neg_inf = normalize_to_double(arr, dst_t, 9);
        switch (dst_t) {
        case ASDF_DATATYPE_FLOAT32:
        case ASDF_DATATYPE_FLOAT64:
            assert_true(isinf(inf));
            assert_true(isinf(neg_inf));
            assert_true(neg_inf < 0);
            break;
        default:
            assert_double(inf, ==, clamp_value(INFINITY, dst_limits.min, dst_limits.max));
            assert_double(neg_inf, ==, clamp_value(-INFINITY, dst_limits.min, dst_limits.max));
            break;
        }
        break;
    }
    default: {
        double minval = normalize_to_double(arr, dst_t, 0);
        assert_double(minval, ==, clamp_value(src_limits.min, dst_limits.min, dst_limits.max));
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
        double expected_maxval = clamp_value(src_limits.max, dst_limits.min, dst_limits.max);
        assert_double(fabs(maxval - expected_maxval), <=, ulp);
    }
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

    if (should_overflow(src_t, dst_t))
        assert_int(n_err, ==, ASDF_NDARRAY_ERR_OVERFLOW);
    else
        assert_int(n_err, ==, ASDF_NDARRAY_OK);

    assert_not_null(array);
    check_expected_values(array, src_t, dst_t);
    free(array);
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

    // Test the ndarray with explicit datatype -- this should convert the raw
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

    /* Redirect warnings to a temp log file.  get_temp_file_path returns a
     * static buffer, so duplicate the path before calling it again. */
    const char *log_path_tmp = get_temp_file_path(fixture->tempfile_prefix, ".log");
    char *log_path = strdup(log_path_tmp);
    assert_not_null(log_path);
    FILE *log_stream = fopen(log_path, "w");
    assert_not_null(log_stream);

    asdf_config_t config = {
        .log = {.stream = log_stream, .level = ASDF_LOG_WARN, .no_color = true},
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
    fclose(log_stream);
    asdf_ndarray_data_dealloc(&ndarray);

    /* Verify that the warning was written to the log */
    size_t log_len = 0;
    char *log_content = read_file(log_path, &log_len);
    free(log_path);
    assert_not_null(log_content);
    assert_not_null(strstr(log_content, "exceeding the threshold of 99"));
    free(log_content);
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


/* Regression test for the bug where asdf_ndarray_data_alloc_temp leaked the
 * asdf_ndarray_internal_t created by a prior asdf_ndarray_storage_set call
 * (and silently dropped the array_storage setting). */
MU_TEST(ndarray_data_alloc_temp_storage_set_ordering) {
    uint64_t shape[1] = {4};
    asdf_ndarray_t nd = {
        .datatype = {.type = ASDF_DATATYPE_UINT8, .size = 1},
        .byteorder = ASDF_BYTEORDER_LITTLE,
        .ndim = 1,
        .shape = shape,
    };

    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);

    /* Call storage_set BEFORE data_alloc_temp -- the natural authoring order.
     * Prior to the fix, data_alloc_temp would leak the internal created here
     * and discard the array_storage setting. */
    asdf_ndarray_storage_set(&nd, ASDF_ARRAY_STORAGE_INLINE);
    uint8_t *data = asdf_ndarray_data_alloc_temp(file, &nd);
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
    MU_RUN_TEST(ndarray_data_alloc_temp_storage_set_ordering)
);


MU_RUN_SUITE(ndarray);
