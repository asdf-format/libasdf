#ifndef _GNU_SOURCE
#define _GNU_SOURCE  /* for memmem */
#endif
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <stc/cstr.h>

#include <errno.h>

#include "asdf/emitter.h"
#include "asdf/error.h"
#include "asdf/core/ndarray.h"
#include "asdf/value.h"

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
    assert_int(asdf_error_code(file), ==, ASDF_ERR_SYSTEM);
    assert_int(asdf_error_errno(file), ==, ENOENT);
    /* NULL safety */
    (void)asdf_error_code(NULL);
    (void)asdf_error_errno(NULL);
    return MUNIT_OK;
}


/** Test opening an empty file */
MU_TEST(test_asdf_open_file_empty) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    FILE *fp = fopen(filename, "w");
    fflush(fp);
    fclose(fp);

    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    // Initially there should be no problem since the library is completely
    // lazy until you try to read anything out of it
    assert_int(asdf_block_count(file), ==, 0);
    const char *error = asdf_error(file);
    assert_not_null(error);
    assert_string_equal(error, "unexpected end of file");
    assert_int(asdf_error_code(file), ==, ASDF_ERR_UNEXPECTED_EOF);
    asdf_close(file);
    return MUNIT_OK;
}


/** Test opening a file theat does not begin with an asdf header */
MU_TEST(test_asdf_open_file_not_asdf) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    FILE *fp = fopen(filename, "w");
    int ret = fputs("just some utter garbage\n", fp);
    if (MUNIT_UNLIKELY(ret <= 0)) {
        munit_error("error writing to temp file)");
        return MUNIT_ERROR;
    }
    fflush(fp);
    fclose(fp);

    asdf_file_t *file = asdf_open_file(filename, "r");
    assert_not_null(file);
    // Initially there should be no problem since the library is completely
    // lazy until you try to read anything out of it
    assert_int(asdf_block_count(file), ==, 0);
    const char *error = asdf_error(file);
    assert_not_null(error);
    assert_string_equal(error, "invalid ASDF header");
    assert_int(asdf_error_code(file), ==, ASDF_ERR_INVALID_ASDF_HEADER);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_open_file_invalid_mode) {
    asdf_file_t *file = asdf_open_file("does-not-exist", "x");
    assert_null(file);
    const char *error = asdf_error(file);
    assert_not_null(error);
    assert_string_equal(error, "invalid argument for mode: x");
    assert_int(asdf_error_code(file), ==, ASDF_ERR_INVALID_ARGUMENT);
    return MUNIT_OK;
}


/* This is more of an interface test to ensure it compiles */
MU_TEST(test_asdf_open_null_arg) {
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    asdf_close(file);
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

    CHECK_GET_INT(int8, "int8", -127);
    CHECK_GET_INT(int16, "int16", -32767);
    CHECK_GET_INT(int32, "int32", -2147483647);
    CHECK_GET_INT(int64, "int64", -9223372036854775807LL);
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
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    asdf_mapping_t *mapping = asdf_mapping_create(file);
    assert_not_null(mapping);
    assert_int(asdf_set_mapping(file, "mapping", mapping), ==, ASDF_VALUE_OK);
    void *buf = NULL;
    size_t size = 0;
    assert_int(asdf_write_to(file, &buf, &size), ==, 0);
    asdf_close(file);

    file = asdf_open((const void *)buf, size);
    assert_not_null(file);
    assert_true(asdf_is_mapping(file, "mapping"));
    assert_int(asdf_get_mapping(file, "mapping", &mapping), ==, ASDF_VALUE_OK);
    assert_not_null(mapping);
    assert_int(asdf_mapping_size(mapping), ==, 0);
    asdf_mapping_destroy(mapping);
    asdf_close(file);
    free(buf);
    return MUNIT_OK;
}


MU_TEST(test_asdf_set_sequence) {
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    asdf_sequence_t *sequence = asdf_sequence_create(file);
    assert_not_null(sequence);
    assert_int(asdf_set_sequence(file, "sequence", sequence), ==, ASDF_VALUE_OK);
    void *buf = NULL;
    size_t size = 0;
    assert_int(asdf_write_to(file, &buf, &size), ==, 0);
    asdf_close(file);

    file = asdf_open((const void *)buf, size);
    assert_not_null(file);
    assert_true(asdf_is_sequence(file, "sequence"));
    assert_int(asdf_get_sequence(file, "sequence", &sequence), ==, ASDF_VALUE_OK);
    assert_not_null(sequence);
    assert_int(asdf_sequence_size(sequence), ==, 0);
    asdf_sequence_destroy(sequence);
    asdf_close(file);
    free(buf);
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
    assert_false(asdf_block_checksum_verify(NULL, NULL));

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
        const unsigned char *expected = asdf_block_checksum(block);
        assert_memory_not_equal(ASDF_BLOCK_CHECKSUM_DIGEST_SIZE, expected , empty_checksum);
        unsigned char computed[ASDF_BLOCK_CHECKSUM_DIGEST_SIZE] = {0};
        assert_true(asdf_block_checksum_verify(block, computed));
        assert_memory_equal(ASDF_BLOCK_CHECKSUM_DIGEST_SIZE, computed, expected);
        asdf_block_close(block);
        asdf_close(file);
    }

    // Test file doped with a bad checksum
    const char *filename = get_fixture_file_path("255-invalid-checksum.asdf");
    file = asdf_open(filename, "r");
    assert_not_null(file);
    block = asdf_block_open(file, 0);
    assert_false(asdf_block_checksum_verify(block, NULL));
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
    assert_string_equal(error, "cannot write to a read-only stream or file");
    assert_int(asdf_error_code(file), ==, ASDF_ERR_STREAM_READ_ONLY);
    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Write a trivial ASDF file containing no YAML and no block index, just a
 * single binary block
 */
