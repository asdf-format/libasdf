#include "munit.h"
#include "util.h"

#include "file.h"


MU_TEST(test_asdf_block_data) {
    const char *filename = get_fixture_file_path("255.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    asdf_block_t *block = asdf_block_open(file, 0);
    assert_not_null(block);
    assert_int(asdf_block_data_size(block), ==, 256);
    size_t size = 0;
    uint8_t *data = (uint8_t *)asdf_block_data(block, &size);
    assert_not_null(data);
    assert_int(size, ==, 256);
    // Test file contains the integers 0 to 255
    for (int idx = 0; idx <= 255; idx++) {
        assert_int(data[idx], ==, idx);
    }
    asdf_block_close(block);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_block,
    MU_RUN_TEST(test_asdf_block_data)
);


MU_RUN_SUITE(test_asdf_block);
