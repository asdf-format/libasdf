#ifndef _GNU_SOURCE
#define _GNU_SOURCE  /* for memmem */
#endif
#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "asdf/core/ndarray.h"

#include "compression/compression.h"
#include "config.h"
#include "file.h"

#include "munit.h"
#include "util.h"


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
    const int64_t *dst = asdf_ndarray_data(ndarray, &size);
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
    const uint8_t *dst = asdf_ndarray_data(ndarray, &size);
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
    asdf_ndarray_data(ndarray, NULL);
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
    const uint8_t *data = asdf_ndarray_data(ndarray, NULL);
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
 * Write a small, highly-compressible ndarray with a given compressor and
 * verify the round-trip.
 *
 * Also verifies that the compressed file is strictly smaller than an
 * uncompressed version.
 */
MU_TEST(write_compressed_ndarray) {
    const char *comp = munit_parameters_get(params, "comp");

    /* 4 KiB of a short repeating pattern: highly compressible */
    const size_t n = 4096;
    const uint64_t shape[] = {n};

    uint8_t *ref = malloc(n);
    if (!ref)
        return MUNIT_ERROR;
    for (size_t idx = 0; idx < n; idx++)
        ref[idx] = (uint8_t)(idx % 4);

    char comp_suffix[32], nocomp_suffix[32];
    snprintf(comp_suffix, sizeof(comp_suffix), "%s-comp.asdf", comp);
    snprintf(nocomp_suffix, sizeof(nocomp_suffix), "%s-nocomp.asdf", comp);
    const char *comp_path = strdup(get_temp_file_path(fixture->tempfile_prefix, comp_suffix));
    const char *nocomp_path = strdup(get_temp_file_path(fixture->tempfile_prefix, nocomp_suffix));

    /* Write compressed version to a temp file */
    {
        asdf_ndarray_t ndarray = {
            .datatype = (asdf_datatype_t){.type = ASDF_DATATYPE_UINT8},
            .byteorder = ASDF_BYTEORDER_BIG,
            .ndim = 1,
            .shape = shape,
        };
        uint8_t *data = asdf_ndarray_data_alloc(&ndarray);
        if (!data) { free(ref); return MUNIT_ERROR; }
        memcpy(data, ref, n);
        assert_int(asdf_ndarray_compression_set(&ndarray, comp), ==, 0);
        asdf_file_t *file = asdf_open(NULL);
        assert_not_null(file);
        asdf_value_t *value = asdf_value_of_ndarray(file, &ndarray);
        assert_not_null(value);
        assert_int(asdf_set_value(file, "data", value), ==, ASDF_VALUE_OK);
        assert_int(asdf_write_to(file, comp_path), ==, 0);
        asdf_close(file);
        asdf_ndarray_data_dealloc(&ndarray);
    }

    /* Write uncompressed version for size comparison */
    {
        asdf_ndarray_t ndarray = {
            .datatype = (asdf_datatype_t){.type = ASDF_DATATYPE_UINT8},
            .byteorder = ASDF_BYTEORDER_BIG,
            .ndim = 1,
            .shape = shape,
        };
        uint8_t *data = asdf_ndarray_data_alloc(&ndarray);
        if (!data) { free(ref); return MUNIT_ERROR; }
        memcpy(data, ref, n);
        asdf_file_t *file = asdf_open(NULL);
        assert_not_null(file);
        asdf_value_t *value = asdf_value_of_ndarray(file, &ndarray);
        assert_not_null(value);
        assert_int(asdf_set_value(file, "data", value), ==, ASDF_VALUE_OK);
        assert_int(asdf_write_to(file, nocomp_path), ==, 0);
        asdf_close(file);
        asdf_ndarray_data_dealloc(&ndarray);
    }

    /* Compressed file should be strictly smaller */
    struct stat comp_st, nocomp_st;
    assert_int(stat(comp_path, &comp_st), ==, 0);
    assert_int(stat(nocomp_path, &nocomp_st), ==, 0);
    munit_logf(MUNIT_LOG_INFO, "%s: compressed=%lld uncompressed=%lld",
               comp, (long long)comp_st.st_size, (long long)nocomp_st.st_size);
    assert_true(comp_st.st_size < nocomp_st.st_size);

    /* Read back compressed file and verify round-trip */
    asdf_file_t *file = asdf_open_file(comp_path, "r");
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    assert_int(asdf_get_ndarray(file, "data", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    size_t read_size = 0;
    const uint8_t *read_data = asdf_ndarray_data(ndarray, &read_size);
    assert_not_null(read_data);
    assert_size(read_size, ==, n);
    assert_memory_equal(n, read_data, ref);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    free(ref);
    free((void *)comp_path);
    free((void *)nocomp_path);
    return MUNIT_OK;
}


/**
 * Helper: write a small, highly-compressible ndarray with a given compressor
 * to a temp file.  Returns the path (caller must free it), or NULL on error.
 */
static const char *write_compressed_ndarray_to_file(
    const char *comp, const char *prefix, const uint8_t *ref, size_t n) {
    const uint64_t shape[] = {n};
    asdf_ndarray_t ndarray = {
        .datatype = (asdf_datatype_t){.type = ASDF_DATATYPE_UINT8},
        .byteorder = ASDF_BYTEORDER_BIG,
        .ndim = 1,
        .shape = shape,
    };
    uint8_t *data = asdf_ndarray_data_alloc(&ndarray);
    if (!data)
        return NULL;

    memcpy(data, ref, n);

    char suffix[64];
    snprintf(suffix, sizeof(suffix), "%s-write.asdf", comp);
    const char *path = strdup(get_temp_file_path(prefix, suffix));

    if (!path) {
        asdf_ndarray_data_dealloc(&ndarray);
        return NULL;
    }

    if (asdf_ndarray_compression_set(&ndarray, comp) != 0) {
        free((void *)path);
        asdf_ndarray_data_dealloc(&ndarray);
        return NULL;
    }

    asdf_file_t *file = asdf_open(NULL);
    if (!file) {
        free((void *)path);
        asdf_ndarray_data_dealloc(&ndarray);
        return NULL;
    }

    asdf_value_t *value = asdf_value_of_ndarray(file, &ndarray);
    if (!value || asdf_set_value(file, "data", value) != ASDF_VALUE_OK ||
        asdf_write_to(file, path) != 0) {
        asdf_close(file);
        asdf_ndarray_data_dealloc(&ndarray);
        free((void *)path);
        return NULL;
    }

    asdf_close(file);
    asdf_ndarray_data_dealloc(&ndarray);
    return path;
}


/**
 * Re-emit an already-compressed block verbatim (no decompression/recompression)
 *
 * Regression test for the write path with opened blocks:
 *  1. Write compressed ndarray to a temp file.
 *  2. Re-open the file and write it to a second temp file without touching any blocks.
 *  3. Verify the second file round-trips correctly.
 */
MU_TEST(reemit_compressed_verbatim) {
    const char *comp = munit_parameters_get(params, "comp");
    const size_t n = 4096;

    uint8_t *data = malloc(n);

    if (!data)
        return MUNIT_ERROR;
    for (size_t idx = 0; idx < n; idx++)
        data[idx] = (uint8_t)(idx % 4);

    const char *first_path = write_compressed_ndarray_to_file(
        comp, fixture->tempfile_prefix, data, n);

    if (!first_path) {
        free(data);
        return MUNIT_ERROR;
    }

    char suffix2[64];
    snprintf(suffix2, sizeof(suffix2), "%s-verbatim.asdf", comp);
    const char *second_path = strdup(get_temp_file_path(fixture->tempfile_prefix, suffix2));

    if (!second_path) {
        free(data);
        free((void *)first_path);
        return MUNIT_ERROR;
    }

    asdf_file_t *file = asdf_open_file(first_path, "r");
    assert_not_null(file);
    assert_int(asdf_write_to(file, second_path), ==, 0);
    asdf_close(file);

    file = asdf_open_file(second_path, "r");
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    assert_int(asdf_get_ndarray(file, "data", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    size_t read_size = 0;
    const uint8_t *read_data = asdf_ndarray_data(ndarray, &read_size);
    assert_not_null(read_data);
    assert_size(read_size, ==, n);
    assert_memory_equal(n, read_data, data);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);

    free(data);
    free((void *)first_path);
    free((void *)second_path);
    return MUNIT_OK;
}


/**
 * Re-write an opened block with a different compressor
 *
 * Regression test for the write path with opened blocks:
 *  1. Write a zlib-compressed ndarray to a temp file (choice of zlib here is arbitrary).
 *  2. Re-open the file, change the block's compressor to bzp2, write to a second file
 *     (choice of bzp2 also arbitrary--point is to ensure it is changed from zlib).
 *  3. Verify the second file round-trips correctly and uses bzp2 compression
 */
MU_TEST(recompress_block) {
    const size_t n = 4096;
    const char *src_comp = "zlib";
    const char *dst_comp = "bzp2";

    uint8_t *data = malloc(n);

    if (!data)
        return MUNIT_ERROR;

    for (size_t idx = 0; idx < n; idx++)
        data[idx] = (uint8_t)(idx % 4);

    const char *first_path = write_compressed_ndarray_to_file(
        src_comp, fixture->tempfile_prefix, data, n);

    if (!first_path) {
        free(data);
        return MUNIT_ERROR;
    }

    const char *second_path = strdup(get_temp_file_path(
        fixture->tempfile_prefix, "recompress.asdf"));

    if (!second_path) {
        free(data);
        free((void *)first_path);
        return MUNIT_ERROR;
    }

    asdf_file_t *file = asdf_open_file(first_path, "r");
    assert_not_null(file);

    /* Find the block for the ndarray and set the new compressor */
    asdf_ndarray_t *ndarray = NULL;
    assert_int(asdf_get_ndarray(file, "data", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    size_t block_idx = ndarray->source;
    asdf_ndarray_destroy(ndarray);

    asdf_block_t *block = asdf_block_open(file, block_idx);
    assert_not_null(block);
    assert_int(asdf_block_compression_set(block, dst_comp), ==, 0);
    asdf_block_close(block);

    assert_int(asdf_write_to(file, second_path), ==, 0);
    asdf_close(file);

    /* open second file and verify round-trip and compression */
    file = asdf_open_file(second_path, "r");
    assert_not_null(file);
    ndarray = NULL;
    assert_int(asdf_get_ndarray(file, "data", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    size_t read_size = 0;
    const uint8_t *read_data = asdf_ndarray_data(ndarray, &read_size);
    assert_not_null(read_data);
    assert_size(read_size, ==, n);
    assert_memory_equal(n, read_data, data);

    block = asdf_block_open(file, ndarray->source);
    assert_not_null(block);
    const char *written_comp = asdf_block_compression(block);
    assert_string_equal(written_comp, dst_comp);
    asdf_block_close(block);

    asdf_ndarray_destroy(ndarray);
    asdf_close(file);

    free(data);
    free((void *)first_path);
    free((void *)second_path);
    return MUNIT_OK;
}


/**
 * Access a compressed block's data, then re-write the file
 *
 * Regression test for the write path with opened blocks:
 *  1. Write a compressed ndarray to a temp file.
 *  2. Re-open the file, open and read the block's data, close the block.
 *  3. Write the file to a second temp file.
 *  4. Verify the second file round-trips correctly.
 */
MU_TEST(access_then_write) {
    const char *comp = munit_parameters_get(params, "comp");
    const size_t n = 4096;

    uint8_t *data = malloc(n);

    if (!data)
        return MUNIT_ERROR;

    for (size_t idx = 0; idx < n; idx++)
        data[idx] = (uint8_t)(idx % 4);

    const char *first_path = write_compressed_ndarray_to_file(
        comp, fixture->tempfile_prefix, data, n);

    if (!first_path) {
        free(data);
        return MUNIT_ERROR;
    }

    /* re-open, access block data, close block */
    char suffix2[64];
    snprintf(suffix2, sizeof(suffix2), "%s-access.asdf", comp);
    const char *second_path = strdup(get_temp_file_path(fixture->tempfile_prefix, suffix2));

    if (!second_path) {
        free(data);
        free((void *)first_path);
        return MUNIT_ERROR;
    }

    asdf_file_t *file = asdf_open_file(first_path, "r");
    assert_not_null(file);

    asdf_ndarray_t *ndarray = NULL;
    assert_int(asdf_get_ndarray(file, "data", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    size_t block_idx = ndarray->source;
    asdf_ndarray_destroy(ndarray);

    asdf_block_t *block = asdf_block_open(file, block_idx);
    assert_not_null(block);
    /* Access the data (triggers decompression) */
    assert_not_null(asdf_block_data(block, NULL));
    asdf_block_close(block);

    /* write to second file */
    assert_int(asdf_write_to(file, second_path), ==, 0);
    asdf_close(file);

    /* open second file and verify round-trip */
    file = asdf_open_file(second_path, "r");
    assert_not_null(file);
    ndarray = NULL;
    assert_int(asdf_get_ndarray(file, "data", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    size_t read_size = 0;
    const uint8_t *read_data = asdf_ndarray_data(ndarray, &read_size);
    assert_not_null(read_data);
    assert_size(read_size, ==, n);
    assert_memory_equal(n, read_data, data);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);

    free(data);
    free((void *)first_path);
    free((void *)second_path);
    return MUNIT_OK;
}


MU_TEST(verify_checksum) {
    const char *comp = munit_parameters_get(params, "comp");
    const size_t n = 4096;

    uint8_t *data = malloc(n);

    if (!data)
        return MUNIT_ERROR;

    for (size_t idx = 0; idx < n; idx++)
        data[idx] = (uint8_t)(idx % 4);

    const char *path = write_compressed_ndarray_to_file(
        comp, fixture->tempfile_prefix, data, n);

    if (!path) {
        free(data);
        return MUNIT_ERROR;
    }

    /* re-open, verify checksum, close block */
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_block_t *block = asdf_block_open(file, 0);
    assert_not_null(block);
    const unsigned char *checksum = asdf_block_checksum(block);
    assert_not_null(checksum);
    unsigned char *empty = calloc(ASDF_BLOCK_CHECKSUM_DIGEST_SIZE, sizeof(unsigned char));
    
    if (!empty)
        return MUNIT_ERROR;

    assert_memory_not_equal(ASDF_BLOCK_CHECKSUM_DIGEST_SIZE, checksum, empty);
    free(empty);
    assert_true(asdf_block_checksum_verify(block, NULL));
    asdf_block_close(block);
    asdf_close(file);

    free(data);
    free((void *)path);
    return MUNIT_OK;
}


/**
 * Write a compressed ndarray to a memory buffer and verify the round-trip
 */
MU_TEST(write_compressed_to_mem) {
    const char *comp = munit_parameters_get(params, "comp");
    const size_t n = 4096;

    uint8_t *ref = malloc(n);
    if (!ref)
        return MUNIT_ERROR;
    for (size_t idx = 0; idx < n; idx++)
        ref[idx] = (uint8_t)(idx % 4);

    const uint64_t shape[] = {n};
    asdf_ndarray_t ndarray = {
        .datatype = (asdf_datatype_t){.type = ASDF_DATATYPE_UINT8},
        .byteorder = ASDF_BYTEORDER_BIG,
        .ndim = 1,
        .shape = shape,
    };
    uint8_t *data = asdf_ndarray_data_alloc(&ndarray);

    if (!data) {
        free(ref);
        return MUNIT_ERROR;
    }

    memcpy(data, ref, n);
    assert_int(asdf_ndarray_compression_set(&ndarray, comp), ==, 0);

    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    asdf_value_t *value = asdf_value_of_ndarray(file, &ndarray);
    assert_not_null(value);
    assert_int(asdf_set_value(file, "data", value), ==, ASDF_VALUE_OK);

    void *buf = NULL;
    size_t size = 0;
    assert_int(asdf_write_to(file, &buf, &size), ==, 0);
    asdf_close(file);
    asdf_ndarray_data_dealloc(&ndarray);

    assert_not_null(buf);
    assert_size(size, >, 0);

    asdf_file_t *rfile = asdf_open_mem_ex(buf, size, NULL);
    assert_not_null(rfile);
    const char *error = asdf_error(rfile);
    if (error)
        munit_logf(MUNIT_LOG_ERROR, "error opening file from buffer: %s", error);
    assert_null(error);
    asdf_ndarray_t *rndarray = NULL;
    assert_int(asdf_get_ndarray(rfile, "data", &rndarray), ==, ASDF_VALUE_OK);
    assert_not_null(rndarray);
    size_t rsize = 0;
    const uint8_t *rdata = asdf_ndarray_data(rndarray, &rsize);
    assert_not_null(rdata);
    assert_size(rsize, ==, n);
    assert_memory_equal(n, rdata, ref);
    asdf_ndarray_destroy(rndarray);
    asdf_close(rfile);
    free(buf);
    free(ref);
    return MUNIT_OK;
}


/**
 * Regression test for the asdf_write_to_mem stream-switch + realloc optimization (issue #187)
 *
 * A large YAML tree (> 2 * _SC_PAGE_SIZE, i.e. > 8 KiB) combined with a
 * non-compressed block that makes blocks_size > 0 will trigger the code path in
 * asdf_write_to_mem that reallocates the buffer and switches the emitter stream
 * mid-emission.  Before the fix the emitter state was reset to INITIAL causing
 * the blocks to overwrite the YAML in the buffer, producing a corrupt file.
 */
MU_TEST(write_to_mem_large_tree_realloc) {
    /* 16 KiB uncompressed block so blocks_size > 0, which is needed for the
     * realloc optimisation to trigger.  Initial allocation will be
     * 2*PAGE (8 KiB) + blocks_size (~16 KiB) = ~24 KiB.  The YAML tree
     * of ~14 KiB fits within the initial allocation, but the remaining space
     * after writing the tree (~10 KiB) is less than blocks_size (~16 KiB),
     * so the realloc path is taken. */
    const size_t n = 16384;
    const uint64_t shape[] = {n};

    uint8_t *ref = malloc(n);
    if (!ref)
        return MUNIT_ERROR;
    for (size_t idx = 0; idx < n; idx++)
        ref[idx] = (uint8_t)(idx % 251);

    asdf_ndarray_t ndarray = {
        .datatype = (asdf_datatype_t){.type = ASDF_DATATYPE_UINT8},
        .byteorder = ASDF_BYTEORDER_BIG,
        .ndim = 1,
        .shape = shape,
    };
    uint8_t *data = asdf_ndarray_data_alloc(&ndarray);

    if (!data) {
        free(ref);
        return MUNIT_ERROR;
    }

    memcpy(data, ref, n);

    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    asdf_value_t *value = asdf_value_of_ndarray(file, &ndarray);
    assert_not_null(value);
    assert_int(asdf_set_value(file, "rawdata", value), ==, ASDF_VALUE_OK);

    /* Add 100 padding entries of 127 bytes each to push YAML past 2 * PAGE (8 KiB) */
    char key[32];
    char val[128];
    memset(val, 'x', sizeof(val) - 1);
    val[sizeof(val) - 1] = '\0';

    for (int idx = 0; idx < 100; idx++) {
        snprintf(key, sizeof(key), "padding_%03d", idx);
        assert_int(asdf_set_string0(file, key, val), ==, ASDF_VALUE_OK);
    }

    void *buf = NULL;
    size_t size = 0;
    assert_int(asdf_write_to(file, &buf, &size), ==, 0);
    asdf_ndarray_data_dealloc(&ndarray);
    asdf_close(file);

    assert_not_null(buf);
    assert_size(size, >, 0);

    asdf_file_t *rfile = asdf_open_mem_ex(buf, size, NULL);
    assert_not_null(rfile);
    const char *error = asdf_error(rfile);
    if (error)
        munit_logf(MUNIT_LOG_ERROR, "error opening file from buffer: %s", error);
    assert_null(error);
    asdf_ndarray_t *rndarray = NULL;
    assert_int(asdf_get_ndarray(rfile, "rawdata", &rndarray), ==, ASDF_VALUE_OK);
    assert_not_null(rndarray);
    size_t rsize = 0;
    const uint8_t *rdata = asdf_ndarray_data(rndarray, &rsize);
    assert_not_null(rdata);
    assert_size(rsize, ==, n);
    assert_memory_equal(n, rdata, ref);
    asdf_ndarray_destroy(rndarray);
    asdf_close(rfile);
    free(buf);
    free(ref);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    compression,
    MU_RUN_TEST(write_compressed_ndarray, comp_test_params),
    MU_RUN_TEST(read_compressed_reference_file, comp_mode_test_params),
    MU_RUN_TEST(read_compressed_block, comp_mode_test_params),
    MU_RUN_TEST(read_compressed_block_to_file, comp_test_params),
    MU_RUN_TEST(read_compressed_block_to_file_on_threshold, comp_test_params),
    MU_RUN_TEST(open_close_compressed_block, comp_mode_test_params),
    MU_RUN_TEST(read_compressed_block_lazy_random_access, comp_mode_test_params),
    MU_RUN_TEST(compressed_block_no_hang_on_segfault, comp_mode_test_params),
    MU_RUN_TEST(reemit_compressed_verbatim, comp_test_params),
    MU_RUN_TEST(recompress_block),
    MU_RUN_TEST(access_then_write, comp_test_params),
    MU_RUN_TEST(write_compressed_to_mem, comp_test_params),
    MU_RUN_TEST(write_to_mem_large_tree_realloc)
);


MU_RUN_SUITE(compression);
