#include <errno.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#include <asdf/file.h>
#include <asdf/core/ndarray.h>
#include <asdf/value.h>

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
MU_TEST(test_asdf_scalar_getters) {
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
    asdf_value_t *value = NULL;
    asdf_value_err_t err = asdf_get_mapping(file, "mapping", &value);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_true(asdf_value_is_mapping(value));
    asdf_value_destroy(value);
    err = asdf_get_mapping(file, "scalar", &value);
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
    asdf_value_t *value = NULL;
    asdf_value_err_t err = asdf_get_sequence(file, "sequence", &value);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_true(asdf_value_is_sequence(value));
    asdf_value_destroy(value);
    err = asdf_get_sequence(file, "scalar", &value);
    assert_int(err, ==, ASDF_VALUE_ERR_TYPE_MISMATCH);
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
MU_TEST(test_asdf_read_compressed_reference_file) {
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
    int64_t *dst = asdf_ndarray_data_raw(ndarray, &size);
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
    uint8_t *dst = asdf_ndarray_data_raw(ndarray, &size);
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


MU_TEST(test_asdf_read_compressed_block) {
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
MU_TEST(test_asdf_read_compressed_block_to_file) {
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
MU_TEST(test_asdf_read_compressed_block_to_file_on_threshold) {
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
MU_TEST(test_asdf_open_close_compressed_block) {
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


/* Used for test_asdf_compressed_block_no_hang_on_segfault
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


MU_TEST(test_asdf_compressed_block_no_hang_on_segfault) {
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
    uint8_t *data = asdf_ndarray_data_raw(ndarray, NULL);
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
MU_TEST(test_asdf_read_compressed_block_lazy_random_access) {
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


MU_TEST_SUITE(
    test_asdf_file,
    MU_RUN_TEST(test_asdf_open_file),
    MU_RUN_TEST(test_asdf_scalar_getters),
    MU_RUN_TEST(test_asdf_get_mapping),
    MU_RUN_TEST(test_asdf_get_sequence),
    MU_RUN_TEST(test_asdf_block_count),
    MU_RUN_TEST(test_asdf_read_compressed_reference_file, comp_mode_test_params),
    MU_RUN_TEST(test_asdf_read_compressed_block, comp_mode_test_params),
    MU_RUN_TEST(test_asdf_read_compressed_block_to_file, comp_test_params),
    MU_RUN_TEST(test_asdf_read_compressed_block_to_file_on_threshold, comp_test_params),
    MU_RUN_TEST(test_asdf_open_close_compressed_block, comp_mode_test_params),
    MU_RUN_TEST(test_asdf_read_compressed_block_lazy_random_access, comp_mode_test_params),
    MU_RUN_TEST(test_asdf_compressed_block_no_hang_on_segfault, comp_mode_test_params)
);


MU_RUN_SUITE(test_asdf_file);
