/** Tests for public error APIs */

#include <errno.h>

#include "asdf/error.h"
#include "asdf/file.h"

#include "munit.h"


MU_TEST(test_asdf_error_common) {
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    ASDF_ERROR_COMMON(file, ASDF_ERR_YAML_PARSE_FAILED);
    asdf_error_code_t code = asdf_error_code(file);
    assert_int(code, ==, ASDF_ERR_YAML_PARSE_FAILED);
    const char *message = asdf_error(file);
    assert_string_equal(message, "YAML parsing failed");

    /* Set a new parameterized error via the value */
    asdf_value_t *value = asdf_get_value(file, "");
    assert_not_null(value);
    ASDF_ERROR_COMMON(value, ASDF_ERR_INVALID_ARGUMENT, "foo", "bar");
    code = asdf_error_code(file);
    assert_int(code, ==, ASDF_ERR_INVALID_ARGUMENT);
    message = asdf_error(file);
    assert_string_equal(message, "invalid argument for foo: bar");
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_error_oom) {
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    ASDF_ERROR_OOM(file);
    asdf_error_code_t code = asdf_error_code(file);
    assert_int(code, ==, ASDF_ERR_OUT_OF_MEMORY);
    const char *message = asdf_error(file);
    assert_string_equal(message, "out of memory");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_error_system) {
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    ASDF_ERROR_SYSTEM(file, EINVAL);
    asdf_error_code_t code = asdf_error_code(file);
    assert_int(code, ==, ASDF_ERR_SYSTEM);
    assert_int(asdf_error_errno(file), ==, EINVAL);
    const char *message = asdf_error(file);
    /* May be platform or locale-dependent ...*/
    assert_string_equal(message, "Invalid argument");
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    error,
    MU_RUN_TEST(test_asdf_error_common),
    MU_RUN_TEST(test_asdf_error_oom),
    MU_RUN_TEST(test_asdf_error_system)
);


MU_RUN_SUITE(error);
