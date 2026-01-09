#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <libfyaml.h>

#include <asdf/core/asdf.h>
#include <asdf/core/ndarray.h>
#include <asdf/file.h>
#include <asdf/value.h>

#include "munit.h"
#include "util.h"


#define CHECK_VALUE_TYPE(key, expected_type) \
    do { \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        asdf_value_type_t __type = asdf_value_get_type(__value); \
        assert_int(__type, ==, (expected_type)); \
        asdf_value_destroy(__value); \
    } while (0)


MU_TEST(test_asdf_value_get_type) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    assert_int(asdf_value_get_type(NULL), ==, ASDF_VALUE_UNKNOWN);
    CHECK_VALUE_TYPE("single_quoted", ASDF_VALUE_STRING);
    CHECK_VALUE_TYPE("double_quoted", ASDF_VALUE_STRING);
    CHECK_VALUE_TYPE("plain", ASDF_VALUE_STRING);
    CHECK_VALUE_TYPE("literal", ASDF_VALUE_STRING);
    CHECK_VALUE_TYPE("folded", ASDF_VALUE_STRING);
    CHECK_VALUE_TYPE("false", ASDF_VALUE_BOOL);
    CHECK_VALUE_TYPE("False", ASDF_VALUE_BOOL);
    CHECK_VALUE_TYPE("FALSE", ASDF_VALUE_BOOL);
    // Parsed as uint8 but can be read as bool
    CHECK_VALUE_TYPE("false0", ASDF_VALUE_UINT8);
    CHECK_VALUE_TYPE("true", ASDF_VALUE_BOOL);
    CHECK_VALUE_TYPE("True", ASDF_VALUE_BOOL);
    CHECK_VALUE_TYPE("TRUE", ASDF_VALUE_BOOL);
    // Parsed as uint8 but can be read as bool
    CHECK_VALUE_TYPE("true1", ASDF_VALUE_UINT8);
    CHECK_VALUE_TYPE("null", ASDF_VALUE_NULL);
    CHECK_VALUE_TYPE("Null", ASDF_VALUE_NULL);
    CHECK_VALUE_TYPE("NULL", ASDF_VALUE_NULL);
    CHECK_VALUE_TYPE("empty", ASDF_VALUE_NULL);
    CHECK_VALUE_TYPE("int8", ASDF_VALUE_INT8);
    CHECK_VALUE_TYPE("int16", ASDF_VALUE_INT16);
    CHECK_VALUE_TYPE("int32", ASDF_VALUE_INT32);
    CHECK_VALUE_TYPE("int64", ASDF_VALUE_INT64);
    CHECK_VALUE_TYPE("uint8", ASDF_VALUE_UINT8);
    CHECK_VALUE_TYPE("uint16", ASDF_VALUE_UINT16);
    CHECK_VALUE_TYPE("uint32", ASDF_VALUE_UINT32);
    CHECK_VALUE_TYPE("uint64", ASDF_VALUE_UINT64);
    CHECK_VALUE_TYPE("bigint", ASDF_VALUE_UNKNOWN);
    CHECK_VALUE_TYPE("float32", ASDF_VALUE_DOUBLE);
    CHECK_VALUE_TYPE("float64", ASDF_VALUE_DOUBLE);
    CHECK_VALUE_TYPE("bigfloat", ASDF_VALUE_DOUBLE);
    asdf_close(file);
    return MUNIT_OK;
}


/* Helper for string conversion tests */
#define CHECK_STR_VALUE(key, expected_value) \
    do { \
        const char *__v = NULL; \
        size_t __len = 0; \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        assert_true(asdf_value_is_string(__value)); \
        asdf_value_err_t __err = asdf_value_as_string(__value, &__v, &__len); \
        assert_int(__err, ==, ASDF_VALUE_OK); \
        char *__vs = strndup(__v, __len); \
        assert_not_null(__vs); \
        assert_string_equal(__vs, (expected_value)); \
        free(__vs); \
        asdf_value_destroy(__value); \
    } while (0)


#define CHECK_STR_VALUE_MISMATCH(key) \
    do { \
        const char *__v = NULL; \
        size_t __len = 0; \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        assert_false(asdf_value_is_string(__value)); \
        asdf_value_err_t __err = asdf_value_as_string(__value, &__v, &__len); \
        assert_int(__err, ==, ASDF_VALUE_ERR_TYPE_MISMATCH); \
        asdf_value_destroy(__value); \
    } while (0)


MU_TEST(test_asdf_value_as_string) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_STR_VALUE("plain", "string");
    CHECK_STR_VALUE("single_quoted", "string");
    CHECK_STR_VALUE("double_quoted", "string");
    CHECK_STR_VALUE("literal", "literal\nstring\n");
    CHECK_STR_VALUE("folded", "folded string\n");
    CHECK_STR_VALUE_MISMATCH("null");
    CHECK_STR_VALUE_MISMATCH("false");
    CHECK_STR_VALUE_MISMATCH("true");
    CHECK_STR_VALUE_MISMATCH("empty");
    CHECK_STR_VALUE_MISMATCH("int8");
    CHECK_STR_VALUE_MISMATCH("float32");

    asdf_value_t *root = asdf_get_value(file, "");
    assert_not_null(root);

    // Test on something that is not a scalar
    const char *str = NULL;
    size_t len = 0;
    assert_int(asdf_value_as_string(root, &str, &len), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_null(str);
    asdf_value_destroy(root);
    asdf_close(file);
    return MUNIT_OK;
}


/* ``_as_string0`` is not very interesting compared to ``_as_string``
 * Should work the same except for returning the string value as a null-terminated copy
 */