MU_TEST(write_block_no_index) {
#ifndef HAVE_MD5
    return MUNIT_SKIP;
#else
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_config_t config = {.emitter = {
        .flags = ASDF_EMITTER_OPT_NO_EMIT_EMPTY_TREE | ASDF_EMITTER_OPT_NO_BLOCK_INDEX}};
    asdf_file_t *file = asdf_open_ex(NULL, 0, &config);
    assert_not_null(file);

    size_t size = (UINT8_MAX + 1) * sizeof(uint8_t);
    uint8_t *data = malloc(size);

    if (!data)
        return MUNIT_ERROR;

    for (int idx = 0; idx <= UINT8_MAX; idx++)
        data[idx] = idx;

    assert_int(asdf_block_append(file, data, size), ==, 0);
    assert_int(asdf_write_to(file, filename), ==, 0);
    asdf_close(file);

    const char *reference = get_fixture_file_path("255-block-no-index.asdf");
    assert_true(compare_files(filename, reference));
    free(data);
    return MUNIT_OK;
#endif
}


MU_TEST(write_block_no_checksum) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_config_t config = {.emitter = {
        .flags = ASDF_EMITTER_OPT_NO_EMIT_EMPTY_TREE | ASDF_EMITTER_OPT_NO_BLOCK_CHECKSUM}};
    asdf_file_t *file = asdf_open_ex(NULL, 0, &config);
    assert_not_null(file);

    size_t size = (UINT8_MAX + 1) * sizeof(uint8_t);
    uint8_t *data = malloc(size);

    if (!data)
        return MUNIT_ERROR;

    for (int idx = 0; idx <= UINT8_MAX; idx++)
        data[idx] = idx;

    assert_int(asdf_block_append(file, data, size), ==, 0);
    assert_int(asdf_write_to(file, filename), ==, 0);
    asdf_close(file);
    free(data);

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
#ifndef HAVE_MD5
    return MUNIT_SKIP;
#else
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_config_t config = {.emitter = { .flags = ASDF_EMITTER_OPT_NO_EMIT_EMPTY_TREE }};
    asdf_file_t *file = asdf_open_ex(NULL, 0, &config);
    assert_not_null(file);

    size_t size = (UINT8_MAX + 1) * sizeof(uint8_t);
    uint8_t *data = malloc(size);

    if (!data)
        return MUNIT_ERROR;

    for (int idx = 0; idx <= UINT8_MAX; idx++)
        data[idx] = idx;

    assert_int(asdf_block_append(file, data, size), ==, 0);
    assert_int(asdf_block_append(file, data, size), ==, 1);
    assert_int(asdf_write_to(file, filename), ==, 0);
    asdf_close(file);

    // Known good reference file containing two blocks and a block index with
    // known-good offsets
    const char *reference = get_fixture_file_path("255-2-blocks.asdf");
    assert_true(compare_files(filename, reference));
    free(data);
    return MUNIT_OK;
#endif
}


/**
 * Test tries to write a simple file containing a single ndarray
 *
 * This attempts to reproduce the fixture file 255.asdf (which is generated
 * with the Python asdf library).  It is not yet anywhere near byte-for-byte
 * equivalent so for now we compare it to the test file 255-out.asdf.
 *
 * TODO: The goal is to make this output as close as possible and eventually
 * equivalent to the file written by Python.  This would require a function
 * that allows overriding the asdf_library software.  This might also prove
 * difficult to achieve *exactly* due to differences in how the respective
 * YAML libraries emit.
 */
