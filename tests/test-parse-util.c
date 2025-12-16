#include "munit.h"
#include "util.h"

#include "parse_util.h"
#include "yaml.h"


/**
 * Test parsing the absolute bare minimum ASDF file that can be parsed without errors
 *
 * It just consists of the #ASDF and #ASDF_STANDARD comments, and nothing more
 */
MU_TEST(test_is_yaml_1_1_directive) {
    assert_false(is_yaml_1_1_directive("", 0));
    // Wrong buffer size given
    assert_false(is_yaml_1_1_directive("%YAML 1.1\n", 8));
    assert_true(is_yaml_1_1_directive("%YAML 1.1\n", 10));
    assert_true(is_yaml_1_1_directive("%YAML 1.1\r\n", 11));
    return MUNIT_OK;
}


MU_TEST(test_is_generic_yaml_directive) {
    assert_false(is_generic_yaml_directive("", 0));
    // Wrong buffer size given
    assert_false(is_generic_yaml_directive("%YAML 2.0\n", 0));

    assert_false(is_generic_yaml_directive("miscellaneous garbage\n", 22));
    assert_true(is_generic_yaml_directive("%YAML 2.0\n", 10));
    assert_true(is_generic_yaml_directive("%YAML 2.0\r\n", 11));
    assert_true(is_generic_yaml_directive("%YAML 22.22\n", 12));
    assert_false(is_generic_yaml_directive("%YAML\n", 6));
    assert_false(is_generic_yaml_directive("%YAML ABC\n", 10));
    assert_false(is_generic_yaml_directive("%YAML 222", 10));
    assert_false(is_generic_yaml_directive("%YAML 2.A\n", 10));
    return MUNIT_OK;
}


MU_TEST_SUITE(
    parse_util,
    MU_RUN_TEST(test_is_yaml_1_1_directive),
    MU_RUN_TEST(test_is_generic_yaml_directive)
);


MU_RUN_SUITE(parse_util);
