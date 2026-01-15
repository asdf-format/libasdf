#ifndef _GNU_SOURCE
#define _GNU_SOURCE  /* for memmem */
#endif
#include <errno.h>
#include <float.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stc/cstr.h>

#include "asdf/emitter.h"
#include "asdf/file.h"
#include "asdf/core/ndarray.h"
#include "asdf/value.h"

#include "compression/compression.h"
#include "config.h"
#include "file.h"

#include "munit.h"
#include "util.h"


/*
 * Very basic test of the `asdf_open_file` interface
 *
 * Tests opening/closing file, and reading a basic value out of the tree
 */
MU_TEST(test_asdf_open_file) {
    const char *filename = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    /* Read some key out of the tree */
    asdf_value_t *value = asdf_get_value(file, "asdf_library/name");
    assert_not_null(value);
    const char *name = NULL;
    asdf_value_err_t err = asdf_value_as_string0(value, &name);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(name);
    assert_string_equal(name, "asdf");
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_open_file_nonexistent) {
    asdf_file_t *file = asdf_open_file("does-not-exist", "r");
    assert_null(file);
    const char *error = asdf_error(file);
    assert_not_null(error);
    assert_string_equal(error, "No such file or directory");
    return MUNIT_OK;
}


MU_TEST(test_asdf_open_file_invalid_mode) {
    asdf_file_t *file = asdf_open_file("does-not-exist", "x");
    assert_null(file);
    const char *error = asdf_error(file);
    assert_not_null(error);
    assert_string_equal(error, "invalid mode string: \"x\"");
    return MUNIT_OK;
}


#define CHECK_GET_INT(type, key, expected) \
    do { \
        type##_t __v = 0; \
        assert_true(asdf_is_int(file, (key))); \
        bool __is = asdf_is_##type(file, (key)); \
        assert_true(__is); \
        asdf_value_err_t __err = asdf_get_##type(file, (key), &__v); \
        assert_int(__err, ==, ASDF_VALUE_OK); \
        type##_t __ve = (expected); \
        assert_int(__v, ==, __ve); \
    } while (0)


