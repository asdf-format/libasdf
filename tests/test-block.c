#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "munit.h"
#include "util.h"

#include "asdf/emitter.h"

#include "file.h"


/* Emitter config used by the write/round-trip block tests: emit only the
 * binary block(s), no empty YAML tree. */
static const asdf_config_t block_only_config = {
    .emitter = {.flags = ASDF_EMITTER_OPT_NO_EMIT_EMPTY_TREE}};


MU_TEST(block_data) {
    const char *filename = get_fixture_file_path("255.asdf");
    asdf_file_t *file = asdf_open(filename, "r");
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


/* create -> data_set (borrowed) -> append -> write -> read back */
MU_TEST(block_create_set_append) {
    asdf_file_t *file = asdf_open_ex(NULL, 0, (asdf_config_t *)&block_only_config);
    assert_not_null(file);

    const char *data = "block content assigned with asdf_block_data_set";
    size_t len = strlen(data);

    asdf_block_t *block = asdf_block_create(file);
    assert_not_null(block);
    assert_int(asdf_block_data_set(block, data, len), ==, 0);
    assert_not_null(asdf_block_append(file, block));
    assert_int(asdf_block_count(file), ==, 1);
    asdf_block_close(block);

    void *buf = NULL;
    size_t size = 0;
    assert_int(asdf_write_to(file, &buf, &size), ==, 0);
    asdf_close(file);

    asdf_file_t *rfile = asdf_open_mem(buf, size);
    assert_not_null(rfile);
    asdf_block_t *rblock = asdf_block_open(rfile, 0);
    assert_not_null(rblock);
    assert_int(asdf_block_data_size(rblock), ==, len);
    size_t rlen = 0;
    const char *rdata = asdf_block_data(rblock, &rlen);
    assert_int(rlen, ==, len);
    assert_memory_equal(len, rdata, data);
    asdf_block_close(rblock);
    asdf_close(rfile);
    free(buf);
    return MUNIT_OK;
}


/* create -> data_alloc -> fill -> append -> write -> read back */
MU_TEST(block_data_alloc_roundtrip) {
    asdf_file_t *file = asdf_open_ex(NULL, 0, (asdf_config_t *)&block_only_config);
    assert_not_null(file);

    size_t n = 256;
    asdf_block_t *block = asdf_block_create(file);
    assert_not_null(block);
    uint8_t *data = asdf_block_data_alloc(block, n);
    assert_not_null(data);

    for (size_t idx = 0; idx < n; idx++)
        data[idx] = (uint8_t)idx;

    assert_not_null(asdf_block_append(file, block));
    asdf_block_close(block);

    void *buf = NULL;
    size_t size = 0;
    assert_int(asdf_write_to(file, &buf, &size), ==, 0);
    asdf_close(file);

    asdf_file_t *rfile = asdf_open_mem(buf, size);
    assert_not_null(rfile);
    asdf_block_t *rblock = asdf_block_open(rfile, 0);
    assert_not_null(rblock);
    size_t rlen = 0;
    const uint8_t *rdata = asdf_block_data(rblock, &rlen);
    assert_int(rlen, ==, n);

    for (size_t idx = 0; idx < n; idx++)
        assert_int(rdata[idx], ==, (uint8_t)idx);

    asdf_block_close(rblock);
    asdf_close(rfile);
    free(buf);
    return MUNIT_OK;
}


/* create then destroy without appending */
MU_TEST(block_create_destroy) {
    asdf_file_t *file = asdf_open_ex(NULL, 0, NULL);
    assert_not_null(file);

    asdf_block_t *block = asdf_block_create(file);
    assert_not_null(block);
    uint8_t *data = asdf_block_data_alloc(block, 128);
    assert_not_null(data);
    memset(data, 0xab, 128);
    asdf_block_destroy(block); /* never appended */
    assert_int(asdf_block_count(file), ==, 0);

    /* a bare create/destroy with no data */
    asdf_block_t *empty = asdf_block_create(file);
    assert_not_null(empty);
    asdf_block_destroy(empty);

    asdf_close(file);
    return MUNIT_OK;
}


/* compression set on an uncompressed block -> compressed on write */
MU_TEST(block_compress_on_write) {
    asdf_file_t *file = asdf_open_ex(NULL, 0, (asdf_config_t *)&block_only_config);
    assert_not_null(file);

    size_t n = 4096;
    asdf_block_t *block = asdf_block_create(file);
    assert_not_null(block);
    uint8_t *data = asdf_block_data_alloc(block, n);
    assert_not_null(data);
    memset(data, 0x5a, n); /* highly compressible */
    assert_int(asdf_block_compression_set(block, "zlib"), ==, 0);
    assert_not_null(asdf_block_append(file, block));
    asdf_block_close(block);

    void *buf = NULL;
    size_t size = 0;
    assert_int(asdf_write_to(file, &buf, &size), ==, 0);
    asdf_close(file);

    asdf_file_t *rfile = asdf_open_mem(buf, size);
    assert_not_null(rfile);
    asdf_block_t *rblock = asdf_block_open(rfile, 0);
    assert_not_null(rblock);
    assert_string_equal(asdf_block_compression(rblock), "zlib");
    assert_int(asdf_block_data_size(rblock), ==, n);
    size_t rlen = 0;
    const uint8_t *rdata = asdf_block_data(rblock, &rlen); /* decompressed */
    assert_int(rlen, ==, n);

    for (size_t idx = 0; idx < n; idx++)
        assert_int(rdata[idx], ==, 0x5a);

    asdf_block_close(rblock);
    asdf_close(rfile);
    free(buf);
    return MUNIT_OK;
}


/* data_set_compressed: reproduce a compressed block byte-for-byte in a new
 * file, then verify it decompresses back to the original */
MU_TEST(block_data_set_compressed) {
    size_t n = 4096;

    /* First produce a compressed block and capture its raw (compressed) bytes */
    asdf_file_t *file = asdf_open_ex(NULL, 0, (asdf_config_t *)&block_only_config);
    assert_not_null(file);
    asdf_block_t *block = asdf_block_create(file);
    assert_not_null(block);
    uint8_t *data = asdf_block_data_alloc(block, n);
    assert_not_null(data);

    for (size_t idx = 0; idx < n; idx++)
        data[idx] = (uint8_t)(idx * 7);

    assert_int(asdf_block_compression_set(block, "zlib"), ==, 0);
    assert_not_null(asdf_block_append(file, block));
    asdf_block_close(block);

    void *buf = NULL;
    size_t size = 0;
    assert_int(asdf_write_to(file, &buf, &size), ==, 0);
    asdf_close(file);

    asdf_file_t *rfile = asdf_open_mem(buf, size);
    assert_not_null(rfile);
    asdf_block_t *rblock = asdf_block_open(rfile, 0);
    assert_not_null(rblock);
    size_t craw = 0;
    const void *raw = asdf_block_data_raw(rblock, &craw); /* compressed bytes */
    assert_not_null(raw);
    assert_int(asdf_block_data_size(rblock), ==, n);
    uint8_t *rawcopy = malloc(craw);
    assert_not_null(rawcopy);
    memcpy(rawcopy, raw, craw);
    asdf_block_close(rblock);
    asdf_close(rfile);
    free(buf);

    /* Build a new file with a verbatim compressed block from those raw bytes */
    asdf_file_t *file2 = asdf_open_ex(NULL, 0, (asdf_config_t *)&block_only_config);
    assert_not_null(file2);
    asdf_block_t *block2 = asdf_block_create(file2);
    assert_not_null(block2);
    assert_int(asdf_block_data_set_compressed(block2, rawcopy, craw, n, "zlib"), ==, 0);
    assert_not_null(asdf_block_append(file2, block2));
    asdf_block_close(block2);

    void *buf2 = NULL;
    size_t size2 = 0;
    assert_int(asdf_write_to(file2, &buf2, &size2), ==, 0);
    asdf_close(file2);
    free(rawcopy);

    /* Read it back and verify the decompressed data matches the original */
    asdf_file_t *r2 = asdf_open_mem(buf2, size2);
    assert_not_null(r2);
    asdf_block_t *b2 = asdf_block_open(r2, 0);
    assert_not_null(b2);
    assert_string_equal(asdf_block_compression(b2), "zlib");
    assert_int(asdf_block_data_size(b2), ==, n);
    size_t rlen = 0;
    const uint8_t *rdata = asdf_block_data(b2, &rlen);
    assert_int(rlen, ==, n);

    for (size_t idx = 0; idx < n; idx++)
        assert_int(rdata[idx], ==, (uint8_t)(idx * 7));

    asdf_block_close(b2);
    asdf_close(r2);
    free(buf2);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    block,
    MU_RUN_TEST(block_data),
    MU_RUN_TEST(block_create_set_append),
    MU_RUN_TEST(block_data_alloc_roundtrip),
    MU_RUN_TEST(block_create_destroy),
    MU_RUN_TEST(block_compress_on_write),
    MU_RUN_TEST(block_data_set_compressed)
);


MU_RUN_SUITE(block);
