/**
 * Emitter internal tests
 *
 * These tests exercise emitter internals (``ASDF_LOCAL`` functions) and must
 * therefore link against ``libasdf_static.la`` rather than the shared library.
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "asdf/core/ndarray.h"
#include "asdf/emitter.h"
#include "asdf/file.h"
#include "asdf/value.h"

#include "emitter.h"
#include "file.h"
#include "stream.h"

#include "munit.h"
#include "util.h"


/**
 * Test that switching the emitter output stream mid-emission preserves state
 *
 * Regression test for issue #187: asdf_emitter_set_output_* functions used to
 * reset emitter->state to ASDF_EMITTER_STATE_INITIAL, breaking the realloc
 * optimization in asdf_write_to_mem and any other mid-emission stream switch.
 *
 * Write YAML to buf_yaml, switch stream to buf_blocks, write the rest
 * (blocks).  Verify that concatenating the two halves matches a reference
 * single-pass write.  Both sides use ASDF_EMITTER_OPT_NO_BLOCK_INDEX so that
 * no position-dependent block index is emitted, making byte-for-byte
 * comparison meaningful.
 */
MU_TEST(test_emitter_stream_switch) {
    const size_t n = 1024;
    const uint64_t shape[] = {n};

    uint8_t ref_data[n];
    for (size_t idx = 0; idx < n; idx++)
        ref_data[idx] = (uint8_t)(idx % 256);

    /* Large buffers; a simple 1 KiB ndarray needs only a few KiB total */
    const size_t cap = 64 * 1024;
    uint8_t *buf_yaml = calloc(1, cap);
    uint8_t *buf_blocks = calloc(1, cap);
    if (!buf_yaml || !buf_blocks) {
        free(buf_yaml);
        free(buf_blocks);
        return MUNIT_ERROR;
    }

    /* split emission */
    asdf_ndarray_t nd1 = {
        .datatype = (asdf_datatype_t){.type = ASDF_DATATYPE_UINT8},
        .byteorder = ASDF_BYTEORDER_BIG,
        .ndim = 1,
        .shape = shape,
    };
    uint8_t *d1 = asdf_ndarray_data_alloc(&nd1);
    assert_not_null(d1);
    memcpy(d1, ref_data, n);

    asdf_file_t *file1 = asdf_open_ex(NULL, 0, NULL);
    assert_not_null(file1);
    asdf_value_t *v1 = asdf_value_of_ndarray(file1, &nd1);
    assert_not_null(v1);
    assert_int(asdf_set_value(file1, "data", v1), ==, ASDF_VALUE_OK);

    asdf_emitter_cfg_t ecfg = ASDF_EMITTER_CFG_DEFAULT;
    ecfg.flags |= ASDF_EMITTER_OPT_NO_BLOCK_INDEX;
    asdf_emitter_t *emitter = asdf_emitter_create(file1, &ecfg);
    assert_not_null(emitter);

    /* Write YAML section to buf_yaml */
    assert_int(asdf_emitter_set_output_mem(emitter, buf_yaml, cap), ==, 0);
    asdf_emitter_state_t mid = asdf_emitter_emit_until(emitter, ASDF_EMITTER_STATE_BLOCKS);
    assert_int(mid, ==, ASDF_EMITTER_STATE_BLOCKS);
    off_t n_yaml = asdf_stream_tell(emitter->stream);
    assert_true(n_yaml > 0);

    /* Switch stream -- after the fix this must NOT reset the emitter state */
    assert_int(asdf_emitter_set_output_mem(emitter, buf_blocks, cap), ==, 0);
    asdf_emitter_state_t final_state = asdf_emitter_emit(emitter);
    assert_int(final_state, ==, ASDF_EMITTER_STATE_END);
    off_t n_blocks = asdf_stream_tell(emitter->stream);
    assert_true(n_blocks > 0);

    asdf_emitter_destroy(emitter);
    asdf_ndarray_data_dealloc(&nd1);
    asdf_close(file1);

    /* single-pass reference emission */
    asdf_ndarray_t nd2 = {
        .datatype = (asdf_datatype_t){.type = ASDF_DATATYPE_UINT8},
        .byteorder = ASDF_BYTEORDER_BIG,
        .ndim = 1,
        .shape = shape,
    };
    uint8_t *d2 = asdf_ndarray_data_alloc(&nd2);
    assert_not_null(d2);
    memcpy(d2, ref_data, n);

    asdf_config_t cfg2 = {.emitter = {.flags = ASDF_EMITTER_OPT_NO_BLOCK_INDEX}};
    asdf_file_t *file2 = asdf_open_mem_ex(NULL, 0, &cfg2);
    assert_not_null(file2);
    asdf_value_t *v2 = asdf_value_of_ndarray(file2, &nd2);
    assert_not_null(v2);
    assert_int(asdf_set_value(file2, "data", v2), ==, ASDF_VALUE_OK);

    void *single_buf = NULL;
    size_t single_size = 0;
    assert_int(asdf_write_to_mem(file2, &single_buf, &single_size), ==, 0);
    asdf_ndarray_data_dealloc(&nd2);
    asdf_close(file2);

    /* single_size is the allocated buffer size (includes trailing zeros), so it
     * will be >= the actual content; verify both halves of the split output
     * match the corresponding positions in the reference buffer */
    assert_size((size_t)n_yaml + (size_t)n_blocks, <=, single_size);
    assert_memory_equal((size_t)n_yaml, buf_yaml, single_buf);
    assert_memory_equal((size_t)n_blocks, buf_blocks, (uint8_t *)single_buf + n_yaml);

    free(buf_yaml);
    free(buf_blocks);
    free(single_buf);
    return MUNIT_OK;
}


/** Regression test for issue #204 */
MU_TEST(meta_prepended_to_root) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(NULL);

    // Add some keys to the file
    asdf_set_string0(file, "observer", "Dennis Richie");

    asdf_time_t time = {
        .value = "1948.78707178",
        .format = ASDF_TIME_FORMAT_JYEAR
    };
    asdf_set_time(file, "obstime", &time);

    // Add a history entry
    asdf_library_set_version(file, "0.0.0");
    assert_int(asdf_history_entry_add(file, "Test metadata emission."), ==, 0);
    asdf_write_to(file, filename);
    asdf_close(file);

    const char *fixture_filename = get_fixture_file_path("issue-204.asdf");
    assert_true(compare_files(filename, fixture_filename));
    return MUNIT_OK;
}


MU_TEST_SUITE(
    emitter,
    MU_RUN_TEST(test_emitter_stream_switch),
    MU_RUN_TEST(meta_prepended_to_root)
);


MU_RUN_SUITE(emitter);
