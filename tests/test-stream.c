#include "munit.h"
#include "util.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "block.h"
#include "stream.h"
#include "stream_intern.h"


#define TOKEN(t) ((const uint8_t *)(t))
#define TOKEN_LEN(t) (sizeof(t) - 1)


static const uint8_t *tokens[] = {TOKEN("dummy"), TOKEN("asdf")};
static size_t token_lens[] = {TOKEN_LEN("dummy"), TOKEN_LEN("asdf")};


MU_TEST(test_file_scan_token_no_match) {
    char buffer[] = "fdsa and some other garbage";
    FILE *file = fmemopen(buffer, strlen(buffer), "r");
    asdf_stream_t *stream = asdf_stream_from_fp(NULL, file, NULL);
    size_t match_offset = 0;
    size_t match_idx = 0;
    int ret = asdf_stream_scan(stream, tokens, token_lens, 2, &match_offset, &match_idx);
    assert_int(ret, ==, 1);

    // Assert that asdf_stream_next() returns NULL (we have exhausted the stream scanning)
    size_t avail = 0;
    const uint8_t *r = asdf_stream_next(stream, 0, &avail);
    assert_int(avail, ==, 0);
    assert_null(r);
    asdf_stream_close(stream);
    return MUNIT_OK;
}


MU_TEST(test_file_scan_token_at_beginning) {
    char buffer[] = "asdf and some other garbage";
    FILE *file = fmemopen(buffer, strlen(buffer), "r");
    asdf_stream_t *stream = asdf_stream_from_fp(NULL, file, NULL);
    size_t match_offset = 0;
    size_t match_idx = 0;
    int ret = asdf_stream_scan(stream, tokens, token_lens, 2, &match_offset, &match_idx);
    assert_int(ret, ==, 0);
    assert_int(match_offset, ==, 0);
    assert_int(match_idx, ==, 1);

    // Assert that asdf_stream_next() returns the position of the matched token
    size_t avail = 0;
    const uint8_t *r = asdf_stream_next(stream, 0, &avail);
    assert_int(memcmp(r, "asdf", 4), ==, 0);
    asdf_stream_close(stream);
    return MUNIT_OK;
}


MU_TEST(test_file_scan_token_at_end) {
    char buffer[] = "and some other garbage asdf";
    FILE *file = fmemopen(buffer, strlen(buffer), "r");
    asdf_stream_t *stream = asdf_stream_from_fp(NULL, file, NULL);
    size_t match_offset = 0;
    size_t match_idx = 0;
    int ret = asdf_stream_scan(stream, tokens, token_lens, 2, &match_offset, &match_idx);
    assert_int(ret, ==, 0);
    assert_int(match_offset, ==, 23);
    assert_int(match_idx, ==, 1);
 
    // Assert that asdf_stream_next() returns the position of the matched token
    size_t avail = 0;
    const uint8_t *r = asdf_stream_next(stream, 0, &avail);
    assert_int(memcmp(r, "asdf", 4), ==, 0);
    assert_int(avail, ==, 4);
    asdf_stream_close(stream);
    return MUNIT_OK;
}


MU_TEST(test_file_scan_token_in_middle) {
    char buffer[] = "fdsa and some asdf other garbage";
    FILE *file = fmemopen(buffer, strlen(buffer), "r");
    asdf_stream_t *stream = asdf_stream_from_fp(NULL, file, NULL);
    size_t match_offset = 0;
    size_t match_idx = 0;
    int ret = asdf_stream_scan(stream, tokens, token_lens, 2, &match_offset, &match_idx);
    assert_int(ret, ==, 0);
    assert_int(match_offset, ==, 14);
    assert_int(match_idx, ==, 1);
 
    // Assert that asdf_stream_next() returns the position of the matched token
    size_t avail = 0;
    const uint8_t *r = asdf_stream_next(stream, 0, &avail);
    assert_int(memcmp(r, "asdf", 4), ==, 0);
    assert_int(avail, ==, 18);
    asdf_stream_close(stream);
    return MUNIT_OK;
}