/* Test the high-level asdf_is_* and asdf_get_* helpers */
MU_TEST(scalar_getters) {
    const char *filename = get_fixture_file_path("scalars.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    assert_true(asdf_is_string(file, "plain"));
    const char *s = NULL;
    size_t len = 0;
    assert_int(asdf_get_string(file, "plain", &s, &len), ==, ASDF_VALUE_OK);
    char *s0 = strndup(s, len);
    assert_string_equal(s0, "string");
    free(s0);

    assert_true(asdf_is_bool(file, "false"));
    bool b = true;
    assert_int(asdf_get_bool(file, "false", &b), ==, ASDF_VALUE_OK);
    assert_false(b);

    assert_true(asdf_is_null(file, "null"));

    CHECK_GET_INT(int8, "int8", 127);
    CHECK_GET_INT(int16, "int16", 32767);
    CHECK_GET_INT(int32, "int32", 2147483647);
    CHECK_GET_INT(int64, "int64", 9223372036854775807LL);
    CHECK_GET_INT(uint8, "uint8", 255);
    CHECK_GET_INT(uint16, "uint16", 65535);
    CHECK_GET_INT(uint32, "uint32", 4294967295);
    CHECK_GET_INT(uint64, "uint64", 18446744073709551615ULL);

    float f = 0;
    assert_true(asdf_is_float(file, "float32"));
    assert_int(asdf_get_float(file, "float32", &f), ==, ASDF_VALUE_OK);
    assert_float(f, ==, 0.15625);

    double d = 0;
    assert_true(asdf_is_double(file, "float64"));
    assert_int(asdf_get_double(file, "float64", &d), ==, ASDF_VALUE_OK);
    assert_double(d, ==, 1.000000059604644775390625);

    assert_int(asdf_get_bool(file, "does-not-exist", &b), ==, ASDF_VALUE_ERR_NOT_FOUND);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_get_mapping) {
    const char *filename = get_fixture_file_path("value-types.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    assert_true(asdf_is_mapping(file, "mapping"));
    assert_false(asdf_is_mapping(file, "scalar"));
    asdf_mapping_t *mapping = NULL;
    asdf_value_err_t err = asdf_get_mapping(file, "mapping", &mapping);
    assert_int(err, ==, ASDF_VALUE_OK);
    asdf_mapping_destroy(mapping);
    err = asdf_get_mapping(file, "scalar", &mapping);
    assert_int(err, ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_get_sequence) {
    const char *filename = get_fixture_file_path("value-types.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    assert_true(asdf_is_sequence(file, "sequence"));
    assert_false(asdf_is_sequence(file, "scalar"));
    asdf_sequence_t *sequence = NULL;
    asdf_value_err_t err = asdf_get_sequence(file, "sequence", &sequence);
    assert_int(err, ==, ASDF_VALUE_OK);
    asdf_sequence_destroy(sequence);
    err = asdf_get_sequence(file, "scalar", &sequence);
    assert_int(err, ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_set_mapping) {
    // TODO: Change this test to use an in-memory file
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open_file(filename, "w");
    assert_not_null(file);
    asdf_mapping_t *mapping = asdf_mapping_create(file);
    assert_not_null(mapping);
    assert_int(asdf_set_mapping(file, "mapping", mapping), ==, ASDF_VALUE_OK);
    asdf_close(file);

    file = asdf_open(filename, "r");
    assert_not_null(file);
    assert_true(asdf_is_mapping(file, "mapping"));
    assert_int(asdf_get_mapping(file, "mapping", &mapping), ==, ASDF_VALUE_OK);
    assert_not_null(mapping);
    assert_int(asdf_mapping_size(mapping), ==, 0);
    asdf_mapping_destroy(mapping);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_set_sequence) {
    // TODO: Change this test to use an in-memory file
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open_file(filename, "w");
    assert_not_null(file);
    asdf_sequence_t *sequence = asdf_sequence_create(file);
    assert_not_null(sequence);
    assert_int(asdf_set_sequence(file, "sequence", sequence), ==, ASDF_VALUE_OK);
    asdf_close(file);

    file = asdf_open(filename, "r");
    assert_not_null(file);
    assert_true(asdf_is_sequence(file, "sequence"));
    assert_int(asdf_get_sequence(file, "sequence", &sequence), ==, ASDF_VALUE_OK);
    assert_not_null(sequence);
    assert_int(asdf_sequence_size(sequence), ==, 0);
    asdf_sequence_destroy(sequence);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_block_count) {
    const char *filename = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    assert_int(asdf_block_count(file), ==, 1);
    asdf_close(file);

    filename = get_reference_file_path("1.6.0/complex.asdf");
    file = asdf_open_file(filename, "r");
    assert_not_null(file);
    assert_int(asdf_block_count(file), ==, 4);
    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Tests the expected contents of fixtures/multi-block.asdf
 */
static int test_multi_block_asdf_content(asdf_file_t *file) {
    assert_int(asdf_block_count(file), ==, 4);

    char key[2];
    for (int idx = 1; idx <= 4; idx++) {
        snprintf(key, 2, "%d", idx);
        asdf_ndarray_t *ndarray = NULL;
        assert_int(asdf_get_ndarray(file, key, &ndarray), ==, ASDF_VALUE_OK);
        assert_not_null(ndarray);
        size_t size = 0;
        const uint8_t *data = asdf_ndarray_data_raw(ndarray, &size);
        assert_not_null(data);
        assert_int(size, ==, 128);
        for (int jdx = 0; jdx < 128; jdx++) {
            assert_int(data[jdx], ==, jdx / idx);
        }
        asdf_ndarray_destroy(ndarray);
    }

    return MUNIT_OK;
}


/**
 * Test that a file without a block index can still be read
 */
MU_TEST(missing_block_index) {
    const char *filename = get_fixture_file_path("multi-block.asdf");
    size_t len = 0;
    char *contents = read_file(filename, &len);
    assert_int(len, ==, 1746);  // Known size of the file
    // Find the beginning of the block index
    const char *block_index_magic = "#ASDF BLOCK INDEX";
    void *block_index_addr = memmem(contents, len, block_index_magic, strlen(block_index_magic));
    size_t block_index_idx = (uintptr_t)block_index_addr - (uintptr_t)contents;
    assert_int(block_index_idx, >, 0);

    // Open a memory buffer that includes just up to the block index, excluding it
    asdf_file_t *file = asdf_open_mem(contents, block_index_idx);
    assert_not_null(file);
    test_multi_block_asdf_content(file);
    asdf_close(file);
    free(contents);
    return MUNIT_OK;
}


/**
 * Test that a multi-block file with a block index can still be read 
 *
 * The test file contains 4 uint8 arrays containing the values 0..127
 * with integer division by the integer in the name of the array (1..4).
 * We insert some new keys in to the YAML tree "by hand", invalidating
 * the block index.
 */
MU_TEST(invalid_block_index) {
    const char *filename = get_fixture_file_path("multi-block.asdf");
    size_t len = 0;
    char *content_bytes = read_file(filename, &len);
    assert_int(len, ==, 1746);  // Known size of the file
    cstr contents = cstr_with_n(content_bytes, len);
    // Find the YAML document end marker
    ssize_t document_end_idx = cstr_find(&contents, "\n...");
    assert_int(document_end_idx, >, 0);
    // Insert a new key
    const char *insertion = "\nnew_key: \"here's some fresh garbage\"";
    cstr_insert(&contents, document_end_idx, insertion);
    csview tree = cstr_subview(&contents, 0, document_end_idx + 4 + strlen(insertion));
    munit_logf(MUNIT_LOG_DEBUG, "new tree:\n\n" c_svfmt "\n", c_svarg(tree));

    asdf_file_t *file = asdf_open_mem(cstr_str(&contents), cstr_size(&contents));
    assert_not_null(file);

    const char *s = NULL;
    assert_int(asdf_get_string0(file, "new_key", &s), ==, ASDF_VALUE_OK);
    assert_string_equal(s, "here's some fresh garbage");
    test_multi_block_asdf_content(file);
    asdf_close(file);
    cstr_drop(&contents);
    free(content_bytes);
    return MUNIT_OK;
}


MU_TEST(test_asdf_block_checksum) {
    assert_null(asdf_block_checksum(NULL));
    const char *filename = get_fixture_file_path("255-invalid-checksum.asdf");
    asdf_file_t *file = asdf_open(filename, "r");
    assert_not_null(file);
    asdf_block_t *block = asdf_block_open(file, 0);
    assert_not_null(block);
    assert_memory_equal(16, asdf_block_checksum(block),
                        "\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad\xbe\xef");
    asdf_block_close(block);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_block_checksum_verify) {
#ifndef HAVE_MD5
    return MUNIT_SKIP;
#else
    assert_false(asdf_block_checksum_verify(NULL));

    // Verify some of the test fixture files that were actually written by
    // libasdf itself, as well as one of the reference files
    const char *filenames[3] = {0};
    filenames[0] = get_reference_file_path("1.6.0/basic.asdf");
    filenames[1] = get_fixture_file_path("255-2-blocks.asdf");
    filenames[2] = get_fixture_file_path("255-block-no-index.asdf");

    asdf_file_t *file = NULL;
    asdf_block_t *block = NULL;
    uint8_t empty_checksum[16] = {0};

    for (int idx = 0; idx < 3; idx++) {
        assert_not_null(filenames[idx]);
        file = asdf_open(filenames[idx], "r");
        assert_not_null(file);
        block = asdf_block_open(file, 0);
        assert_not_null(block);
        assert_memory_not_equal(16, asdf_block_checksum(block), empty_checksum);
        assert_true(asdf_block_checksum_verify(block));
        asdf_block_close(block);
        asdf_close(file);
    }

    // Test file doped with a bad checksum
    const char *filename = get_fixture_file_path("255-invalid-checksum.asdf");
    file = asdf_open(filename, "r");
    assert_not_null(file);
    block = asdf_block_open(file, 0);
    assert_false(asdf_block_checksum_verify(block));
    assert_not_null(block);
    asdf_block_close(block);
    asdf_close(file);
    return MUNIT_OK;
#endif
}


MU_TEST(test_asdf_block_append) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(filename, "w");
    assert_not_null(file);
    const char *data = "this is my data and it is my friend";
    size_t len = strlen(data);
    assert_int(asdf_block_append(file, data, len), ==, 0);
    assert_int(asdf_block_count(file), ==, 1);
    asdf_block_t *block = asdf_block_open(file, 0);
    assert_not_null(block);
    assert_int(asdf_block_data_size(block), ==, len);
    size_t read_len = 0;
    const char *read_data = asdf_block_data(block, &read_len);
    assert_int(read_len, ==, len);
    assert_memory_equal(len, read_data, data);
    asdf_block_close(block);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_block_append_read_only) {
    const char *filename = get_fixture_file_path("multi-block.asdf");
    asdf_file_t *file = asdf_open(filename, "r");
    assert_not_null(file);
    assert_int(asdf_block_append(file, NULL, 0), ==, -1);
    const char *error = asdf_error(file);
    assert_string_equal(error, "cannot append blocks to read-only files");
    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Write a trivial ASDF file containing no YAML and no block index, just a
 * single binary block
 */
MU_TEST(write_block_no_index) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_config_t config = {.emitter = {
        .flags = ASDF_EMITTER_OPT_NO_EMIT_EMPTY_TREE | ASDF_EMITTER_OPT_NO_BLOCK_INDEX}};
    asdf_file_t *file = asdf_open_ex(filename, "w", &config);
    assert_not_null(file);

    size_t size = (UINT8_MAX + 1) * sizeof(uint8_t);
    uint8_t *data = malloc(size);

    if (!data)
        return MUNIT_ERROR;

    for (int idx = 0; idx <= UINT8_MAX; idx++)
        data[idx] = idx;

    assert_int(asdf_block_append(file, data, size), ==, 0);
    asdf_close(file);

    const char *reference = get_fixture_file_path("255-block-no-index.asdf");
    assert_true(compare_files(filename, reference));
    free(data);
    return MUNIT_OK;
}


MU_TEST(write_block_no_checksum) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_config_t config = {.emitter = {
        .flags = ASDF_EMITTER_OPT_NO_EMIT_EMPTY_TREE | ASDF_EMITTER_OPT_NO_BLOCK_CHECKSUM}};
    asdf_file_t *file = asdf_open_ex(filename, "w", &config);
    assert_not_null(file);

    size_t size = (UINT8_MAX + 1) * sizeof(uint8_t);
    uint8_t *data = malloc(size);

    if (!data)
        return MUNIT_ERROR;

    for (int idx = 0; idx <= UINT8_MAX; idx++)
        data[idx] = idx;

    assert_int(asdf_block_append(file, data, size), ==, 0);
    asdf_close(file);

    file = asdf_open(filename, "r");
    assert_not_null(file);
    asdf_block_t *block = asdf_block_open(file, 0);
    assert_not_null(block);
    uint8_t expected[16] = {0};
    assert_memory_equal(16, asdf_block_checksum(block), expected);
    asdf_block_close(block);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(write_blocks_and_index) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_config_t config = {.emitter = { .flags = ASDF_EMITTER_OPT_NO_EMIT_EMPTY_TREE }};
    asdf_file_t *file = asdf_open_ex(filename, "w", &config);
    assert_not_null(file);

    size_t size = (UINT8_MAX + 1) * sizeof(uint8_t);
    uint8_t *data = malloc(size);

    if (!data)
        return MUNIT_ERROR;

    for (int idx = 0; idx <= UINT8_MAX; idx++)
        data[idx] = idx;

    assert_int(asdf_block_append(file, data, size), ==, 0);
    assert_int(asdf_block_append(file, data, size), ==, 1);
    asdf_close(file);

    // Known good reference file containing two blocks and a block index with
    // known-good offsets
    const char *reference = get_fixture_file_path("255-2-blocks.asdf");
    assert_true(compare_files(filename, reference));
    free(data);
    return MUNIT_OK;
}


MU_TEST(test_asdf_set_scalar_type) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(filename, "w");
    assert_not_null(file);
    assert_int(asdf_set_string(file, "string", "string", 6), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_string0(file, "string0", "string0"), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_null(file, "null"), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_bool(file, "false", false), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_bool(file, "true", true), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_int8(file, "int8", INT8_MIN), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_int16(file, "int16", INT16_MIN), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_int32(file, "int32", INT32_MIN), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_int64(file, "int64", INT64_MIN), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_uint8(file, "uint8", UINT8_MAX), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_uint16(file, "uint16", UINT16_MAX), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_uint32(file, "uint32", UINT32_MAX), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_uint64(file, "uint64", UINT64_MAX), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_float(file, "float", FLT_MAX), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_double(file, "double", DBL_MAX), ==, ASDF_VALUE_OK);
    asdf_close(file);

    const char *reference = get_fixture_file_path("scalars-out.asdf");
    assert_true(compare_files(filename, reference));
    return MUNIT_OK;
}


MU_TEST(test_asdf_set_scalar_overwrite) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(filename, "w");
    assert_not_null(file);
    assert_int(asdf_set_string0(file, "string", "string"), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_string0(file, "string", "newstring"), ==, ASDF_VALUE_OK);
    asdf_close(file);

    file = asdf_open(filename, "r");
    assert_not_null(file);
    const char *read = NULL;
    assert_int(asdf_get_string0(file, "string", &read), ==, ASDF_VALUE_OK);
    assert_string_equal(read, "newstring");
    asdf_close(file);
    return MUNIT_OK;
}


/**
 * When setting a value to a path that doesn't already exist (the intermediat
 * nodes don't exist) the metadata structure is automatically materialized via
 * the path
 *
 * This tests a simple case of that.
 */
MU_TEST(test_asdf_set_path_materialization) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(filename, "w");
    assert_not_null(file);
    const char *val = "val";
    const char *path = "a/b/c/[1]/d";
    assert_int(asdf_set_string0(file, path, val), ==, ASDF_VALUE_OK);
    asdf_close(file);

    // Re-open the file in read mode and check the structure
    file = asdf_open(filename, "r");
    assert_not_null(file);
    const char *read_val = NULL;
    assert_int(asdf_get_string0(file, path, &read_val), ==, ASDF_VALUE_OK);
    assert_string_equal(read_val, val);

    // Since the sequence a/b/c didn't exist previously, but we assigned into
    // its 1-th element, this automatically fills the rest of the sequence with
    // nulls.  Check that
    asdf_sequence_t *seq = NULL;
    assert_int(asdf_get_sequence(file, "a/b/c", &seq), ==, ASDF_VALUE_OK);
    assert_int(asdf_sequence_size(seq), ==, 2);
    asdf_value_t *null = asdf_sequence_get(seq, 0);
    // NULL in the pointer sense, not the YAML scalar sense
    assert_not_null(null);
    assert_true(asdf_value_is_null(null));
    asdf_value_destroy(null);
    asdf_sequence_destroy(seq);
    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Compression tests
 * =================
 */


/**
 * Parameterize compression tests
 *
 * The reference file ``compressed.asdf`` contains two compressed arrays one under the name "zlib"
 * and one under the name "bzp2" so the test is parameterized on that basis.
 *
 * Will have to add a new test file and test case for lz4 compression.
 */
static char *comp_params[] = {"zlib", "bzp2", "lz4", NULL};
#ifdef ASDF_BLOCK_DECOMP_LAZY_AVAILABLE
static char *mode_params[] = {"eager", "lazy", NULL};
#else
static char *mode_params[] = {"eager", NULL};
#endif
static MunitParameterEnum comp_mode_test_params[] = {
    {"comp", comp_params},
    {"mode", mode_params},
    {NULL, NULL}
};


static MunitParameterEnum comp_test_params[] = {
    {"comp", comp_params},
    {NULL, NULL}
};


static asdf_block_decomp_mode_t decomp_mode_from_param(const char *mode) {
    if (strcmp(mode, "eager") == 0)
        return ASDF_BLOCK_DECOMP_MODE_EAGER;

    if (strcmp(mode, "lazy") == 0)
        return ASDF_BLOCK_DECOMP_MODE_LAZY;

    UNREACHABLE();
}


/** Basic test against the compressed.asdf reference file */
MU_TEST(read_compressed_reference_file) {
    const char *comp = munit_parameters_get(params, "comp");

    if (strcmp(comp, "lz4") == 0) {
        munit_log(MUNIT_LOG_INFO, "no lz4 compression in this reference file");
        return MUNIT_SKIP;
    }

    const char *filename = get_reference_file_path("1.6.0/compressed.asdf");
    asdf_config_t config = {
        .decomp = {
            .mode = decomp_mode_from_param(munit_parameters_get(params, "mode"))
        }
    };
    asdf_file_t *file = asdf_open_file_ex(filename, "r", &config);
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    asdf_value_err_t err = asdf_get_ndarray(file, comp, &ndarray);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);

    // The arrays in this reference file just contain the values 0 to 127
    int64_t expected[128] = {0};

    for (int idx = 0; idx < 128; idx++)
        expected[idx] = idx;

    size_t size = 0;
    const int64_t *dst = asdf_ndarray_data_raw(ndarray, &size);
    assert_int(size, ==, sizeof(int64_t) * 128);
    assert_memory_equal(size, dst, expected);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}


static void fisher_yates_shuffle(size_t *array, uint32_t size) {
    for (uint32_t idx = size - 1; idx > 0; idx--) {
        uint32_t jdx = (size_t) (munit_rand_uint32() % (idx + 1));
        size_t tmp = array[idx];
        array[idx] = array[jdx];
        array[jdx] = tmp;
    }
}


/** Test routine used for many of the compressed file tests
 *
 * Same basic test of reading the array data and testing the data against its
 * expected values
 *
 * If given randomize=true the pages of the expected data are checked in
 * random order
 */
static int test_compressed_file(
    asdf_file_t *file, const char *comp, bool should_own_fd, bool randomize) {
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    asdf_value_err_t err = asdf_get_ndarray(file, comp, &ndarray);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);

    // Each page-worth of data in this file contains the repeating pattern 0 to 255
    // except the first byte in each page which starts with the page index as a
    // canary
    int page_size = 4096;
    int num_pages = 100;
    uint8_t *expected = malloc(page_size * num_pages);

    if (!expected)
        return MUNIT_ERROR;

    for (int idx = 0; idx < page_size * num_pages; idx++) {
        if (idx % page_size == 0)
            expected[idx] = (idx / page_size) % 256;
        else
            expected[idx] = idx % 256;
    }

    size_t size = 0;
    const uint8_t *dst = asdf_ndarray_data_raw(ndarray, &size);
    // Check for errors and log it if there was one (useful for debugging failures in this test)
    const char *error = asdf_error(file);
    if (error)
        munit_logf(MUNIT_LOG_ERROR, "error after opening the ndarray: %s", error);
    assert_null(error);
    assert_int(size, ==, page_size * num_pages);

    if (!randomize) {
        assert_memory_equal(size, dst, expected);
    } else {
        size_t *pages = malloc(num_pages * sizeof(size_t));

        if (!pages)
            return MUNIT_ERROR;

        for (int idx = 0; idx < num_pages; idx++)
            pages[idx] = idx;

        fisher_yates_shuffle(pages, num_pages);

        for (int idx = 0; idx < num_pages; idx++) {
            size_t page_idx = pages[idx];
            //munit_logf(MUNIT_LOG_DEBUG, "checking page %zu\n", page_idx);
            assert_memory_equal(
                page_size, dst + (page_idx * page_size), expected + (page_idx * page_size));
        }

        free(pages);
    }

    const asdf_block_t *block = asdf_ndarray_block(ndarray);
    assert_not_null(block);
    assert_not_null(block->comp_state);

    int fd = block->comp_state->fd;

    if (should_own_fd) {
        assert_true(block->comp_state->own_fd);
        assert_int(fd, >, 2);
        struct stat st;
        assert_int(fstat(fd, &st), ==, 0);
        assert_true(S_ISREG(st.st_mode));
    } else {
        assert_false(block->comp_state->own_fd);
        assert_int(fd, ==, -1);
    }

    asdf_ndarray_destroy(ndarray);

    if (should_own_fd) {
        // The file descriptor for the temp file was closed
        errno = 0;
        assert_int(close(fd), ==, -1);
        assert_int(errno, ==, EBADF);
    }

    free(expected);
    return MUNIT_OK;
}