MU_TEST(test_asdf_value_as_string0) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    const char *str = NULL;
    asdf_value_t *value = asdf_get_value(file, "plain");
    assert_not_null(value);
    asdf_value_err_t err = asdf_value_as_string0(value, &str);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(str);
    assert_string_equal(str, "string");
    asdf_value_destroy(value);

    str = NULL;
    value = asdf_get_value(file, "int8");
    assert_not_null(value);
    err = asdf_value_as_string0(value, &str);
    assert_int(err, ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_null(str);

    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_scalar) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    const char *s = NULL;
    size_t len = 0;
    int8_t i = 0;
    asdf_value_t *value = asdf_get_value(file, "int8");
    assert_not_null(value); \
    assert_true(asdf_value_is_scalar(value));
    asdf_value_err_t err = asdf_value_as_int8(value, &i);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_int(i, ==, -127);
    err = asdf_value_as_scalar(value, &s, &len);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(s);
    assert_int(len, ==, 4);
    assert_memory_equal(len, s, "-127");
    asdf_value_destroy(value);

    // Misc error conditions
    assert_int(asdf_value_as_scalar(NULL, NULL, NULL), ==, ASDF_VALUE_ERR_UNKNOWN);
    value = asdf_value_of_mapping(asdf_mapping_create(file));
    assert_int(asdf_value_as_scalar(value, NULL, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    asdf_value_destroy(value);
    value = asdf_value_of_sequence(asdf_sequence_create(file));
    assert_int(asdf_value_as_scalar(value, NULL, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    asdf_value_destroy(value);

    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_scalar0) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    const char *s = NULL;
    int8_t i = 0;
    asdf_value_t *value = asdf_get_value(file, "int8");
    assert_not_null(value); \
    asdf_value_err_t err = asdf_value_as_int8(value, &i);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_int(i, ==, -127);
    err = asdf_value_as_scalar0(value, &s);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(s);
    assert_string_equal(s, "-127");
    asdf_value_destroy(value);

    // Misc error conditions
    assert_int(asdf_value_as_scalar0(NULL, NULL), ==, ASDF_VALUE_ERR_UNKNOWN);
    value = asdf_value_of_mapping(asdf_mapping_create(file));
    assert_int(asdf_value_as_scalar0(value, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    asdf_value_destroy(value);
    value = asdf_value_of_sequence(asdf_sequence_create(file));
    assert_int(asdf_value_as_scalar0(value, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    asdf_value_destroy(value);

    asdf_close(file);
    return MUNIT_OK;
}


/* Helpers for bool conversion test */
#define CHECK_BOOL_VALUE(key, expected_value) \
    do { \
        bool __v = !(expected_value); \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        assert_true(asdf_value_is_bool(__value)); \
        asdf_value_err_t __err = asdf_value_as_bool(__value, &__v); \
        assert_int(__err, ==, ASDF_VALUE_OK); \
        assert_int(__v, ==, (expected_value)); \
        asdf_value_destroy(__value); \
    } while (0)


#define CHECK_BOOL_MISMATCH(key) \
    do { \
        bool __v = false; \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        assert_false(asdf_value_is_bool(__value)); \
        asdf_value_err_t __err = asdf_value_as_bool(__value, &__v); \
        assert_int(__err, ==, ASDF_VALUE_ERR_TYPE_MISMATCH); \
        asdf_value_destroy(__value); \
    } while (0)


MU_TEST(test_asdf_value_as_bool) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_BOOL_VALUE("false", false);
    CHECK_BOOL_VALUE("False", false);
    CHECK_BOOL_VALUE("FALSE", false);
    // Allow 0 to be cast to bool
    CHECK_BOOL_VALUE("false0", false);
    CHECK_BOOL_VALUE("true", true);
    CHECK_BOOL_VALUE("True", true);
    CHECK_BOOL_VALUE("TRUE", true);
    // Allow 1 to be cast to bool
    CHECK_BOOL_VALUE("true1", true);
    CHECK_BOOL_MISMATCH("int64");
    CHECK_BOOL_MISMATCH("plain");

    asdf_value_t *root = asdf_get_value(file, "");
    assert_int(asdf_value_as_bool(root, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    asdf_value_destroy(root);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_is_bool) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_value_t *int_value = asdf_value_of_int8(file, 0);
    assert_true(asdf_value_is_bool(int_value));
    asdf_value_destroy(int_value);

    int_value = asdf_value_of_int8(file, 1);
    assert_true(asdf_value_is_bool(int_value));
    asdf_value_destroy(int_value);

    int_value = asdf_value_of_int8(file, -1);
    assert_false(asdf_value_is_bool(int_value));
    asdf_value_destroy(int_value);

    asdf_value_t *root = asdf_get_value(file, "");
    assert_false(asdf_value_is_bool(root));
    asdf_value_destroy(root);
    asdf_close(file);
    return MUNIT_OK;
}


/* Helpers for null conversion tests */
#define CHECK_NULL_VALUE(key, expected) \
    do { \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        bool __v = !(expected); \
        __v = asdf_value_is_null(__value); \
        assert_int(__v, ==, (expected)); \
        asdf_value_destroy(__value); \
    } while (0)


MU_TEST(test_asdf_value_is_null) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_NULL_VALUE("null", true);
    CHECK_NULL_VALUE("Null", true);
    CHECK_NULL_VALUE("NULL", true);
    CHECK_NULL_VALUE("empty", true);
    CHECK_NULL_VALUE("plain", false);
    CHECK_NULL_VALUE("false0", false);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_is_int) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_value_t *value = asdf_get_value(file, "");
    assert_not_null(value);
    assert_false(asdf_value_is_int(value));
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


/* Helpers for int conversion tests */
#define CHECK_INT_VALUE(type, key, expected_err, expected_value) \
    do { \
        type##_t __v = 0; \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        assert_true(asdf_value_is_int(__value)); \
        asdf_value_err_t __err = asdf_value_as_##type(__value, &__v); \
        assert_int(__err, ==, (expected_err)); \
        type##_t __ve = (expected_value); \
        assert_int(__v, ==, __ve); \
        asdf_value_destroy(__value); \
    } while (0)


#define CHECK_INT_VALUE_MISMATCH(type, key) \
    do { \
        type##_t __v = 0; \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        assert_false(asdf_value_is_##type(__value)); \
        asdf_value_err_t __err = asdf_value_as_##type(__value, &__v); \
        assert_int(__err, ==, ASDF_VALUE_ERR_TYPE_MISMATCH); \
        asdf_value_destroy(__value); \
    } while (0)


#define CHECK_VALUE_IS_INT(type, key, expected) \
    do { \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        if (expected) \
            assert_true(asdf_value_is_##type(__value)); \
        else \
            assert_false(asdf_value_is_##type(__value)); \
        asdf_value_destroy(__value); \
    } while (0)


MU_TEST(test_asdf_value_as_int8) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(int8, "int8", ASDF_VALUE_OK, -127);
    CHECK_INT_VALUE(int8, "uint8", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int8, "int16", ASDF_VALUE_ERR_OVERFLOW, 1);
    CHECK_INT_VALUE(int8, "uint16", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int8, "int32", ASDF_VALUE_ERR_OVERFLOW, 1);
    CHECK_INT_VALUE(int8, "uint32", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int8, "int64", ASDF_VALUE_ERR_OVERFLOW, 1);
    CHECK_INT_VALUE(int8, "uint64", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE_MISMATCH(int8, "plain");
    CHECK_INT_VALUE_MISMATCH(int8, "");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_is_int8) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_VALUE_IS_INT(int8, "", false);
    CHECK_VALUE_IS_INT(int8, "int8", true);
    CHECK_VALUE_IS_INT(int8, "int16", false);
    CHECK_VALUE_IS_INT(int8, "uint8", false);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_int16) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(int16, "int8", ASDF_VALUE_OK, -127);
    CHECK_INT_VALUE(int16, "uint8", ASDF_VALUE_OK, 255);
    CHECK_INT_VALUE(int16, "int16", ASDF_VALUE_OK, -32767);
    CHECK_INT_VALUE(int16, "uint16", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int16, "int32", ASDF_VALUE_ERR_OVERFLOW, 1);
    CHECK_INT_VALUE(int16, "uint32", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int16, "int64", ASDF_VALUE_ERR_OVERFLOW, 1);
    CHECK_INT_VALUE(int16, "uint64", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE_MISMATCH(int16, "plain");
    CHECK_INT_VALUE_MISMATCH(int16, "");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_is_int16) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_VALUE_IS_INT(int16, "", false);
    CHECK_VALUE_IS_INT(int16, "int8", true);
    CHECK_VALUE_IS_INT(int16, "int16", true);
    CHECK_VALUE_IS_INT(int16, "int32", false);
    CHECK_VALUE_IS_INT(int16, "uint16", false);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_int32) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(int32, "int8", ASDF_VALUE_OK, -127);
    CHECK_INT_VALUE(int32, "uint8", ASDF_VALUE_OK, 255);
    CHECK_INT_VALUE(int32, "int16", ASDF_VALUE_OK, -32767);
    CHECK_INT_VALUE(int32, "uint16", ASDF_VALUE_OK, 65535);
    CHECK_INT_VALUE(int32, "int32", ASDF_VALUE_OK, -2147483647);
    CHECK_INT_VALUE(int32, "uint32", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int32, "int64", ASDF_VALUE_ERR_OVERFLOW, 1);
    CHECK_INT_VALUE(int32, "uint64", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE_MISMATCH(int32, "plain");
    CHECK_INT_VALUE_MISMATCH(int32, "");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_is_int32) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_VALUE_IS_INT(int32, "", false);
    CHECK_VALUE_IS_INT(int32, "int8", true);
    CHECK_VALUE_IS_INT(int32, "int16", true);
    CHECK_VALUE_IS_INT(int32, "int32", true);
    CHECK_VALUE_IS_INT(int32, "int64", false);
    CHECK_VALUE_IS_INT(int32, "uint32", false);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_int64) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(int64, "int64", ASDF_VALUE_OK, -9223372036854775807LL);
    CHECK_INT_VALUE(int64, "int32", ASDF_VALUE_OK, -2147483647);
    CHECK_INT_VALUE(int64, "int16", ASDF_VALUE_OK, -32767);
    CHECK_INT_VALUE(int64, "int8", ASDF_VALUE_OK, -127);
    CHECK_INT_VALUE(int64, "uint64", ASDF_VALUE_ERR_OVERFLOW, -1);
    CHECK_INT_VALUE(int64, "uint32", ASDF_VALUE_OK, 4294967295);
    CHECK_INT_VALUE(int64, "uint16", ASDF_VALUE_OK, 65535);
    CHECK_INT_VALUE(int64, "uint8", ASDF_VALUE_OK, 255);
    CHECK_INT_VALUE_MISMATCH(int64, "plain");
    CHECK_INT_VALUE_MISMATCH(int64, "");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_is_int64) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_VALUE_IS_INT(int64, "", false);
    CHECK_VALUE_IS_INT(int64, "int8", true);
    CHECK_VALUE_IS_INT(int64, "int16", true);
    CHECK_VALUE_IS_INT(int64, "int32", true);
    CHECK_VALUE_IS_INT(int64, "int64", true);
    CHECK_VALUE_IS_INT(int64, "uint8", true);
    CHECK_VALUE_IS_INT(int64, "uint16", true);
    CHECK_VALUE_IS_INT(int64, "uint32", true);
    CHECK_VALUE_IS_INT(int64, "uint64", false);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_uint8) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(uint8, "int8", ASDF_VALUE_ERR_OVERFLOW, -127);
    CHECK_INT_VALUE(uint8, "uint8", ASDF_VALUE_OK, 255);
    CHECK_INT_VALUE(uint8, "int16", ASDF_VALUE_ERR_OVERFLOW, 1);
    CHECK_INT_VALUE(uint8, "uint16", ASDF_VALUE_ERR_OVERFLOW, 255);
    CHECK_INT_VALUE(uint8, "int32", ASDF_VALUE_ERR_OVERFLOW, 1);
    CHECK_INT_VALUE(uint8, "uint32", ASDF_VALUE_ERR_OVERFLOW, 255);
    CHECK_INT_VALUE(uint8, "int64", ASDF_VALUE_ERR_OVERFLOW, 1);
    CHECK_INT_VALUE(uint8, "uint64", ASDF_VALUE_ERR_OVERFLOW, 255);
    CHECK_INT_VALUE_MISMATCH(uint8, "plain");
    CHECK_INT_VALUE_MISMATCH(uint8, "");
    asdf_value_t *int_value = asdf_value_of_uint16(file, 1);
    uint8_t u8 = 0;
    assert_int(asdf_value_as_uint8(int_value, &u8), ==, ASDF_VALUE_OK);
    assert_int(u8, ==, 1);
    asdf_value_destroy(int_value);

    u8 = 0;
    int_value = asdf_value_of_int8(file, 1);
    assert_int(asdf_value_as_uint8(int_value, &u8), ==, ASDF_VALUE_OK);
    assert_int(u8, ==, 1);
    asdf_value_destroy(int_value);

    u8 = 0;
    int_value = asdf_value_of_int16(file, 1);
    assert_int(asdf_value_as_uint8(int_value, &u8), ==, ASDF_VALUE_OK);
    assert_int(u8, ==, 1);
    asdf_value_destroy(int_value);

    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_is_uint8) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_VALUE_IS_INT(uint8, "", false);
    CHECK_VALUE_IS_INT(uint8, "int8", false);
    CHECK_VALUE_IS_INT(uint8, "uint8", true);
    CHECK_VALUE_IS_INT(uint8, "int16", false);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_uint16) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(uint16, "int8", ASDF_VALUE_ERR_OVERFLOW, -127);
    CHECK_INT_VALUE(uint16, "uint8", ASDF_VALUE_OK, 255);
    CHECK_INT_VALUE(uint16, "int16", ASDF_VALUE_ERR_OVERFLOW, -32767);
    CHECK_INT_VALUE(uint16, "uint16", ASDF_VALUE_OK, 65535);
    CHECK_INT_VALUE(uint16, "int32", ASDF_VALUE_ERR_OVERFLOW, 1);
    CHECK_INT_VALUE(uint16, "uint32", ASDF_VALUE_ERR_OVERFLOW, 65535);
    CHECK_INT_VALUE(uint16, "int64", ASDF_VALUE_ERR_OVERFLOW, 1);
    CHECK_INT_VALUE(uint16, "uint64", ASDF_VALUE_ERR_OVERFLOW, 65535);
    CHECK_INT_VALUE_MISMATCH(uint16, "plain");
    CHECK_INT_VALUE_MISMATCH(uint16, "");

    asdf_value_t *int_value = asdf_value_of_uint32(file, 1);
    uint16_t u16 = 0;
    assert_int(asdf_value_as_uint16(int_value, &u16), ==, ASDF_VALUE_OK);
    assert_int(u16, ==, 1);
    asdf_value_destroy(int_value);

    u16 = 0;
    int_value = asdf_value_of_int8(file, 1);
    assert_int(asdf_value_as_uint16(int_value, &u16), ==, ASDF_VALUE_OK);
    assert_int(u16, ==, 1);
    asdf_value_destroy(int_value);

    u16 = 0;
    int_value = asdf_value_of_int32(file, 1);
    assert_int(asdf_value_as_uint16(int_value, &u16), ==, ASDF_VALUE_OK);
    assert_int(u16, ==, 1);
    asdf_value_destroy(int_value);

    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_is_uint16) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_VALUE_IS_INT(uint16, "", false);
    CHECK_VALUE_IS_INT(uint16, "int8", false);
    CHECK_VALUE_IS_INT(uint16, "uint8", true);
    CHECK_VALUE_IS_INT(uint16, "int16", false);
    CHECK_VALUE_IS_INT(uint16, "uint32", false);
    CHECK_VALUE_IS_INT(uint16, "int32", false);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_uint32) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(uint32, "int8", ASDF_VALUE_ERR_OVERFLOW, -127);
    CHECK_INT_VALUE(uint32, "uint8", ASDF_VALUE_OK, 255);
    CHECK_INT_VALUE(uint32, "int16", ASDF_VALUE_ERR_OVERFLOW, -32767);
    CHECK_INT_VALUE(uint32, "uint16", ASDF_VALUE_OK, 65535);
    CHECK_INT_VALUE(uint32, "int32", ASDF_VALUE_ERR_OVERFLOW, -2147483647);
    CHECK_INT_VALUE(uint32, "uint32", ASDF_VALUE_OK, 4294967295);
    CHECK_INT_VALUE(uint32, "int64", ASDF_VALUE_ERR_OVERFLOW, 1);
    CHECK_INT_VALUE(uint32, "uint64", ASDF_VALUE_ERR_OVERFLOW, 4294967295);
    CHECK_INT_VALUE_MISMATCH(uint32, "plain");
    CHECK_INT_VALUE_MISMATCH(uint32, "");

    asdf_value_t *int_value = asdf_value_of_uint64(file, 1);
    uint32_t u32 = 0;
    assert_int(asdf_value_as_uint32(int_value, &u32), ==, ASDF_VALUE_OK);
    assert_int(u32, ==, 1);
    asdf_value_destroy(int_value);

    u32 = 0;
    int_value = asdf_value_of_int8(file, 1);
    assert_int(asdf_value_as_uint32(int_value, &u32), ==, ASDF_VALUE_OK);
    assert_int(u32, ==, 1);
    asdf_value_destroy(int_value);

    u32 = 0;
    int_value = asdf_value_of_int32(file, 1);
    assert_int(asdf_value_as_uint32(int_value, &u32), ==, ASDF_VALUE_OK);
    assert_int(u32, ==, 1);
    asdf_value_destroy(int_value);

    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_is_uint32) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_VALUE_IS_INT(uint32, "", false);
    CHECK_VALUE_IS_INT(uint32, "int8", false);
    CHECK_VALUE_IS_INT(uint32, "uint8", true);
    CHECK_VALUE_IS_INT(uint32, "int16", false);
    CHECK_VALUE_IS_INT(uint32, "uint32", true);
    CHECK_VALUE_IS_INT(uint32, "int32", false);
    CHECK_VALUE_IS_INT(uint32, "uint64", false);
    CHECK_VALUE_IS_INT(uint32, "int64", false);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_uint64) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(uint64, "int8", ASDF_VALUE_ERR_OVERFLOW, -127);
    CHECK_INT_VALUE(uint64, "uint8", ASDF_VALUE_OK, 255);
    CHECK_INT_VALUE(uint64, "int16", ASDF_VALUE_ERR_OVERFLOW, -32767);
    CHECK_INT_VALUE(uint64, "uint16", ASDF_VALUE_OK, 65535);
    CHECK_INT_VALUE(uint64, "int32", ASDF_VALUE_ERR_OVERFLOW, -2147483647);
    CHECK_INT_VALUE(uint64, "uint32", ASDF_VALUE_OK, 4294967295);
    CHECK_INT_VALUE(uint64, "int64", ASDF_VALUE_ERR_OVERFLOW, -9223372036854775807LL);
    CHECK_INT_VALUE(uint64, "uint64", ASDF_VALUE_OK, 18446744073709551615ULL);
    CHECK_INT_VALUE_MISMATCH(uint64, "plain");
    CHECK_INT_VALUE_MISMATCH(uint64, "");

    asdf_value_t *int_value = asdf_value_of_int8(file, 1);
    uint64_t u64 = 0;
    assert_int(asdf_value_as_uint64(int_value, &u64), ==, ASDF_VALUE_OK);
    assert_int(u64, ==, 1);
    asdf_value_destroy(int_value);

    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_is_uint64) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_VALUE_IS_INT(uint64, "", false);
    CHECK_VALUE_IS_INT(uint64, "int8", false);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_uint64_on_bigint) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_value_t *value = asdf_get_value(file, "bigint");
    assert_not_null(value);
    uint64_t v = 0;
    asdf_value_err_t err = asdf_value_as_uint64(value, &v);
    assert_int(err, ==, ASDF_VALUE_ERR_OVERFLOW);
    assert_false(asdf_value_is_int(value));
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


