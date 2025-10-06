#include <asdf.h>
#include <asdf/gwcs/gwcs.h>

#include "munit.h"
#include "util.h"


MU_TEST(test_asdf_get_gwcs) {
    const char *path = get_fixture_file_path("roman_wcs.asdf");
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);

    asdf_gwcs_t *gwcs = NULL;
    asdf_value_err_t err = asdf_get_gwcs(file, "wcs", &gwcs);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(gwcs);

    assert_string_equal(gwcs->name, "270p65x48y69");

    asdf_gwcs_destroy(gwcs);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_gwcs,
    MU_RUN_TEST(test_asdf_get_gwcs)
);


MU_RUN_SUITE(test_asdf_gwcs);