MU_TEST(read_compressed_block) {
    const char *comp = munit_parameters_get(params, "comp");
    const char *filename = get_fixture_file_path("compressed.asdf");
    asdf_config_t config = {
        .decomp = {
            .mode = decomp_mode_from_param(munit_parameters_get(params, "mode"))
        }
    };
    asdf_file_t *file = asdf_open_file_ex(filename, "r", &config);
    int ret = test_compressed_file(file, comp, false, false);
    asdf_close(file);
    return ret;
}


/**
 * Test decompression to a temp file (set memory threshold very low to force it)
 */
MU_TEST(read_compressed_block_to_file) {
    const char *comp = munit_parameters_get(params, "comp");
    const char *filename = get_fixture_file_path("compressed.asdf");

    asdf_config_t config = {
        .decomp = {
            .mode = ASDF_BLOCK_DECOMP_MODE_EAGER,
            .max_memory_bytes = 1
        }
    };
    asdf_file_t *file = asdf_open_file_ex(filename, "r", &config);
    int ret = test_compressed_file(file, comp, true, false);
    asdf_close(file);
    return ret;
}


/**
 * Test decompression to a temp file based on memory threshold
 */
MU_TEST(read_compressed_block_to_file_on_threshold) {
    const char *comp = munit_parameters_get(params, "comp");
    const char *filename = get_fixture_file_path("compressed.asdf");

    // Determine the threshold parameter to used based on the actual system memory
    size_t total_memory = get_total_memory();

    if (total_memory == 0) {
        munit_log(MUNIT_LOG_INFO, "memory information not available; skipping test...");
        return MUNIT_SKIP;
    }

    // Choose a smallish value (less then the array size in the test file) to determine a
    // memory threshold that should trigger file use
    double max_memory_threshold = (100.0 / (double)total_memory);

    asdf_config_t config = {
        .decomp = {
            .mode = ASDF_BLOCK_DECOMP_MODE_EAGER,
            .max_memory_threshold = max_memory_threshold
        }
    };
    asdf_file_t *file = asdf_open_ex(filename, "r", &config);
    int ret = test_compressed_file(file, comp, true, false);
    asdf_close(file);
    return ret;
}