#define CHECK_FLOAT_VALUE(type, key, expected_err, expected_value) \
    do { \
        type __v = 0; \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        asdf_value_err_t __err = asdf_value_as_##type(__value, &__v); \
        assert_int(__err, ==, (expected_err)); \
        type __ve = (expected_value); \
        assert_##type(__v, ==, __ve); \
        asdf_value_destroy(__value); \
    } while (0)


#define CHECK_FLOAT_VALUE_MISMATCH(type, key) \
    do { \
        type __v = 0; \
        asdf_value_t *__value = asdf_get_value(file, (key)); \
        assert_not_null(__value); \
        assert_false(asdf_value_is_##type(__value)); \
        asdf_value_err_t __err = asdf_value_as_##type(__value, &__v); \
        assert_##type(__err, ==, ASDF_VALUE_ERR_TYPE_MISMATCH); \
        asdf_value_destroy(__value); \
    } while (0)


MU_TEST(test_asdf_value_as_float) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_FLOAT_VALUE(float, "float32", ASDF_VALUE_OK, 0.15625);
    CHECK_FLOAT_VALUE(float, "float64", ASDF_VALUE_OK, 1.000000059604644775390625);
    CHECK_FLOAT_VALUE_MISMATCH(float, "plain");
    CHECK_FLOAT_VALUE_MISMATCH(float, "");
    asdf_value_t *float_val = asdf_value_of_float(file, 1.0F);
    float out = 0.0F;
    assert_int(asdf_value_as_float(float_val, &out), ==, ASDF_VALUE_OK);
    assert_float(out, ==, 1.0F);
    asdf_value_destroy(float_val);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_is_float) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    assert_false(asdf_is_float(file, ""));
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_double) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_FLOAT_VALUE(double, "float32", ASDF_VALUE_OK, 0.15625);
    CHECK_FLOAT_VALUE(double, "float64", ASDF_VALUE_OK, 1.000000059604644775390625);
    CHECK_FLOAT_VALUE_MISMATCH(double, "plain");
    CHECK_FLOAT_VALUE_MISMATCH(double, "");

    asdf_value_t *float_val = asdf_value_of_float(file, 1.0F);
    double out = 0.0;
    assert_int(asdf_value_as_double(float_val, &out), ==, ASDF_VALUE_OK);
    assert_double(out, ==, 1.0);
    asdf_value_destroy(float_val);

    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_type) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    asdf_value_t *value = asdf_get_value(file, "plain");
    asdf_value_t *clone = NULL;
    const char *str = NULL;
    assert_int(asdf_value_as_type(value, ASDF_VALUE_UNKNOWN, (void *)&clone), ==, ASDF_VALUE_OK);
    assert_not_null(clone);
    asdf_value_destroy(clone);
    assert_int(asdf_value_as_type(value, ASDF_VALUE_SEQUENCE, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_int(asdf_value_as_type(value, ASDF_VALUE_MAPPING, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_int(asdf_value_as_type(value, ASDF_VALUE_SCALAR, (void *)&str), ==, ASDF_VALUE_OK);
    assert_string_equal(str, "string");
    str = NULL;
    assert_int(asdf_value_as_type(value, ASDF_VALUE_STRING, (void *)&str), ==, ASDF_VALUE_OK);
    assert_string_equal(str, "string");
    str = NULL;
    assert_int(asdf_value_as_type(value, ASDF_VALUE_BOOL, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_int(asdf_value_as_type(value, ASDF_VALUE_NULL, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_int(asdf_value_as_type(value, ASDF_VALUE_INT8, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_int(asdf_value_as_type(value, ASDF_VALUE_INT16, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_int(asdf_value_as_type(value, ASDF_VALUE_INT32, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_int(asdf_value_as_type(value, ASDF_VALUE_INT64, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_int(asdf_value_as_type(value, ASDF_VALUE_UINT8, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_int(asdf_value_as_type(value, ASDF_VALUE_UINT16, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_int(asdf_value_as_type(value, ASDF_VALUE_UINT32, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_int(asdf_value_as_type(value, ASDF_VALUE_UINT64, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_int(asdf_value_as_type(value, ASDF_VALUE_FLOAT, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_int(asdf_value_as_type(value, ASDF_VALUE_DOUBLE, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_int(asdf_value_as_type(value, ASDF_VALUE_EXTENSION, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    assert_int(asdf_value_as_type(value, -2, NULL), ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    asdf_value_destroy(value);

    // Special case of null
    value = asdf_get_value(file, "null");
    assert_int(asdf_value_as_type(value, ASDF_VALUE_NULL, NULL), ==, ASDF_VALUE_OK);
    asdf_value_destroy(value);

    asdf_close(file);
    return MUNIT_OK;
}



MU_TEST(test_asdf_value_is_type) {
    const char *path = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    asdf_value_t *value = asdf_get_value(file, "plain");
    assert_false(asdf_value_is_type(value, -2));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_UNKNOWN));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_SEQUENCE));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_MAPPING));
    assert_true(asdf_value_is_type(value, ASDF_VALUE_SCALAR));
    assert_true(asdf_value_is_type(value, ASDF_VALUE_STRING));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_BOOL));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_NULL));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_INT8));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_INT16));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_INT32));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_INT64));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_UINT8));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_UINT16));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_UINT32));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_UINT64));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_FLOAT));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_DOUBLE));
    assert_false(asdf_value_is_type(value, ASDF_VALUE_EXTENSION));
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_of_mapping) {
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open_file(path, "w");
    assert_not_null(file);
    asdf_mapping_t *mapping = asdf_mapping_create(file);
    assert_not_null(mapping);
    asdf_value_t *value = asdf_value_of_mapping(mapping);
    assert_int(asdf_set_value(file, "mapping", value), ==, ASDF_VALUE_OK);
    asdf_close(file);
    file = asdf_open(path, "r");
    mapping = NULL;
    asdf_get_mapping(file, "mapping", &mapping);
    assert_not_null(mapping);
    assert_int(asdf_mapping_size(mapping), ==, 0);
    asdf_mapping_destroy(mapping);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_of_sequence) {
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open_file(path, "w");
    assert_not_null(file);
    asdf_sequence_t *sequence = asdf_sequence_create(file);
    assert_not_null(sequence);
    asdf_value_t *value = asdf_value_of_sequence(sequence);
    assert_int(asdf_set_value(file, "sequence", value), ==, ASDF_VALUE_OK);
    asdf_close(file);
    file = asdf_open(path, "r");
    sequence = NULL;
    asdf_get_sequence(file, "sequence", &sequence);
    assert_not_null(sequence);
    assert_int(asdf_sequence_size(sequence), ==, 0);
    asdf_sequence_destroy(sequence);
    asdf_close(file);
    return MUNIT_OK;
}


