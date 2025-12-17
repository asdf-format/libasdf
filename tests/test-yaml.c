#include "munit.h"

#include "yaml.h"


#define CHECK_EMPTY_PATH(path) do { \
    assert_int(asdf_yaml_path_size(&path), ==, 1); \
    const asdf_yaml_path_component_t *comp = asdf_yaml_path_at(&path, 0); \
    assert_not_null(comp); \
    assert_null(comp->parent); \
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_MAP); \
    assert_string_equal(comp->key, ""); \
} while (0)


MU_TEST(test_asdf_yaml_path_parse_empty) {
    asdf_yaml_path_t path = asdf_yaml_path_init();
    assert_true(asdf_yaml_path_parse(NULL, &path));
    CHECK_EMPTY_PATH(path);
    asdf_yaml_path_clear(&path);

    assert_true(asdf_yaml_path_parse("", &path));
    CHECK_EMPTY_PATH(path);
    asdf_yaml_path_clear(&path);

    assert_true(asdf_yaml_path_parse("/", &path));
    CHECK_EMPTY_PATH(path);
    asdf_yaml_path_clear(&path);

    assert_true(asdf_yaml_path_parse("     ", &path));
    CHECK_EMPTY_PATH(path);
    asdf_yaml_path_clear(&path);

    asdf_yaml_path_drop(&path);
    return MUNIT_OK;
}


MU_TEST(test_asdf_yaml_path_parse_single_component) {
    asdf_yaml_path_t path = asdf_yaml_path_init();
    assert_true(asdf_yaml_path_parse("a", &path));
    assert_int(asdf_yaml_path_size(&path), ==, 1);
    const asdf_yaml_path_component_t *comp = asdf_yaml_path_at(&path, 0);
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_MAP);
    assert_string_equal(comp->key, "a");
    assert_string_equal(comp->parent, "");
    asdf_yaml_path_clear(&path);

    assert_true(asdf_yaml_path_parse("0", &path));
    assert_int(asdf_yaml_path_size(&path), ==, 1);
    comp = asdf_yaml_path_at(&path, 0);
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_ANY);
    assert_string_equal(comp->key, "0");
    assert_int(comp->index, ==, 0);
    assert_string_equal(comp->parent, "");
    asdf_yaml_path_clear(&path);

    assert_true(asdf_yaml_path_parse("[0]", &path));
    assert_int(asdf_yaml_path_size(&path), ==, 1);
    comp = asdf_yaml_path_at(&path, 0);
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_SEQ);
    assert_null(comp->key);
    assert_int(comp->index, ==, 0);
    assert_string_equal(comp->parent, "");
    asdf_yaml_path_clear(&path);

    assert_true(asdf_yaml_path_parse("'a'", &path));
    assert_int(asdf_yaml_path_size(&path), ==, 1);
    comp = asdf_yaml_path_at(&path, 0);
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_MAP);
    assert_string_equal(comp->key, "a");
    assert_string_equal(comp->parent, "");
    asdf_yaml_path_clear(&path);

    assert_true(asdf_yaml_path_parse("\"a\"", &path));
    assert_int(asdf_yaml_path_size(&path), ==, 1);
    comp = asdf_yaml_path_at(&path, 0);
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_MAP);
    assert_string_equal(comp->key, "a");
    assert_string_equal(comp->parent, "");
    asdf_yaml_path_clear(&path);

    asdf_yaml_path_drop(&path);
    return MUNIT_OK;
}


MU_TEST(test_asdf_yaml_path_parse_invalid_paths) {
    asdf_yaml_path_t path = asdf_yaml_path_init();
    // Unbalanced strings
    assert_false(asdf_yaml_path_parse("'", &path));
    assert_false(asdf_yaml_path_parse("a/'b", &path));
    assert_false(asdf_yaml_path_parse("a/b'", &path));
    assert_false(asdf_yaml_path_parse("\"", &path));
    assert_false(asdf_yaml_path_parse("a/'b\"", &path));
    assert_false(asdf_yaml_path_parse("a/b\"", &path));

    // Unbalanced brackets
    assert_false(asdf_yaml_path_parse("[", &path));
    assert_false(asdf_yaml_path_parse("]", &path));
    assert_false(asdf_yaml_path_parse("a/[0", &path));
    assert_false(asdf_yaml_path_parse("a/0]", &path));
    asdf_yaml_path_drop(&path);
    return MUNIT_OK;
}


MU_TEST(test_asdf_yaml_path_parse) {
    asdf_yaml_path_t path = asdf_yaml_path_init();
    // Sample path covering most cases
    assert_true(asdf_yaml_path_parse("/a/0/-1/[0]/[-1]/\"a/b\"/'a/b'/d\\\\/'e\\['/'f\\]'", &path));
    assert_int(asdf_yaml_path_size(&path), ==, 10);
    const asdf_yaml_path_component_t *comp = asdf_yaml_path_at(&path, 0);
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_MAP);
    assert_string_equal(comp->key, "a");
    assert_string_equal(comp->parent, "");

    comp = asdf_yaml_path_at(&path, 1);
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_ANY);
    assert_string_equal(comp->key, "0");
    assert_int(comp->index, ==, 0);
    assert_string_equal(comp->parent, "a");

    comp = asdf_yaml_path_at(&path, 2);
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_ANY);
    assert_string_equal(comp->key, "-1");
    assert_int(comp->index, ==, -1);
    assert_string_equal(comp->parent, "a/0");

    comp = asdf_yaml_path_at(&path, 3);
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_SEQ);
    assert_null(comp->key);
    assert_int(comp->index, ==, 0);
    assert_string_equal(comp->parent, "a/0/-1");

    comp = asdf_yaml_path_at(&path, 4);
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_SEQ);
    assert_null(comp->key);
    assert_int(comp->index, ==, -1);
    assert_string_equal(comp->parent, "a/0/-1/[0]");

    comp = asdf_yaml_path_at(&path, 5);
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_MAP);
    assert_string_equal(comp->key, "a/b");
    assert_string_equal(comp->parent, "a/0/-1/[0]/[-1]");

    comp = asdf_yaml_path_at(&path, 6);
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_MAP);
    assert_string_equal(comp->key, "a/b");
    assert_string_equal(comp->parent, "a/0/-1/[0]/[-1]/\"a/b\"");

    comp = asdf_yaml_path_at(&path, 7);
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_MAP);
    assert_string_equal(comp->key, "d\\\\");
    assert_string_equal(comp->parent, "a/0/-1/[0]/[-1]/\"a/b\"/'a/b'");

    comp = asdf_yaml_path_at(&path, 8);
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_MAP);
    assert_string_equal(comp->key, "e\\[");
    assert_string_equal(comp->parent, "a/0/-1/[0]/[-1]/\"a/b\"/'a/b'/d\\\\");

    comp = asdf_yaml_path_at(&path, 9);
    assert_int(comp->target, ==, ASDF_YAML_PC_TARGET_MAP);
    assert_string_equal(comp->key, "f\\]");
    assert_string_equal(comp->parent, "a/0/-1/[0]/[-1]/\"a/b\"/'a/b'/d\\\\/'e\\['");

    asdf_yaml_path_drop(&path);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    yaml,
    MU_RUN_TEST(test_asdf_yaml_path_parse_empty),
    MU_RUN_TEST(test_asdf_yaml_path_parse_invalid_paths),
    MU_RUN_TEST(test_asdf_yaml_path_parse_single_component),
    MU_RUN_TEST(test_asdf_yaml_path_parse)
);


MU_RUN_SUITE(yaml);