/**
 * Test opening a compressed block in lazy read mode, but without reading it
 *
 * Tests edge cases where the compression handler isn't stopped properly or
 * goes into an undefined state if we don't decompress the whole file first.
 */
MU_TEST(open_close_compressed_block) {
    const char *comp = munit_parameters_get(params, "comp");
    const char *filename = get_fixture_file_path("compressed.asdf");
    asdf_config_t config = {
        .decomp = {
            .mode = decomp_mode_from_param(munit_parameters_get(params, "mode"))
        }
    };
    asdf_file_t *file = asdf_open_ex(filename, "r", &config);
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    asdf_value_err_t err = asdf_get_ndarray(file, comp, &ndarray);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    asdf_ndarray_data_raw(ndarray, NULL);
    // Check for errors and log it if there was one (useful for debugging failures in this test)
    const char *error = asdf_error(file);
    if (error)
        munit_logf(MUNIT_LOG_ERROR, "error after opening the ndarray: %s", error);
    assert_null(error);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}


/* Used for compressed_block_no_hang_on_segfault
 *
 * This is to ensure that trying to access the data after the block is closed
 * actually results in a segfault instead of just hanging the process
 *
 * (if the test fails the process will just hang)
 */
static sigjmp_buf sigsegv_jmp;


static void segv_handler(int sig) {
    siglongjmp(sigsegv_jmp, sig);
}