MU_TEST(test_file_scan_token_spans_buffers) {
    char buffer[] = "fdsa and some asdf other garbage";

    // Hack buf_size so that it only reads up to part-way into the token to match
    // TODO item is still to make an option to configure the read buffer size explicitly
    for (size_t buf_size = 15; buf_size < 18; buf_size++) {
        FILE *file = fmemopen(buffer, strlen(buffer), "r");
        asdf_stream_t *stream = asdf_stream_from_fp(NULL, file, NULL);
        file_userdata_t *data = stream->userdata;
        data->buf_size = buf_size;

        size_t match_offset = 0;
        size_t match_idx = 0;
        int ret = asdf_stream_scan(stream, tokens, token_lens, 2, &match_offset, &match_idx);
        assert_int(ret, ==, 0);
        assert_int(match_offset, ==, 14);
        assert_int(match_idx, ==, 1);
 
        // Assert that asdf_stream_next() returns the position of the matched token
        size_t avail = 0;
        const uint8_t *r = asdf_stream_next(stream, 0, &avail);
        assert_int(memcmp(r, "asdf", 4), ==, 0);
        // At first glance this seems wrong, but under the circumstances it's actually
        // right: The buffer will contain buf_size bytes, but the position of the match in
        // the buffer will depend on how much of the buffer window had to be shifted to find
        // the match
        assert_int(avail, ==, data->buf_avail - data->buf_pos);
        asdf_stream_close(stream);
    }

    return MUNIT_OK;
}


MU_TEST(test_stream_file_open_mem) {
    const char *filename = get_fixture_file_path("255.asdf");
    asdf_stream_t *stream = asdf_stream_from_file(NULL, filename);
    assert_not_null(stream);
    off_t offset = 0x3bb;  // Known offset of the first block data in this file
    size_t size = 256;  // Known size of the block data in this file
    size_t avail = 0;
    uint8_t *addr = (uint8_t *)stream->open_mem(stream, offset, size, &avail);
    assert_not_null(addr);
    assert_int(avail, ==, size);
    // Test file contains the integers 0 to 255
    for (int idx = 0; idx <= 255; idx++) {
        assert_int(addr[idx], ==, idx);
    }
    assert_int(stream->close_mem(stream, (void *)addr), ==, 0);
    asdf_stream_close(stream);
    return MUNIT_OK;
}


MU_TEST(test_stream_mem_open_mem) {
    const char *filename = get_fixture_file_path("255.asdf");
    size_t filesize = 0;
    char *data = tail_file(filename, 0, &filesize);
    assert_not_null(data);
    asdf_stream_t *stream = asdf_stream_from_memory(NULL, (const void *)data, filesize);
    assert_not_null(stream);
    off_t offset = 0x3bb;  // Known offset of the first block data in this file
    size_t size = 256;  // Known size of the block data in this file
    size_t avail = 0;
    uint8_t *addr = (uint8_t *)stream->open_mem(stream, offset, size, &avail);
    assert_not_null(addr);
    assert_int(avail, ==, size);
    // Test file contains the integers 0 to 255
    for (int idx = 0; idx <= 255; idx++) {
        assert_int(addr[idx], ==, idx);
    }
    assert_int(stream->close_mem(stream, (void *)addr), ==, 0);
    asdf_stream_close(stream);
    free(data);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_stream,
    MU_RUN_TEST(test_file_scan_token_at_beginning),
    MU_RUN_TEST(test_file_scan_token_at_end),
    MU_RUN_TEST(test_file_scan_token_in_middle),
    MU_RUN_TEST(test_file_scan_token_spans_buffers),
    MU_RUN_TEST(test_stream_file_open_mem),
    MU_RUN_TEST(test_stream_mem_open_mem)
);


MU_RUN_SUITE(test_asdf_stream);