#define CHECK_SET_VALUE_OF_TYPE(type, val) do { \
    value = asdf_value_of_##type(file, (val)); \
    assert_not_null(value); \
    assert_int(asdf_set_value(file, #type, value), ==, ASDF_VALUE_OK); \
} while (0)


MU_TEST(test_asdf_value_of_type) {
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open_file(path, "w");
    assert_not_null(file);

    asdf_value_t *value = asdf_value_of_string(file, "string", 6);
    assert_not_null(value);
    assert_int(asdf_set_value(file, "string", value), ==, ASDF_VALUE_OK);

    CHECK_SET_VALUE_OF_TYPE(string0, "string0");

    value = asdf_value_of_null(file);
    assert_not_null(value);
    assert_int(asdf_set_value(file, "null", value), ==, ASDF_VALUE_OK);

    value = asdf_value_of_bool(file, false);
    assert_not_null(value);
    assert_int(asdf_set_value(file, "false", value), ==, ASDF_VALUE_OK);

    value = asdf_value_of_bool(file, true);
    assert_not_null(value);
    assert_int(asdf_set_value(file, "true", value), ==, ASDF_VALUE_OK);

    CHECK_SET_VALUE_OF_TYPE(int8, INT8_MIN);
    CHECK_SET_VALUE_OF_TYPE(int16, INT16_MIN);
    CHECK_SET_VALUE_OF_TYPE(int32, INT32_MIN);
    CHECK_SET_VALUE_OF_TYPE(int64, INT64_MIN);
    CHECK_SET_VALUE_OF_TYPE(uint8, UINT8_MAX);
    CHECK_SET_VALUE_OF_TYPE(uint16, UINT16_MAX);
    CHECK_SET_VALUE_OF_TYPE(uint32, UINT32_MAX);
    CHECK_SET_VALUE_OF_TYPE(uint64, UINT64_MAX);
    CHECK_SET_VALUE_OF_TYPE(float, FLT_MAX);
    CHECK_SET_VALUE_OF_TYPE(double, DBL_MAX);
    asdf_close(file);

    const char *fixture_path = get_fixture_file_path("scalars-out.asdf");
    assert_true(compare_files(path, fixture_path));
    return MUNIT_OK;
}


/**
 * Test that scalars explicitly tagged as !!str as interpreted as strings
 */
MU_TEST(test_value_tagged_strings) {
    const char *path = get_fixture_file_path("tagged-scalars.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_STR_VALUE("string", "string");
    CHECK_STR_VALUE("bool_string", "true");
    CHECK_STR_VALUE("null_string", "null");
    CHECK_STR_VALUE("int_string", "1");
    CHECK_STR_VALUE("float_string", "1.0");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_mapping_create) {
    // TODO: For this test change it to just an in-memory file; we won't write anything anyways
    // But we need an open file with which to associate the mapping
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(path, "w");
    assert_not_null(file);
    asdf_mapping_t *mapping = asdf_mapping_create(file);
    assert_not_null(mapping);
    asdf_mapping_destroy(mapping);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_mapping_item_destroy) {
    asdf_mapping_item_destroy(NULL); // OK
    return MUNIT_OK;
}


MU_TEST(test_asdf_mapping_iter) {
    const char *path = get_fixture_file_path("value-types.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_mapping_t *mapping = NULL;
    asdf_value_err_t err = asdf_get_mapping(file, "mapping", &mapping);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_size(mapping), ==, 2);
    asdf_mapping_iter_t iter = asdf_mapping_iter_init();

    asdf_mapping_item_t *item = asdf_mapping_iter(mapping, &iter);
    assert_not_null(item);
    assert_not_null(asdf_mapping_item_key(item));
    assert_string_equal(asdf_mapping_item_key(item), "foo");
    asdf_value_t *value = asdf_mapping_item_value(item);
    assert_not_null(value);
    const char *s = NULL;
    assert_int(asdf_value_as_string0(value, &s), ==, ASDF_VALUE_OK);
    assert_string_equal(s, "foo");

    item = asdf_mapping_iter(mapping, &iter);
    assert_not_null(item);
    assert_not_null(asdf_mapping_item_key(item));
    assert_string_equal(asdf_mapping_item_key(item), "bar");
    value = asdf_mapping_item_value(item);
    assert_not_null(value);
    s = NULL;
    assert_int(asdf_value_as_string0(value, &s), ==, ASDF_VALUE_OK);
    assert_string_equal(s, "bar");

    assert_null(asdf_mapping_iter(mapping, &iter));
    assert_null((void *)iter);
    asdf_mapping_destroy(mapping);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_mapping_get) {
    const char *path = get_fixture_file_path("value-types.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_mapping_t *mapping = NULL;
    asdf_value_err_t err = asdf_get_mapping(file, "mapping", &mapping);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(mapping);
    asdf_value_t *foo = asdf_mapping_get(mapping, "foo");
    assert_not_null(foo);
    assert_true(asdf_value_is_string(foo));
    const char *s = NULL;
    assert_int(asdf_value_as_string0(foo, &s), ==, ASDF_VALUE_OK);
    assert_not_null(s);
    assert_string_equal(s, "foo");
    asdf_value_destroy(foo);

    asdf_value_t *bar = asdf_mapping_get(mapping, "bar");
    assert_not_null(bar);
    assert_true(asdf_value_is_string(bar));
    assert_int(asdf_value_as_string0(bar, &s), ==, ASDF_VALUE_OK);
    assert_not_null(s);
    assert_string_equal(s, "bar");
    asdf_value_destroy(bar);

    asdf_value_t *null = asdf_mapping_get(mapping, "does-not-exist");
    assert_null(null);

    asdf_mapping_destroy(mapping);
    asdf_close(file);
    return MUNIT_OK;
}


/** Test basic mapping setters */
// TODO: Change to use in-memory file
MU_TEST(test_asdf_mapping_set_scalars) {
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open_file(path, "w");
    assert_not_null(file);
    asdf_mapping_t *mapping = asdf_mapping_create(file);
    assert_not_null(mapping);
    assert_int(asdf_mapping_set_string(mapping, "string", "string", 6), ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_set_string0(mapping, "string0", "string0"), ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_set_null(mapping, "null"), ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_set_bool(mapping, "false", false), ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_set_bool(mapping, "true", true), ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_set_int8(mapping, "int8", INT8_MIN), ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_set_int16(mapping, "int16", INT16_MIN), ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_set_int32(mapping, "int32", INT32_MIN), ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_set_int64(mapping, "int64", INT64_MIN), ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_set_uint8(mapping, "uint8", UINT8_MAX), ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_set_uint16(mapping, "uint16", UINT16_MAX), ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_set_uint32(mapping, "uint32", UINT32_MAX), ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_set_uint64(mapping, "uint64", UINT64_MAX), ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_set_float(mapping, "float", FLT_MAX), ==, ASDF_VALUE_OK);
    assert_int(asdf_mapping_set_double(mapping, "double", DBL_MAX), ==, ASDF_VALUE_OK);

    // Assign the mapping as the root
    assert_int(asdf_set_mapping(file, "", mapping), ==, ASDF_VALUE_OK);
    asdf_close(file);

    const char *fixture_path = get_fixture_file_path("scalars-out.asdf");
    assert_true(compare_files(path, fixture_path));
    return MUNIT_OK;
}


#define CHECK_ASDF_SEQUENCE_GET_INT_TYPE(idx, type, expected) do { \
    value = asdf_sequence_get(sequence, (idx)); \
    assert_not_null(value); \
    type##_t _val = 0; \
    assert_int(asdf_value_as_##type(value, &_val), ==, ASDF_VALUE_OK); \
    assert_##type(_val, ==, (expected)); \
    asdf_value_destroy(value); \
} while (0)


MU_TEST(test_asdf_sequence_append) {
    // TODO: For this test change it to just an in-memory file; we won't write anything anyways
    // But we need an open file with which to associate the sequence
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(path, "w");
    assert_not_null(file);
    asdf_sequence_t *sequence = asdf_sequence_create(file);
    assert_not_null(sequence);
    assert_int(asdf_sequence_append_string(sequence, "string", 6), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_append_string(NULL, "string", 6), ==, ASDF_VALUE_ERR_UNKNOWN);
    assert_int(asdf_sequence_append_string0(sequence, "string0"), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_append_string0(NULL, "string0"), ==, ASDF_VALUE_ERR_UNKNOWN);
    assert_int(asdf_sequence_append_bool(sequence, false), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_append_bool(sequence, true), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_append_null(NULL), ==, ASDF_VALUE_ERR_UNKNOWN);
    assert_int(asdf_sequence_append_null(sequence), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_append_int8(sequence, INT8_MIN), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_append_int16(sequence, INT16_MIN), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_append_int32(sequence, INT32_MIN), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_append_int64(sequence, INT64_MIN), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_append_uint8(sequence, UINT8_MAX), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_append_uint16(sequence, UINT16_MAX), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_append_uint32(sequence, UINT32_MAX), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_append_uint64(sequence, UINT64_MAX), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_append_float(sequence, FLT_MAX), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_append_double(sequence, DBL_MAX), ==, ASDF_VALUE_OK);

    asdf_sequence_t *sequence_val = asdf_sequence_create(file);
    assert_not_null(sequence_val);
    asdf_mapping_t *mapping_val = asdf_mapping_create(file);
    assert_not_null(mapping_val);
    assert_int(asdf_sequence_append_sequence(sequence, sequence_val), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_append_mapping(sequence, mapping_val), ==, ASDF_VALUE_OK);

    assert_int(asdf_set_sequence(file, "sequence", sequence), ==, ASDF_VALUE_OK);
    asdf_close(file);

    file = asdf_open(path, "r");
    sequence = NULL;
    assert_not_null(file);
    assert_int(asdf_get_sequence(file, "sequence", &sequence), ==, ASDF_VALUE_OK);
    assert_not_null(sequence);
    assert_int(asdf_sequence_size(sequence), ==, 17);

    asdf_value_t *value = asdf_sequence_get(sequence, 0);
    assert_not_null(value);
    const char *str = NULL;
    size_t len = 0;
    assert_int(asdf_value_as_string(value, &str, &len), ==, ASDF_VALUE_OK);
    assert_memory_equal(strlen("string"), str, "string");
    asdf_value_destroy(value);

    value = asdf_sequence_get(sequence, 1);
    assert_not_null(value);
    assert_int(asdf_value_as_string0(value, &str), ==, ASDF_VALUE_OK);
    assert_string_equal(str, "string0");
    asdf_value_destroy(value);

    bool bool_val = true;
    value = asdf_sequence_get(sequence, 2);
    assert_not_null(value);
    assert_int(asdf_value_as_bool(value, &bool_val), ==, ASDF_VALUE_OK);
    assert_false(bool_val);
    asdf_value_destroy(value);

    value = asdf_sequence_get(sequence, 3);
    assert_not_null(value);
    assert_int(asdf_value_as_bool(value, &bool_val), ==, ASDF_VALUE_OK);
    assert_true(bool_val);
    asdf_value_destroy(value);

    value = asdf_sequence_get(sequence, 4);
    assert_true(asdf_value_is_null(value));
    asdf_value_destroy(value);

    CHECK_ASDF_SEQUENCE_GET_INT_TYPE(5, int8, INT8_MIN);
    CHECK_ASDF_SEQUENCE_GET_INT_TYPE(6, int16, INT16_MIN);
    CHECK_ASDF_SEQUENCE_GET_INT_TYPE(7, int32, INT32_MIN);
    CHECK_ASDF_SEQUENCE_GET_INT_TYPE(8, int64, INT64_MIN);
    CHECK_ASDF_SEQUENCE_GET_INT_TYPE(9, uint8, UINT8_MAX);
    CHECK_ASDF_SEQUENCE_GET_INT_TYPE(10, uint16, UINT16_MAX);
    CHECK_ASDF_SEQUENCE_GET_INT_TYPE(11, uint32, UINT32_MAX);
    CHECK_ASDF_SEQUENCE_GET_INT_TYPE(12, uint64, UINT64_MAX);

    float float_val = 0.0F;
    value = asdf_sequence_get(sequence, 13);
    assert_not_null(value);
    assert_int(asdf_value_as_float(value, &float_val), ==, ASDF_VALUE_OK);
    assert_double_equal(float_val, FLT_MAX, 9);
    asdf_value_destroy(value);

    double double_val = 0.0;
    value = asdf_sequence_get(sequence, 14);
    assert_not_null(value);
    assert_int(asdf_value_as_double(value, &double_val), ==, ASDF_VALUE_OK);
    assert_double_equal(double_val, DBL_MAX, 9);
    asdf_value_destroy(value);

    sequence_val = NULL;
    value = asdf_sequence_get(sequence, 15);
    assert_int(asdf_value_as_sequence(value, &sequence_val), ==, ASDF_VALUE_OK);
    assert_not_null(sequence_val);
    assert_int(asdf_sequence_size(sequence_val), ==, 0);
    asdf_sequence_destroy(sequence_val);

    mapping_val = NULL;
    value = asdf_sequence_get(sequence, 16);
    assert_int(asdf_value_as_mapping(value, &mapping_val), ==, ASDF_VALUE_OK);
    assert_not_null(mapping_val);
    assert_int(asdf_mapping_size(mapping_val), ==, 0);
    asdf_mapping_destroy(mapping_val);

    asdf_sequence_destroy(sequence);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_sequence_create) {
    // TODO: For this test change it to just an in-memory file; we won't write anything anyways
    // But we need an open file with which to associate the sequence
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(path, "w");
    assert_not_null(file);
    asdf_sequence_t *sequence = asdf_sequence_create(file);
    assert_not_null(sequence);
    asdf_sequence_destroy(sequence);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_sequence_iter) {
    const char *path = get_fixture_file_path("value-types.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_sequence_t *sequence = NULL;
    asdf_value_err_t err = asdf_get_sequence(file, "sequence", &sequence);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_size(sequence), ==, 2);
    asdf_sequence_iter_t iter = asdf_sequence_iter_init();

    asdf_value_t *item = asdf_sequence_iter(sequence, &iter);
    assert_not_null(item);
    int8_t i8 = -1;
    assert_int(asdf_value_as_int8(item, &i8), ==, ASDF_VALUE_OK);
    assert_int(i8, ==, 0);

    item = asdf_sequence_iter(sequence, &iter);
    assert_not_null(item);
    assert_int(asdf_value_as_int8(item, &i8), ==, ASDF_VALUE_OK);
    assert_int(i8, ==, 1);

    assert_null(asdf_sequence_iter(sequence, &iter));
    assert_null((void *)iter);
    asdf_sequence_destroy(sequence);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_sequence_get) {
    const char *path = get_fixture_file_path("value-types.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_sequence_t *sequence = NULL;
    asdf_value_err_t err = asdf_get_sequence(file, "sequence", &sequence);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(sequence);

    asdf_value_t *first = asdf_sequence_get(sequence, 0);
    assert_not_null(first);
    assert_true(asdf_value_is_int(first));
    int8_t i8 = -1;
    assert_int(asdf_value_as_int8(first, &i8), ==, ASDF_VALUE_OK);
    assert_int(i8, ==, 0);
    asdf_value_destroy(first);

    asdf_value_t *second = asdf_sequence_get(sequence, 1);
    assert_not_null(second);
    assert_true(asdf_value_is_int(second));
    assert_int(asdf_value_as_int8(second, &i8), ==, ASDF_VALUE_OK);
    assert_int(i8, ==, 1);
    asdf_value_destroy(second);

    /* TODO: When memory allocation is improved for asdf_value_t, it might be that this returns
     * a pointer to the same asdf_value_t at the same address as the previous lookup; TBD */
    asdf_value_t *last = asdf_sequence_get(sequence, -1);
    assert_not_null(last);
    assert_true(asdf_value_is_int(last));
    assert_int(asdf_value_as_int8(last, &i8), ==, ASDF_VALUE_OK);
    assert_int(i8, ==, 1);
    asdf_value_destroy(last);

    asdf_value_t *third = asdf_sequence_get(sequence, 2);
    assert_null(third);

    asdf_sequence_destroy(sequence);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_container_iter) {
    const char *path = get_fixture_file_path("value-types.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_value_t *container = asdf_get_value(file, "sequence");
    assert_not_null(container);

    asdf_container_iter_t iter = asdf_container_iter_init();
    asdf_container_item_t *item = NULL;
    int8_t i8 = -1;

    for (int idx = 0; idx < 2; idx++) {
        item = asdf_container_iter(container, &iter);
        assert_not_null(item);
        assert_int(asdf_container_item_index(item), ==, idx);
        assert_int(asdf_value_as_int8(asdf_container_item_value(item), &i8), ==, ASDF_VALUE_OK);
        assert_int(i8, ==, idx);
    }

    assert_null(asdf_container_iter(container, &iter));
    asdf_value_destroy(container);

    // Test on a mapping now
    container = asdf_get_value(file, "mapping");
    assert_not_null(container);
    const char *s = NULL;
    const char *expected[] = {"foo", "bar"};
    for (int idx = 0; idx < 2; idx++) {
        item = asdf_container_iter(container, &iter);
        assert_not_null(item);
        assert_string_equal(asdf_container_item_key(item), expected[idx]);
        assert_int(asdf_value_as_string0(asdf_container_item_value(item), &s), ==, ASDF_VALUE_OK);
        assert_string_equal(s, expected[idx]);
    }
    assert_null(asdf_container_iter(container, &iter));

    asdf_value_destroy(container);
    asdf_close(file);
    return MUNIT_OK;
}


/** Regression test for :issue:`69` */
MU_TEST(test_value_copy_with_parent_path) {
    const char *filename = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    asdf_value_t *value = asdf_get_value(file, "/history/extensions/0");
    assert_not_null(value);
    assert_true(asdf_value_is_mapping(value));

    asdf_mapping_t *map = (asdf_mapping_t *)value;
    asdf_value_t *ext_uri = asdf_mapping_get(map, "extension_uri");
    assert_not_null(ext_uri);

    // The test: Cloned values should still retain their original path, as should any
    // child values retrieved from them.
    asdf_value_t *value_clone = asdf_value_clone(value);
    assert_string_equal(asdf_value_path(value_clone), "/history/extensions/0");
    asdf_mapping_t *value_clone_map = NULL;
    asdf_value_as_mapping(value_clone, &value_clone_map);
    assert_not_null(value_clone_map);
    asdf_value_t *ext_uri_clone = asdf_mapping_get(value_clone_map, "extension_uri");
    assert_string_equal(asdf_value_path(ext_uri_clone), "/history/extensions/0/extension_uri");

    // Crucially, it has the full path; this is just the control case
    assert_string_equal(asdf_value_path(ext_uri), "/history/extensions/0/extension_uri");

    asdf_value_destroy(ext_uri_clone);
    asdf_value_destroy(value_clone);
    asdf_value_destroy(ext_uri);
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_file) {
    const char *filename = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    asdf_value_t *value = asdf_get_value(file, "/history/extensions/0");
    assert_not_null(value);
    assert_ptr_equal(asdf_value_file(value), file);
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


static bool value_find_pred_a(asdf_value_t *value) {
    const char *str = NULL;
    asdf_value_err_t err = asdf_value_as_string0(value, &str);

    if (ASDF_VALUE_OK != err)
        return false;

    return strcmp(str, "a") == 0;
}


static bool value_find_pred_b(asdf_value_t *value) {
    const char *str = NULL;
    asdf_value_err_t err = asdf_value_as_string0(value, &str);

    if (ASDF_VALUE_OK != err)
        return false;

    return strcmp(str, "b") == 0;
}


MU_TEST(test_asdf_value_find_iter_ex_descend_mapping_only) {
    const char *filename = get_fixture_file_path("nested.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    asdf_value_t *root = asdf_get_value(file, "/");
    assert_not_null(root);
    assert_true(asdf_value_is_mapping(root));

    asdf_find_iter_t iter = asdf_find_iter_init_ex(true, asdf_find_descend_mapping_only, 1);
    asdf_find_item_t *item = NULL;

    item = asdf_value_find_iter(root, value_find_pred_a, &iter);
    assert_not_null(item);
    const char *str = NULL;
    const char *path = asdf_find_item_path(item);
    assert_string_equal(path, "/a");
    asdf_value_t *val = asdf_find_item_value(item);
    asdf_value_err_t err = asdf_value_as_string0(val, &str);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_string_equal(str, "a");

    item = asdf_value_find_iter(root, value_find_pred_a, &iter);
    assert_not_null(item);
    path = asdf_find_item_path(item);
    assert_string_equal(path, "/c/a");
    val = asdf_find_item_value(item);
    err = asdf_value_as_string0(val, &str);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_string_equal(str, "a");

    item = asdf_value_find_iter(root, value_find_pred_a, &iter);
    // Found all values matching the predicate
    assert_null(item);

    asdf_value_destroy(root);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_find_iter_ex_descend_sequence_only) {
    const char *filename = get_fixture_file_path("nested.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    asdf_value_t *root = asdf_get_value(file, "/");
    assert_not_null(root);
    assert_true(asdf_value_is_mapping(root));

    asdf_find_iter_t iter = asdf_find_iter_init_ex(true, asdf_find_descend_sequence_only, 1);
    asdf_find_item_t *item = NULL;
    
    item = asdf_value_find_iter(root, value_find_pred_a, &iter);
    assert_not_null(item);
    const char *str = NULL;
    const char *path = asdf_find_item_path(item);
    assert_string_equal(path, "/a");
    asdf_value_t *val = asdf_find_item_value(item);
    asdf_value_err_t err = asdf_value_as_string0(val, &str);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_string_equal(str, "a");

    item = asdf_value_find_iter(root, value_find_pred_a, &iter);
    assert_not_null(item);
    path = asdf_find_item_path(item);
    assert_string_equal(path, "/d/0");
    val = asdf_find_item_value(item);
    err = asdf_value_as_string0(val, &str);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_string_equal(str, "a");

    item = asdf_value_find_iter(root, value_find_pred_a, &iter);
    // Found all values matching the predicate
    assert_null(item);

    asdf_value_destroy(root);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_find_iter_max_depth) {
    const char *filename = get_fixture_file_path("nested.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    asdf_value_t *root = asdf_get_value(file, "/");
    assert_not_null(root);
    assert_true(asdf_value_is_mapping(root));

    // When iteration is set with max_depth = 0 it should never descend into
    // any sub-collections, but should still find values at the top-level of
    // the input root collection
    asdf_find_iter_t iter = asdf_find_iter_init_ex(true, asdf_find_descend_all, 0);
    asdf_find_item_t *item = NULL;

    item = asdf_value_find_iter(root, value_find_pred_a, &iter);
    assert_not_null(item);
    const char *str = NULL;
    const char *path = asdf_find_item_path(item);
    assert_string_equal(path, "/a");
    asdf_value_t *val = asdf_find_item_value(item);
    asdf_value_err_t err = asdf_value_as_string0(val, &str);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_string_equal(str, "a");

    item = asdf_value_find_iter(root, value_find_pred_a, &iter);
    // Found all values matching the predicate
    assert_null(item);

    asdf_value_destroy(root);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_find_ex) {
    const char *filename = get_fixture_file_path("nested.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    asdf_value_t *root = asdf_get_value(file, "/");
    assert_not_null(root);
    assert_true(asdf_value_is_mapping(root));

    asdf_find_item_t *item = asdf_value_find_ex(
        root, value_find_pred_a, true, asdf_find_descend_all, 1);
    assert_not_null(item);
    const char *str = NULL;

    const char *path = asdf_find_item_path(item);
    assert_string_equal(path, "/a");
    asdf_value_t *val = asdf_find_item_value(item);
    asdf_value_err_t err = asdf_value_as_string0(val, &str);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_string_equal(str, "a");

    asdf_find_item_destroy(item);
    asdf_value_destroy(root);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_find_iter) {
    const char *filename = get_fixture_file_path("nested.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    asdf_value_t *root = asdf_get_value(file, "/");
    assert_not_null(root);
    assert_true(asdf_value_is_mapping(root));

    asdf_find_iter_t iter = asdf_find_iter_init();
    asdf_find_item_t *item = NULL;

    item = asdf_value_find_iter(root, value_find_pred_b, &iter);
    assert_not_null(item);
    const char *str = NULL;
    const char *path = asdf_find_item_path(item);
    // BFS should find the value "b" at the top-level first
    assert_string_equal(path, "/b");
    asdf_value_t *val = asdf_find_item_value(item);
    asdf_value_err_t err = asdf_value_as_string0(val, &str);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_string_equal(str, "b");

    item = asdf_value_find_iter(root, value_find_pred_b, &iter);
    path = asdf_find_item_path(item);
    assert_string_equal(path, "/c/b");
    val = asdf_find_item_value(item);
    err = asdf_value_as_string0(val, &str);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_string_equal(str, "b");

    item = asdf_value_find_iter(root, value_find_pred_b, &iter);
    path = asdf_find_item_path(item);
    assert_string_equal(path, "/d/1");
    val = asdf_find_item_value(item);
    err = asdf_value_as_string0(val, &str);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_string_equal(str, "b");

    // Should be no more values found
    item = asdf_value_find_iter(root, value_find_pred_b, &iter);
    assert_null(item);

    asdf_value_destroy(root);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_find) {
    const char *filename = get_fixture_file_path("nested.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    asdf_value_t *root = asdf_get_value(file, "/");
    assert_not_null(root);
    assert_true(asdf_value_is_mapping(root));

    asdf_find_item_t *item = asdf_value_find(root, value_find_pred_b);
    assert_not_null(item);
    const char *str = NULL;
    const char *path = asdf_find_item_path(item);
    // BFS should find the value "b" at the top-level first
    assert_string_equal(path, "/b");
    asdf_value_t *val = asdf_find_item_value(item);
    asdf_value_err_t err = asdf_value_as_string0(val, &str);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_string_equal(str, "b");
    asdf_find_item_destroy(item);

    asdf_value_destroy(root);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_find_on_scalar) {
    const char *filename = get_fixture_file_path("nested.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    asdf_value_t *root = asdf_get_value(file, "/a");
    assert_not_null(root);
    assert_true(asdf_value_is_scalar(root));

    asdf_find_item_t *item = asdf_value_find(root, value_find_pred_a);
    assert_not_null(item);
    const char *str = NULL;
    const char *path = asdf_find_item_path(item);
    assert_string_equal(path, "/a");
    asdf_value_t *val = asdf_find_item_value(item);
    asdf_value_err_t err = asdf_value_as_string0(val, &str);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_string_equal(str, "a");
    asdf_find_item_destroy(item);

    root = asdf_get_value(file, "/b");
    assert_not_null(root);
    assert_true(asdf_value_is_scalar(root));
    item = asdf_value_find(root, value_find_pred_a);
    assert_null(item);

    asdf_value_destroy(root);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_path) {
    assert_null(asdf_value_path(NULL));
    const char *filename = get_fixture_file_path("nested.asdf");
    asdf_file_t *file = asdf_open(filename, "r");
    assert_not_null(file);
    asdf_value_t *value = asdf_get_value(file, "/d/0");
    assert_not_null(value);
    assert_string_equal(asdf_value_path(value), "/d/0");
    asdf_value_t *clone = asdf_value_clone(value);
    assert_string_equal(asdf_value_path(clone), "/d/0");
    asdf_value_destroy(value);
    asdf_value_destroy(clone);
    asdf_close(file);
    return MUNIT_OK;
}


/** This test is basically tautological :) */
MU_TEST(test_asdf_value_type_string) {
    assert_string_equal(asdf_value_type_string(-1), "<unknown>");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_SEQUENCE), "sequence");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_MAPPING), "mapping");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_SCALAR), "scalar");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_STRING), "string");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_BOOL), "bool");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_NULL), "null");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_INT8), "int8");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_INT16), "int16");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_INT32), "int32");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_INT64), "int64");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_UINT8), "uint8");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_UINT16), "uint16");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_UINT32), "uint32");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_UINT64), "uint64");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_FLOAT), "float");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_DOUBLE), "double");
    assert_string_equal(asdf_value_type_string(ASDF_VALUE_EXTENSION), "<extension>");
    return MUNIT_OK;
}