MU_TEST(compressed_block_no_hang_on_segfault) {
    const char *comp = munit_parameters_get(params, "comp");
    const char *filename = get_fixture_file_path("compressed.asdf");
    asdf_config_t config = {
        .decomp = {
            .mode = decomp_mode_from_param(munit_parameters_get(params, "mode")),
            .chunk_size = 4096
        }
    };
    asdf_file_t *file = asdf_open_ex(filename, "r", &config);
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    asdf_value_err_t err = asdf_get_ndarray(file, comp, &ndarray);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    const uint8_t *data = asdf_ndarray_data_raw(ndarray, NULL);
    // Check for errors and log it if there was one (useful for debugging failures in this test)
    const char *error = asdf_error(file);
    if (error)
        munit_logf(MUNIT_LOG_ERROR, "error after opening the ndarray: %s", error);
    assert_null(error);

    volatile uint8_t x = data[0];
    (void)x;

    asdf_ndarray_destroy(ndarray);

    // Try to access the data after the ndarray is closed; should segfault
    struct sigaction sa = {0};
    struct sigaction old_segv_sa = {0};
    struct sigaction old_bus_sa = {0};
    sa.sa_handler = segv_handler;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGSEGV, &sa, &old_segv_sa);
    sigaction(SIGBUS, &sa, &old_bus_sa);

    int rc = sigsetjmp(sigsegv_jmp, 1);
    if (rc == 0) {
        alarm(1);
        x = data[4096];
        munit_log(MUNIT_LOG_INFO, "fail: did not segfault");
        alarm(0);
        sigaction(SIGSEGV, &old_segv_sa, NULL);
        sigaction(SIGBUS, &old_bus_sa, NULL);
        return MUNIT_FAIL;
    }

    if (rc == SIGBUS) {
        munit_log(MUNIT_LOG_INFO, "passed: got SIGBUS");
    } else if (rc == SIGSEGV) {
        munit_log(MUNIT_LOG_INFO, "passed: got SIGSEGV");
    }

    alarm(0);
    sigaction(SIGSEGV, &old_segv_sa, NULL);
    sigaction(SIGBUS, &old_bus_sa, NULL);

    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Test decompressed block lazy random access
 */
