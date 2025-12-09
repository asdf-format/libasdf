/**
 * Tests for the test framework and utilities themselves
 *
 * The unit tests use munit (https://nemequ.github.io/munit/) but with some
 * rather sneaky wrappers around it.  There are also a few test utilities.
 */

#include <sys/stat.h>

#include "munit.h"
#include "util.h"


MU_TEST(test_test_name_1) {
    assert_string_equal(fixture->suite_name, "tests");
    assert_string_equal(fixture->test_name, "test_test_name_1");
    assert_string_equal(fixture->tempfile_prefix, "tests-test_test_name_1-");
    return MUNIT_OK;
}


/** Same as ``test_test_name_1`` but ensures a different test name :) */
MU_TEST(test_test_name_2) {
    assert_string_equal(fixture->suite_name, "tests");
    assert_string_equal(fixture->test_name, "test_test_name_2");
    assert_string_equal(fixture->tempfile_prefix, "tests-test_test_name_2-");
    return MUNIT_OK;
}


MU_TEST(test_get_temp_file_path) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    assert_not_null(filename);
    struct stat st;
    assert_int(stat(TEMP_DIR, &st), !=, -1);
    size_t temp_dir_len = strlen(TEMP_DIR);
    size_t prefix_len = strlen(fixture->tempfile_prefix);
    assert_int(strlen(filename), ==, temp_dir_len + 1 + prefix_len + 6 + 5);
    assert_memory_equal(temp_dir_len, filename, TEMP_DIR);
    assert_memory_equal(prefix_len, filename + 1 + temp_dir_len, fixture->tempfile_prefix);
    assert_memory_equal(5, filename + 1 + temp_dir_len + prefix_len + 6, ".asdf");
    return MUNIT_OK;
}


MU_TEST_SUITE(
    tests,
    MU_RUN_TEST(test_test_name_1),
    MU_RUN_TEST(test_test_name_2),
    MU_RUN_TEST(test_get_temp_file_path)
);


MU_RUN_SUITE(tests);
