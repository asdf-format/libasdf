#include <asdf/file.h>
#include <asdf/value.h>

#include "munit.h"
#include "util.h"


/*
 * Very basic test of the `asdf_open_file` interface
 *
 * Tests opening/closing file, and reading a basic value out of the tree
 */
MU_TEST(test_asdf_open_file) {
    const char *filename = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    /* Read some key out of the tree */
    asdf_value_t *value = asdf_get_value(file, "asdf_library/name");
    assert_not_null(value);
    const char *name = NULL;
    asdf_value_err_t err = asdf_value_as_string0(value, &name);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(name);
    assert_string_equal(name, "asdf");
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


#define CHECK_GET_INT(type, key, expected) \
    do { \
        type##_t __v = 0; \
        assert_true(asdf_is_int(file, (key))); \
        bool __is = asdf_is_##type(file, (key)); \
        assert_true(__is); \
        asdf_value_err_t __err = asdf_get_##type(file, (key), &__v); \
        assert_int(__err, ==, ASDF_VALUE_OK); \
        type##_t __ve = (expected); \
        assert_int(__v, ==, __ve); \
    } while (0)


/* Test the high-level asdf_is_* and asdf_get_* helpers */
MU_TEST(test_asdf_scalar_getters) {
    const char *filename = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    assert_true(asdf_is_string(file, "plain"));
    const char *s = NULL;
    size_t len = 0;
    assert_int(asdf_get_string(file, "plain", &s, &len), ==, ASDF_VALUE_OK);
    char *s0 = strndup(s, len);
    assert_string_equal(s0, "string");
    free(s0);

    assert_true(asdf_is_bool(file, "false"));
    bool b = true;
    assert_int(asdf_get_bool(file, "false", &b), ==, ASDF_VALUE_OK);
    assert_false(b);

    assert_true(asdf_is_null(file, "null"));

    CHECK_GET_INT(int8, "int8", 127);
    CHECK_GET_INT(int16, "int16", 32767);
    CHECK_GET_INT(int32, "int32", 2147483647);
    CHECK_GET_INT(int64, "int64", 9223372036854775807LL);
    CHECK_GET_INT(uint8, "uint8", 255);
    CHECK_GET_INT(uint16, "uint16", 65535);
    CHECK_GET_INT(uint32, "uint32", 4294967295);
    CHECK_GET_INT(uint64, "uint64", 18446744073709551615ULL);

    float f = 0;
    assert_true(asdf_is_float(file, "float32"));
    assert_int(asdf_get_float(file, "float32", &f), ==, ASDF_VALUE_OK);
    assert_float(f, ==, 0.15625);

    double d = 0;
    assert_true(asdf_is_double(file, "float64"));
    assert_int(asdf_get_double(file, "float64", &d), ==, ASDF_VALUE_OK);
    assert_double(d, ==, 1.000000059604644775390625);

    assert_int(asdf_get_bool(file, "does-not-exist", &b), ==, ASDF_VALUE_ERR_NOT_FOUND);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_get_mapping) {
    const char *filename = get_fixture_file_path("value-types.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    assert_true(asdf_is_mapping(file, "mapping"));
    assert_false(asdf_is_mapping(file, "scalar"));
    asdf_value_t *value = NULL;
    asdf_value_err_t err = asdf_get_mapping(file, "mapping", &value);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_true(asdf_value_is_mapping(value));
    asdf_value_destroy(value);
    err = asdf_get_mapping(file, "scalar", &value);
    assert_int(err, ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_get_sequence) {
    const char *filename = get_fixture_file_path("value-types.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    assert_true(asdf_is_sequence(file, "sequence"));
    assert_false(asdf_is_sequence(file, "scalar"));
    asdf_value_t *value = NULL;
    asdf_value_err_t err = asdf_get_sequence(file, "sequence", &value);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_true(asdf_value_is_sequence(value));
    asdf_value_destroy(value);
    err = asdf_get_sequence(file, "scalar", &value);
    assert_int(err, ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_block_count) {
    const char *filename = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    assert_int(asdf_block_count(file), ==, 1);
    asdf_close(file);

    filename = get_reference_file_path("1.6.0/complex.asdf");
    file = asdf_open_file(filename, "r");
    assert_not_null(file);
    assert_int(asdf_block_count(file), ==, 4);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_file,
    MU_RUN_TEST(test_asdf_open_file),
    MU_RUN_TEST(test_asdf_scalar_getters),
    MU_RUN_TEST(test_asdf_get_mapping),
    MU_RUN_TEST(test_asdf_get_sequence),
    MU_RUN_TEST(test_asdf_block_count)
);


MU_RUN_SUITE(test_asdf_file);