MU_TEST(read_compressed_block_lazy_random_access) {
    const char *comp = munit_parameters_get(params, "comp");
    const char *filename = get_fixture_file_path("compressed.asdf");

    asdf_config_t config = {
        .decomp = {
            // Run the test in eager mode too as a control
            .mode = decomp_mode_from_param(munit_parameters_get(params, "mode")),
            .chunk_size = 4096
        }
    };

    asdf_file_t *file = asdf_open_ex(filename, "r", &config);
    int ret = test_compressed_file(file, comp, false, true);
    asdf_close(file);
    return ret;
}


/**
 * Write mode tests
 * ================
 */


/**
 * Opening and closing a file without adding content results in an empty file
 */
MU_TEST(write_empty) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    // Allow emitting an "empty" ASDF file that is still a valid ASDF file
    // (has the ASDF header) but contains no tree or blocks.
    asdf_file_t *file = asdf_open(filename, "w");
    assert_not_null(file);
    asdf_close(file);
    size_t len = SIZE_MAX;
    const char *contents = read_file(filename, &len);
    assert_int(len, ==, 0);
    free((char *)contents);
    return MUNIT_OK;
}


/**
 * Write the bare minimal valid ASDF file with no tree or blocks
 */
MU_TEST(write_minimal) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    // Allow emitting an "empty" ASDF file that is still a valid ASDF file
    // (has the ASDF header) but contains no tree or blocks.
    asdf_config_t config = { .emitter = { .flags = ASDF_EMITTER_OPT_EMIT_EMPTY }};
    asdf_file_t *file = asdf_open_ex(filename, "w", &config);
    assert_not_null(file);
    asdf_close(file);
    assert_true(compare_files(filename, get_fixture_file_path("parse-minimal.asdf")));
    return MUNIT_OK;
}