MU_TEST(write_ndarray) {
#ifndef HAVE_MD5
    return MUNIT_SKIP;
#else
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(filename, "w");
    assert_not_null(file);

    asdf_ndarray_t ndarray = {
        .datatype = (asdf_datatype_t){.type = ASDF_DATATYPE_UINT8},
        .byteorder = ASDF_BYTEORDER_BIG,
        .ndim = 1,
        .shape = (const uint64_t[]){255},
    };

    uint8_t *data = asdf_ndarray_data_alloc(&ndarray);
    assert_not_null(data);

    for (int idx = 0; idx < 255; idx++)
        data[idx] = idx;

    asdf_value_t *value = asdf_value_of_ndarray(file, &ndarray);
    assert_not_null(value);
    assert_int(asdf_set_value(file, "data", value), ==, ASDF_VALUE_OK);
    asdf_close(file);
    asdf_ndarray_data_dealloc(&ndarray);

    // TODO: Re-enable this; currently byte-for-byte perfect formatting isn't achievable
    // reliably: https://github.com/asdf-format/libasdf/issues/149
    //const char *reference = get_fixture_file_path("255-out.asdf");
    //assert_true(compare_files(filename, reference));
    return MUNIT_OK;
#endif
}


MU_TEST(test_asdf_set_scalar_type) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(NULL);
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
    asdf_library_set_version(file, "0.0.0");
    assert_int(asdf_write_to(file, filename), ==, 0);
    asdf_close(file);

    const char *reference = get_fixture_file_path("scalars-out.asdf");
    assert_true(compare_files(filename, reference));
    return MUNIT_OK;
}


MU_TEST(test_asdf_set_scalar_overwrite) {
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    assert_int(asdf_set_string0(file, "string", "string"), ==, ASDF_VALUE_OK);
    assert_int(asdf_set_string0(file, "string", "newstring"), ==, ASDF_VALUE_OK);
    void *buf = NULL;
    size_t buf_size = 0;
    assert_int(asdf_write_to(file, &buf, &buf_size), ==, 0);
    asdf_close(file);

    file = asdf_open((const void *)buf, buf_size);
    assert_not_null(file);
    const char *read = NULL;
    assert_int(asdf_get_string0(file, "string", &read), ==, ASDF_VALUE_OK);
    assert_string_equal(read, "newstring");
    asdf_close(file);
    free(buf);
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
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    const char *val = "val";
    const char *path = "a/b/c/[1]/d";
    assert_int(asdf_set_string0(file, path, val), ==, ASDF_VALUE_OK);
    void *buf = NULL;
    size_t buf_size = 0;
    assert_int(asdf_write_to(file, &buf, &buf_size), ==, 0);
    asdf_close(file);

    // Re-open the file in read mode and check the structure
    file = asdf_open((const void *)buf, buf_size);
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
    free(buf);
    return MUNIT_OK;
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
    asdf_file_t *file = asdf_open_ex(NULL, 0, &config);
    assert_not_null(file);
    assert_int(asdf_write_to(file, filename), ==, 0);
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
        .flags = ASDF_EMITTER_OPT_EMIT_EMPTY_TREE | ASDF_EMITTER_OPT_NO_EMIT_ASDF_LIBRARY }};
    asdf_file_t *file = asdf_open_ex(NULL, 0, &config);
    assert_not_null(file);
    assert_int(asdf_write_to(file, filename), ==, 0);
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
        .flags = ASDF_EMITTER_OPT_EMIT_EMPTY_TREE | ASDF_EMITTER_OPT_NO_EMIT_ASDF_LIBRARY,
        .tag_handles = tag_handles
    }};
    asdf_file_t *file = asdf_open_ex(NULL, 0, &config);
    assert_not_null(file);
    assert_int(asdf_write_to(file, filename), ==, 0);
    asdf_close(file);
    assert_true(compare_files(filename, get_fixture_file_path("custom-tag-handle.asdf")));
    return MUNIT_OK;
}


/** Regression test for a double-free that could occur in this case */
MU_TEST(test_asdf_set_value_double_free) {
    const char *filename = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(filename, "w");
    assert_not_null(file);
    asdf_mapping_t *new_root = asdf_mapping_create(file);
    assert_not_null(new_root);
    assert_int(asdf_mapping_set_string0(new_root, "foo", "bar"), ==, ASDF_VALUE_OK);
    asdf_value_t *new_root_val = asdf_value_of_mapping(new_root);
    assert_int(asdf_set_value(file, "", new_root_val), ==, ASDF_VALUE_OK);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    file,
    MU_RUN_TEST(test_asdf_open_file),
    MU_RUN_TEST(test_asdf_open_file_nonexistent),
    MU_RUN_TEST(test_asdf_open_file_empty),
    MU_RUN_TEST(test_asdf_open_file_not_asdf),
    MU_RUN_TEST(test_asdf_open_file_invalid_mode),
    MU_RUN_TEST(test_asdf_open_null_arg),
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
    MU_RUN_TEST(write_ndarray),
    MU_RUN_TEST(test_asdf_set_scalar_type),
    MU_RUN_TEST(test_asdf_set_scalar_overwrite),
    MU_RUN_TEST(test_asdf_set_path_materialization),
    MU_RUN_TEST(write_empty),
    MU_RUN_TEST(write_minimal),
    MU_RUN_TEST(write_minimal_empty_tree),
    MU_RUN_TEST(write_custom_tag_handle),
    MU_RUN_TEST(test_asdf_set_value_double_free)
);


MU_RUN_SUITE(file);
