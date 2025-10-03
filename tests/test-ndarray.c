#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

#include <asdf/core/ndarray.h>
#include <asdf/file.h>

#include "munit.h"
#include "util.h"


/* Read contiguous 1-D "tiles" from arrays of different shapes */
MU_TEST(test_asdf_ndarray_read_1d_tile_contiguous) {
    const char *path = get_fixture_file_path("tiles.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
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
MU_TEST(test_asdf_ndarray_read_2d_tile) {
    const char *path = get_fixture_file_path("tiles.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
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
MU_TEST(test_asdf_ndarray_read_3d_tile) {
    const char *path = get_fixture_file_path("tiles.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
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


/* Helper for test_asdf_ndarray_read_tile_byteswap
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
MU_TEST(test_asdf_ndarray_read_tile_byteswap) {
    const char *path = get_fixture_file_path("byteorder.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
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


/** Helper functions for `test_asdf_ndarray_numeric_conversion` */

/**
 * For the purposes of `test_asdf_ndarray_numeric_conversion` will the result
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


MU_TEST(test_asdf_ndarray_numeric_conversion) {
    const char *src_dtype = munit_parameters_get(params, "src_dtype");
    const char *dst_dtype = munit_parameters_get(params, "dst_dtype");
    const char *src_byteorder = munit_parameters_get(params, "src_byteorder");
    asdf_scalar_datatype_t src_t = asdf_ndarray_datatype_from_string(src_dtype);
    asdf_scalar_datatype_t dst_t = asdf_ndarray_datatype_from_string(dst_dtype);
    assert_int(src_t, !=, ASDF_DATATYPE_UNKNOWN);
    assert_int(dst_t, !=, ASDF_DATATYPE_UNKNOWN);
    const char *path = get_fixture_file_path("numeric.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
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


MU_TEST(test_asdf_ndarray_record_datatype) {
    const char *path = get_fixture_file_path("datatypes.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    asdf_value_err_t err = asdf_get_ndarray(file, "record", &ndarray);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    asdf_datatype_t *datatype = &ndarray->datatype;
    assert_int(datatype->type, ==, ASDF_DATATYPE_RECORD);
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


MU_TEST(test_heap_use_after_free_issue_63) {
    const char *path = get_fixture_file_path("multiple_hdu.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    asdf_value_err_t err = asdf_get_ndarray(file, "TABLE", &ndarray);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);

    asdf_datatype_t *datatype = &ndarray->datatype;
    assert_int(datatype->type, ==, ASDF_DATATYPE_RECORD);
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


MU_TEST_SUITE(
    test_asdf_ndarray,
    MU_RUN_TEST(test_asdf_ndarray_read_1d_tile_contiguous),
    MU_RUN_TEST(test_asdf_ndarray_read_2d_tile),
    MU_RUN_TEST(test_asdf_ndarray_read_3d_tile),
    MU_RUN_TEST(test_asdf_ndarray_read_tile_byteswap),
    MU_RUN_TEST(test_asdf_ndarray_numeric_conversion, test_numeric_conversion_params),
    MU_RUN_TEST(test_asdf_ndarray_record_datatype),
    MU_RUN_TEST(test_heap_use_after_free_issue_63)
);


MU_RUN_SUITE(test_asdf_ndarray);