/**
 * Write the bare minimal valid ASDF file with an empty YAML tree
 */
MU_TEST(write_minimal_empty_tree) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    // Allow emitting an "empty" ASDF file that is still a valid ASDF file
    // (has the ASDF header) but contains no tree or blocks.
    asdf_config_t config = { .emitter = {
        .flags = ASDF_EMITTER_OPT_EMIT_EMPTY_TREE }};
    asdf_file_t *file = asdf_open_ex(filename, "w", &config);
    assert_not_null(file);
    asdf_close(file);
    assert_true(compare_files(filename, get_fixture_file_path("parse-minimal-empty-tree.asdf")));
    return MUNIT_OK;
}


/**
 * Test adding additional custom tag handles to the document
 */
MU_TEST(write_custom_tag_handle) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    // Allow emitting an "empty" ASDF file that is still a valid ASDF file
    // (has the ASDF header) but contains no tree or blocks.
    asdf_yaml_tag_handle_t tag_handles[] = {{ "!foo", "tag:example.com:foo/" }, { NULL, NULL }};
    asdf_config_t config = { .emitter = {
        .flags = ASDF_EMITTER_OPT_EMIT_EMPTY_TREE,
        .tag_handles = tag_handles
    }};
    asdf_file_t *file = asdf_open_ex(filename, "w", &config);
    assert_not_null(file);
    asdf_close(file);
    assert_true(compare_files(filename, get_fixture_file_path("custom-tag-handle.asdf")));
    return MUNIT_OK;
}


