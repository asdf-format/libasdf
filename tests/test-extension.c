#include <stdlib.h>

#include "munit.h"
#include "util.h"

#include <asdf/core/software.h>
#include <asdf/extension.h>
#include <asdf/file.h>
#include <asdf/util.h>


/* Struct that represents the "foo" type extension */
typedef struct {
    const char *foo;
} asdf_foo_t;


static asdf_software_t asdf_foo_software = {
    .name = "foo",
    .author = "STScI",
    .homepage = "https://stsci.edu",
    .version = "1.0.0"
};


// TODO: Not used yet, just needs to be defined
static asdf_value_err_t asdf_foo_deserialize(asdf_value_t *value,
                                             UNUSED(const void *userdata), void **out) {
    size_t foo_len = 0;
    const char *foo_val = NULL;
    asdf_value_err_t err = asdf_value_as_string(value, &foo_val, &foo_len);

    if (ASDF_VALUE_OK != err)
        return err;

    const char *foo_prefix = "foo:";
    size_t prefix_len = strlen(foo_prefix);
    char *buf = malloc(prefix_len + foo_len + 1);

    if (!buf)
        return ASDF_VALUE_ERR_OOM;

    memcpy(buf, foo_prefix, prefix_len);
    memcpy(buf + prefix_len, foo_val, foo_len);
    buf[prefix_len + foo_len] = '\0';

    asdf_foo_t *foo = malloc(sizeof(asdf_foo_t));

    if (!foo) {
        free(buf);
        return ASDF_VALUE_ERR_OOM;
    }
    foo->foo = (const char *)buf;
    *out = foo;
    return ASDF_VALUE_OK;
}


static void asdf_foo_dealloc(void *value) {
    asdf_foo_t *foo = value;
    if (foo && foo->foo) {
        free((void *)foo->foo);
        foo->foo = NULL;
    }
    free(foo);
}


ASDF_REGISTER_EXTENSION(
    foo,
    "stsci.edu:asdf/tests/foo-1.0.0",
    asdf_foo_t,
    &asdf_foo_software,
    asdf_foo_deserialize,
    asdf_foo_dealloc,
    NULL
)


MU_TEST(extension_registered) {
    const char *path = get_fixture_file_path("trivial-extension.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    const asdf_extension_t *ext = asdf_extension_get(file, "stsci.edu:asdf/tests/foo-1.0.0");
    assert_not_null(ext);
    assert_ptr_equal(ext, &asdf_foo_extension);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_is_foo) {
    const char *path = get_fixture_file_path("trivial-extension.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_value_t *value = asdf_get_value(file, "foo");
    assert_not_null(value);
    assert_true(asdf_value_is_foo(value));
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_value_as_foo) {
    const char *path = get_fixture_file_path("trivial-extension.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_value_t *value = asdf_get_value(file, "foo");
    assert_not_null(value);
    asdf_foo_t *foo = NULL;
    assert_int(asdf_value_as_foo(value, &foo), ==, ASDF_VALUE_OK);
    assert_not_null(foo);
    assert_not_null(foo->foo);
    assert_string_equal(foo->foo, "foo:foo");
    asdf_value_destroy(value);
    // TODO: User has to destroy the asdf_foo_t object for now; consider fixing this in #34
    asdf_foo_destroy(foo);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_is_foo) {
    const char *path = get_fixture_file_path("trivial-extension.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    assert_true(asdf_is_foo(file, "foo"));
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_get_foo) {
    const char *path = get_fixture_file_path("trivial-extension.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_foo_t *foo = NULL;
    assert_int(asdf_get_foo(file, "foo", &foo), ==, ASDF_VALUE_OK);
    assert_not_null(foo);
    assert_not_null(foo->foo);
    assert_string_equal(foo->foo, "foo:foo");
    // TODO: User has to destroy the asdf_foo_t object for now; consider fixing this in #34
    asdf_foo_destroy(foo);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    extension,
    MU_RUN_TEST(extension_registered),
    MU_RUN_TEST(test_asdf_value_is_foo),
    MU_RUN_TEST(test_asdf_value_as_foo),
    MU_RUN_TEST(test_asdf_is_foo),
    MU_RUN_TEST(test_asdf_get_foo)
);


MU_RUN_SUITE(extension);
