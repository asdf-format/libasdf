/**
 * Tests for the test framework and utilities themselves
 *
 * The unit tests use munit (https://nemequ.github.io/munit/) but with some
 * rather sneaky wrappers around it.  There are also a few test utilities.
 */

#include "munit.h"


MU_TEST(test_test_name_1) {
    assert_string_equal(fixture->suite_name, "tests");
    assert_string_equal(fixture->test_name, "test_test_name_1");
    assert_string_equal(fixture->tempfile_prefix, "tests-test_test_name_1");
    return MUNIT_OK;
}


/** Same as ``test_test_name_1`` but ensures a different test name :) */
MU_TEST(test_test_name_2) {
    assert_string_equal(fixture->suite_name, "tests");
    assert_string_equal(fixture->test_name, "test_test_name_2");
    assert_string_equal(fixture->tempfile_prefix, "tests-test_test_name_2");
    return MUNIT_OK;
}


MU_TEST_SUITE(
    tests,
    MU_RUN_TEST(test_test_name_1),
    MU_RUN_TEST(test_test_name_2)
);


MU_RUN_SUITE(tests);