MU_TEST_SUITE(
    file,
    MU_RUN_TEST(test_asdf_open_file),
    MU_RUN_TEST(test_asdf_open_file_nonexistent),
    MU_RUN_TEST(test_asdf_open_file_invalid_mode),
    MU_RUN_TEST(scalar_getters),
    MU_RUN_TEST(test_asdf_get_mapping),
    MU_RUN_TEST(test_asdf_get_sequence),
    MU_RUN_TEST(test_asdf_set_mapping),
    MU_RUN_TEST(test_asdf_set_sequence),
    MU_RUN_TEST(test_asdf_block_count),
    MU_RUN_TEST(missing_block_index),
    MU_RUN_TEST(invalid_block_index),
    MU_RUN_TEST(test_asdf_block_checksum),
    MU_RUN_TEST(test_asdf_block_checksum_verify),
    MU_RUN_TEST(test_asdf_block_append),
    MU_RUN_TEST(test_asdf_block_append_read_only),
    MU_RUN_TEST(write_block_no_index),
    MU_RUN_TEST(write_block_no_checksum),
    MU_RUN_TEST(write_blocks_and_index),
    MU_RUN_TEST(test_asdf_set_scalar_type),
    MU_RUN_TEST(test_asdf_set_scalar_overwrite),
    MU_RUN_TEST(test_asdf_set_path_materialization),
    MU_RUN_TEST(read_compressed_reference_file, comp_mode_test_params),
    MU_RUN_TEST(read_compressed_block, comp_mode_test_params),
    MU_RUN_TEST(read_compressed_block_to_file, comp_test_params),
    MU_RUN_TEST(read_compressed_block_to_file_on_threshold, comp_test_params),
    MU_RUN_TEST(open_close_compressed_block, comp_mode_test_params),
    MU_RUN_TEST(read_compressed_block_lazy_random_access, comp_mode_test_params),
    MU_RUN_TEST(compressed_block_no_hang_on_segfault, comp_mode_test_params),
    MU_RUN_TEST(write_empty),
    MU_RUN_TEST(write_minimal),
    MU_RUN_TEST(write_minimal_empty_tree),
    MU_RUN_TEST(write_custom_tag_handle)
);


MU_RUN_SUITE(file);