/** Regression test for issue #75 */
MU_TEST(test_raw_value_type_preserved_after_type_resolution) {
    const char *filename = get_fixture_file_path("nested.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    asdf_value_t *root = asdf_get_value(file, "/");
    assert_not_null(root);
    assert_true(asdf_value_is_mapping(root));
    assert_true(asdf_value_is_container(root));

    asdf_meta_t *meta = NULL;
    assert_int(asdf_value_as_meta(root, &meta), ==, ASDF_VALUE_OK);
    assert_not_null(meta);

    assert_true(asdf_value_is_meta(root));
    assert_true(asdf_value_is_mapping(root));
    assert_true(asdf_value_is_container(root));

    // It can still be iterated over too, etc
    asdf_mapping_t *root_map = NULL;
    assert_int(asdf_value_as_mapping(root, &root_map), ==, ASDF_VALUE_OK);
    asdf_mapping_iter_t iter = asdf_mapping_iter_init();
    asdf_mapping_item_t *item = asdf_mapping_iter(root_map, &iter);
    assert_not_null(item);
    asdf_value_t *a = asdf_mapping_item_value(item);
    assert_string_equal(asdf_value_path(a), "/a");
    asdf_value_destroy(a);
    free(item);
    asdf_meta_destroy(meta);
    asdf_value_destroy(root);
    asdf_close(file);
    return MUNIT_OK;
}

/** Regression test for double-free bug on cloned extension values */
MU_TEST(regression_clone_extension_value) {
    const char *path = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_value_t *value = asdf_get_value(file, "data");
    assert_not_null(value);
    assert_true(asdf_value_is_ndarray(value));
    asdf_value_t *cloned = asdf_value_clone(value);
    assert_true(asdf_value_is_ndarray(cloned));
    assert_string_equal(asdf_value_path(value), asdf_value_path(cloned));
    asdf_value_destroy(value);
    asdf_value_destroy(cloned);
    asdf_close(file);
    return MUNIT_OK;
}

/** Regression test for overflow bug when reading negative integers */
MU_TEST(regression_read_min_int) {
    const char *path = get_fixture_file_path("scalars-out.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_INT_VALUE(int8, "int8", ASDF_VALUE_OK, INT8_MIN);
    CHECK_INT_VALUE(int16, "int16", ASDF_VALUE_OK, INT16_MIN);
    CHECK_INT_VALUE(int32, "int32", ASDF_VALUE_OK, INT32_MIN);
    CHECK_INT_VALUE(int64, "int64", ASDF_VALUE_OK, INT64_MIN);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(regression_read_flt_max) {
    const char *path = get_fixture_file_path("scalars-out.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    CHECK_FLOAT_VALUE(float, "float", ASDF_VALUE_OK, FLT_MAX);
    CHECK_FLOAT_VALUE(double, "double", ASDF_VALUE_OK, DBL_MAX);
    CHECK_FLOAT_VALUE(float, "double", ASDF_VALUE_ERR_OVERFLOW, DBL_MAX);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    value,
    MU_RUN_TEST(test_asdf_value_get_type),
    MU_RUN_TEST(test_asdf_value_as_string),
    MU_RUN_TEST(test_asdf_value_as_string0),
    MU_RUN_TEST(test_asdf_value_as_scalar),
    MU_RUN_TEST(test_asdf_value_as_scalar0),
    MU_RUN_TEST(test_asdf_value_as_bool),
    MU_RUN_TEST(test_asdf_value_is_bool),
    MU_RUN_TEST(test_asdf_value_is_null),
    MU_RUN_TEST(test_asdf_value_is_int),
    MU_RUN_TEST(test_asdf_value_as_int8),
    MU_RUN_TEST(test_asdf_value_is_int8),
    MU_RUN_TEST(test_asdf_value_as_int16),
    MU_RUN_TEST(test_asdf_value_is_int16),
    MU_RUN_TEST(test_asdf_value_as_int32),
    MU_RUN_TEST(test_asdf_value_is_int32),
    MU_RUN_TEST(test_asdf_value_as_int64),
    MU_RUN_TEST(test_asdf_value_is_int64),
    MU_RUN_TEST(test_asdf_value_as_uint8),
    MU_RUN_TEST(test_asdf_value_is_uint8),
    MU_RUN_TEST(test_asdf_value_as_uint16),
    MU_RUN_TEST(test_asdf_value_is_uint16),
    MU_RUN_TEST(test_asdf_value_as_uint32),
    MU_RUN_TEST(test_asdf_value_is_uint32),
    MU_RUN_TEST(test_asdf_value_as_uint64),
    MU_RUN_TEST(test_asdf_value_is_uint64),
    MU_RUN_TEST(test_asdf_value_as_uint64_on_bigint),
    MU_RUN_TEST(test_asdf_value_as_float),
    MU_RUN_TEST(test_asdf_value_is_float),
    MU_RUN_TEST(test_asdf_value_as_double),
    MU_RUN_TEST(test_asdf_value_as_type),
    MU_RUN_TEST(test_asdf_value_is_type),
    MU_RUN_TEST(test_asdf_value_of_mapping),
    MU_RUN_TEST(test_asdf_value_of_sequence),
    MU_RUN_TEST(test_asdf_value_of_type),
    MU_RUN_TEST(test_value_tagged_strings),
    MU_RUN_TEST(test_asdf_mapping_create),
    MU_RUN_TEST(test_asdf_mapping_item_destroy),
    MU_RUN_TEST(test_asdf_mapping_iter),
    MU_RUN_TEST(test_asdf_mapping_get),
    MU_RUN_TEST(test_asdf_mapping_set_scalars),
    MU_RUN_TEST(test_asdf_sequence_append),
    MU_RUN_TEST(test_asdf_sequence_create),
    MU_RUN_TEST(test_asdf_sequence_iter),
    MU_RUN_TEST(test_asdf_sequence_get),
    MU_RUN_TEST(test_asdf_container_iter),
    MU_RUN_TEST(test_value_copy_with_parent_path),
    MU_RUN_TEST(test_asdf_value_file),
    MU_RUN_TEST(test_asdf_value_find_iter_ex_descend_mapping_only),
    MU_RUN_TEST(test_asdf_value_find_iter_ex_descend_sequence_only),
    MU_RUN_TEST(test_asdf_value_find_iter_max_depth),
    MU_RUN_TEST(test_asdf_value_find_ex),
    MU_RUN_TEST(test_asdf_value_find_iter),
    MU_RUN_TEST(test_asdf_value_find),
    MU_RUN_TEST(test_asdf_value_find_on_scalar),
    MU_RUN_TEST(test_asdf_value_path),
    MU_RUN_TEST(test_asdf_value_type_string),
    MU_RUN_TEST(test_raw_value_type_preserved_after_type_resolution),
    // TODO: Maybe set up a separate test suite for regression tests
    MU_RUN_TEST(regression_clone_extension_value),
    MU_RUN_TEST(regression_read_min_int),
    MU_RUN_TEST(regression_read_flt_max)
);


MU_RUN_SUITE(value);
