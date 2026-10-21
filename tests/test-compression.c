#ifndef _GNU_SOURCE
#define _GNU_SOURCE  /* for memmem */
#endif
#include <errno.h>
#include <pthread.h>
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

#if defined(HAVE_USERFAULTFD) && defined(HAVE_DECL_SYS_USERFAULTFD)
#include <fcntl.h>
#include <linux/userfaultfd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>

#ifndef UFFD_USER_MODE_ONLY
/* Missing from older Linux headers; see the same fallback in compression.c */
#define UFFD_USER_MODE_ONLY 1
#endif
#endif

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
static char *mode_params[] = {"auto", "eager", "lazy", NULL};
#else
static char *mode_params[] = {"auto", "eager", NULL};
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


/**
 * Lazy decompression chunk sizes, in pages, or "default" to leave
 * decomp.chunk_size unset
 */
static char *chunk_params[] = {"1", "4", "default", NULL};
static MunitParameterEnum comp_mode_chunk_test_params[] = {
    {"comp", comp_params},
    {"mode", mode_params},
    {"chunk", chunk_params},
    {NULL, NULL}
};


static size_t chunk_size_from_param(const char *chunk) {
    if (strcmp(chunk, "default") == 0)
        return 0;

    return strtoul(chunk, NULL, 10) * (size_t)sysconf(_SC_PAGESIZE);
}


static asdf_block_decomp_mode_t decomp_mode_from_param(const char *mode) {
    if (strcmp(mode, "auto") == 0)
        return ASDF_BLOCK_DECOMP_MODE_AUTO;

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

        // Always read page 1 first: with lazy decompression in chunks of
        // more than one page, the first page fault is then guaranteed not to
        // be on the first page of a chunk, ensuring better determinism in
        // failure modes. The bug in #274 is triggered particularly when the
        // decompression chunk_size is > page_size, and the first fault falls
        // on a page other than the first (0-th) page within that chunk.
        for (int idx = 0; idx < num_pages; idx++) {
            if (pages[idx] == 1) {
                pages[idx] = pages[0];
                pages[0] = 1;
                break;
            }
        }

        for (int idx = 0; idx < num_pages; idx++) {
            size_t page_idx = pages[idx];
            //munit_logf(MUNIT_LOG_DEBUG, "checking page %zu\n", page_idx);
            assert_memory_equal(
                page_size, dst + (page_idx * page_size), expected + (page_idx * page_size));
        }

        free(pages);
    }

    // In lazy mode errors in the decompression handler only occur once the
    // data is actually read, so check again
    error = asdf_error(file);
    if (error)
        munit_logf(MUNIT_LOG_ERROR, "error after reading the ndarray data: %s", error);
    assert_null(error);

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
 * Read compressed arrays with an offset into their (uncompressed) block data
 *
 * Each array in offset.asdf is a view starting 17 bytes (arbitrarily) into
 * the 4th page (also arbitrary) of same paged pattern used in compressed.asdf,
 * so the view's data starts past several pages that must be decompressed first
 * (in lazy mode), and the 5th page's canary byte appears at index 4096 - 17 of
 * the view.
 */
MU_TEST(read_compressed_ndarray_offset) {
    const char *comp = munit_parameters_get(params, "comp");
    const char *filename = get_fixture_file_path("offset.asdf");
    asdf_config_t config = {
        .decomp = {
            .mode = decomp_mode_from_param(munit_parameters_get(params, "mode"))
        }
    };
    asdf_file_t *file = asdf_open_file_ex(filename, "r", &config);
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    assert_int(asdf_get_ndarray(file, comp, &ndarray), ==, ASDF_VALUE_OK);

    const size_t page_size = 4096;
    const size_t num_pages = 100;
    const size_t offset = (3 * page_size) + 17;
    const size_t nelem = (page_size * num_pages) - offset;
    assert_int(ndarray->offset, ==, offset);
    assert_int(ndarray->shape[0], ==, nelem);

    uint8_t *expected = malloc(nelem);

    if (!expected)
        return MUNIT_ERROR;

    for (size_t idx = 0; idx < nelem; idx++) {
        size_t base_idx = idx + offset;

        if (base_idx % page_size == 0)
            expected[idx] = (base_idx / page_size) % 256;
        else
            expected[idx] = base_idx % 256;
    }

    size_t size = 0;
    const uint8_t *data = asdf_ndarray_data(ndarray, &size);
    assert_not_null(data);
    assert_null(asdf_error(file));
    assert_size(size, ==, nelem);
    assert_uint8(data[0], ==, 17);
    assert_uint8(data[page_size - 17], ==, 4);
    assert_memory_equal(size, data, expected);

    /* The raw data is the whole compressed block, without the offset */
    size_t raw_size = 0;
    size_t block_raw_size = 0;
    const void *raw = asdf_ndarray_data_raw(ndarray, &raw_size);
    assert_not_null(raw);
    asdf_block_t *block = asdf_ndarray_block(ndarray);
    assert_not_null(block);
    assert_ptr_equal(raw, asdf_block_data_raw(block, &block_raw_size));
    assert_size(raw_size, ==, block_raw_size);
    assert_size(raw_size, <, nelem);

    /* read_all, etc. returns the data starting from the offset */
    void *buf = NULL;
    assert_int(asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_SOURCE, &buf), ==, ASDF_NDARRAY_OK);
    assert_memory_equal(nelem, buf, expected);
    free(buf);

    asdf_ndarray_err_t err = ASDF_NDARRAY_ERR_INVAL;
    assert_uint8(asdf_ndarray_at_err(ndarray, uint8_t, &err, (page_size * 50) - offset), ==, 50);
    assert_int(err, ==, ASDF_NDARRAY_OK);

    free(expected);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
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


/*
 * Probes for whether lazy decompression is possible on this system
 *
 * These mirror the library's internal (and not exported) runtime checks, so
 * that the tests can determine which decompression mode the library should
 * select.
 */
#if defined(HAVE_USERFAULTFD) && defined(HAVE_DECL_SYS_USERFAULTFD)
/**
 * Open a userfaultfd and perform the API handshake; returns -1 on failure
 *
 * If ``ioctls`` is non-``NULL`` it receives the bitmask of ioctls supported
 * by the kernel, as returned by the handshake.
 */
static int open_uffd(uint64_t *ioctls) {
    struct uffdio_api uffd_api = {.api = UFFD_API};
    long uffd = syscall(SYS_userfaultfd, O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY);

    // Pre-5.11 kernels reject the unknown flag; retry without it
    if (uffd < 0 && errno == EINVAL)
        uffd = syscall(SYS_userfaultfd, O_CLOEXEC | O_NONBLOCK);

    if (uffd < 0)
        return -1;

    if (ioctl((int)uffd, UFFDIO_API, &uffd_api) == -1) {
        close((int)uffd);
        return -1;
    }

    if (ioctls)
        *ioctls = uffd_api.ioctls;

    return (int)uffd;
}


/**
 * Test whether the kernel supports lazy decompression with userfaultfd
 */
static bool lazy_available(void) {
    uint64_t ioctls = 0;
    int uffd = open_uffd(&ioctls);

    if (uffd < 0)
        return false;

    close(uffd);
    // The ioctls bitmask is indexed by the _UFFDIO_* bit numbers
    return (ioctls & (1ULL << _UFFDIO_REGISTER)) != 0;
}


/**
 * Test whether userfaultfd can handle a mapping of a temp file in the default
 * temp dir
 *
 * This generally only succeeds if the temp dir is on a memory-backed
 * filesystem such as tmpfs.  The result determines whether an explicit
 * request for lazy decompression takes precedence over decompressing to a
 * temp file.
 */
static bool lazy_file_backing_available(void) {
    size_t page_size = sysconf(_SC_PAGESIZE);
    const char *tmp_dir = getenv("ASDF_TMPDIR");
    char path[4096];
    bool available = false;
    int uffd = -1;
    int fd = -1;
    void *map = MAP_FAILED;
    struct uffdio_register uffd_reg = {0};

    if (!tmp_dir || !tmp_dir[0])
        tmp_dir = getenv("TMPDIR");

    if (!tmp_dir || !tmp_dir[0])
        tmp_dir = "/tmp";

    uffd = open_uffd(NULL);

    if (uffd < 0)
        goto finish;

    snprintf(path, sizeof(path), "%s/libasdf-test-XXXXXX", tmp_dir);
    fd = mkstemp(path);

    if (fd < 0)
        goto finish;

    unlink(path);

    if (ftruncate(fd, (off_t)page_size) != 0)
        goto finish;

    map = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (map == MAP_FAILED)
        goto finish;

    uffd_reg.range.start = (uintptr_t)map;
    uffd_reg.range.len = page_size;
    uffd_reg.mode = UFFDIO_REGISTER_MODE_MISSING;
    available = ioctl(uffd, UFFDIO_REGISTER, &uffd_reg) == 0;

finish:
    if (map != MAP_FAILED)
        munmap(map, page_size);

    if (fd >= 0)
        close(fd);

    if (uffd >= 0)
        close(uffd);

    return available;
}
#else
static bool lazy_available(void) {
    return false;
}


static bool lazy_file_backing_available(void) {
    return false;
}
#endif


/**
 * Test that the effective decompression mode is selected correctly
 *
 * Eager mode is always eager; auto and lazy modes are lazy if and only if
 * lazy decompression is available on this system, and otherwise fall back to
 * eager.
 *
 * Regression test for #276.
 */
MU_TEST(decomp_mode_selection) {
    const char *comp = munit_parameters_get(params, "comp");
    const char *filename = get_fixture_file_path("compressed.asdf");
    asdf_block_decomp_mode_t mode = decomp_mode_from_param(munit_parameters_get(params, "mode"));
    asdf_config_t config = {.decomp = {.mode = mode}};
    asdf_block_decomp_mode_t expected = ASDF_BLOCK_DECOMP_MODE_EAGER;

    if (mode != ASDF_BLOCK_DECOMP_MODE_EAGER && lazy_available())
        expected = ASDF_BLOCK_DECOMP_MODE_LAZY;

    asdf_file_t *file = asdf_open_file_ex(filename, "r", &config);
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    assert_int(asdf_get_ndarray(file, comp, &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    assert_not_null(asdf_ndarray_data(ndarray, NULL));
    const asdf_block_t *block = asdf_ndarray_block(ndarray);
    assert_not_null(block);
    assert_not_null(block->comp_state);
    assert_int(block->comp_state->mode, ==, expected);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Test the effective decompression mode when decompressing to a temp file
 *
 * Lazy decompression is incompatible with decompressing to a temp file, so
 * in auto mode decompression to a temp file takes precedence and is eager,
 * whereas in lazy mode lazy decompression (in memory) takes precedence if it
 * is available, including the check that userfaultfd works with the temp
 * dir (see lazy_file_backing_available).
 *
 * Regression test for #276.
 */
MU_TEST(decomp_mode_selection_to_file) {
    const char *comp = munit_parameters_get(params, "comp");
    const char *filename = get_fixture_file_path("compressed.asdf");
    asdf_block_decomp_mode_t mode = decomp_mode_from_param(munit_parameters_get(params, "mode"));
    asdf_config_t config = {.decomp = {.mode = mode, .max_memory_bytes = 1}};
    asdf_block_decomp_mode_t expected = ASDF_BLOCK_DECOMP_MODE_EAGER;
    bool expect_own_fd = true;

    if (mode == ASDF_BLOCK_DECOMP_MODE_LAZY && lazy_available() &&
        lazy_file_backing_available()) {
        expected = ASDF_BLOCK_DECOMP_MODE_LAZY;
        expect_own_fd = false;
    }

    asdf_file_t *file = asdf_open_file_ex(filename, "r", &config);
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    assert_int(asdf_get_ndarray(file, comp, &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    assert_not_null(asdf_ndarray_data(ndarray, NULL));
    const asdf_block_t *block = asdf_ndarray_block(ndarray);
    assert_not_null(block);
    assert_not_null(block->comp_state);
    assert_int(block->comp_state->mode, ==, expected);
    assert_int(block->comp_state->own_fd, ==, expect_own_fd);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
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
 *
 * The pages of the array are read in random order, so with chunks larger
 * than one page the first fault in a chunk is usually not on its first page.
 * With the default chunk size the whole array fits in one chunk.
 *
 * Regression test for #274.
 */
MU_TEST(read_compressed_block_lazy_random_access) {
    const char *comp = munit_parameters_get(params, "comp");
    const char *filename = get_fixture_file_path("compressed.asdf");

    asdf_config_t config = {
        .decomp = {
            // Run the test in eager mode too as a control
            .mode = decomp_mode_from_param(munit_parameters_get(params, "mode")),
            .chunk_size = chunk_size_from_param(munit_parameters_get(params, "chunk"))
        }
    };

    asdf_file_t *file = asdf_open_ex(filename, "r", &config);
    int ret = test_compressed_file(file, comp, false, true);
    asdf_close(file);
    return ret;
}


/*
 * The concurrent fault test needs lazy decompression with userfaultfd (so it
 * is Linux-only anyway) and pthread barriers, which are optional in POSIX and
 * notably missing on macOS; where either is unavailable the test is skipped.
 */
#if defined(HAVE_USERFAULTFD) && defined(HAVE_DECL_SYS_USERFAULTFD) && \
    defined(_POSIX_BARRIERS) && _POSIX_BARRIERS > 0
/** Argument for concurrent_fault_thread */
typedef struct {
    pthread_barrier_t *barrier;
    const volatile uint8_t *addr;
    uint8_t value;
} concurrent_fault_arg_t;


static void *concurrent_fault_thread(void *arg) {
    concurrent_fault_arg_t *fault_arg = arg;
    pthread_barrier_wait(fault_arg->barrier);
    fault_arg->value = *fault_arg->addr;
    return NULL;
}


#define CONCURRENT_FAULT_THREADS 8


/**
 * Test several threads faulting on lazily-decompressed pages at once
 *
 * The data pointer is shared between threads (not the asdf_ndarray_t itself,
 * which is not currently thread-safe) which each read a different
 * not-yet-decompressed page at the same time.  Each of their faults is
 * reported to the decompression handler separately, and whichever it handles
 * first decompresses all pages up to that one.  So faults on earlier pages are
 * generally still queued (in the waiting threads) for pages it has already
 * filled (without waking their threads), and the handler has to cope with
 * them.
 *
 * This is the regression tested here: the UFFDIO_COPY ioctl called from the
 * subsequently awakened threads would get an EEXIST error which was not handled
 * properly.
 *
 * One-page chunks are used so that this is independent of the chunk size
 * handling tested in read_compressed_block_lazy_random_access.
 */
MU_TEST(read_compressed_block_lazy_concurrent_fault) {
    const char *comp = munit_parameters_get(params, "comp");
    const char *filename = get_fixture_file_path("compressed.asdf");

    if (!lazy_available()) {
        munit_log(MUNIT_LOG_INFO, "lazy decompression not available; skipping test...");
        return MUNIT_SKIP;
    }

    asdf_config_t config = {
        .decomp = {
            .mode = ASDF_BLOCK_DECOMP_MODE_LAZY,
            .chunk_size = (size_t)sysconf(_SC_PAGESIZE)
        }
    };
    asdf_file_t *file = asdf_open_ex(filename, "r", &config);
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    assert_int(asdf_get_ndarray(file, comp, &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    size_t size = 0;
    const uint8_t *data = asdf_ndarray_data(ndarray, &size);
    assert_not_null(data);

    // Same data layout as in test_compressed_file: 100 4 KiB "pages" of the
    // repeating pattern 0 to 255, except the first byte of each is its index
    const size_t page_size = 4096;
    const size_t num_pages = 100;
    assert_size(size, ==, page_size * num_pages);
    uint8_t *expected = malloc(size);

    if (!expected)
        return MUNIT_ERROR;

    for (size_t idx = 0; idx < size; idx++)
        expected[idx] = (idx % page_size == 0) ? (idx / page_size) % 256 : idx % 256;

    // Spread the faults over the first part of the array, so that there is
    // still data after them to be decompressed by later faults
    pthread_barrier_t barrier;
    pthread_t threads[CONCURRENT_FAULT_THREADS];
    concurrent_fault_arg_t args[CONCURRENT_FAULT_THREADS];
    assert_int(pthread_barrier_init(&barrier, NULL, CONCURRENT_FAULT_THREADS), ==, 0);

    for (int idx = 0; idx < CONCURRENT_FAULT_THREADS; idx++) {
        size_t fault_offset = (size_t)(idx + 1) * 5 * page_size;
        args[idx] = (concurrent_fault_arg_t){.barrier = &barrier, .addr = data + fault_offset};
        // Each thread is started and blocks at the barrier--once all
        // CONCURRENT_FAULT_THREADS(=8) reach the pthread_barrier_wait call
        // the barrier is unblocked and the first thread to run causes the
        // first page fault and becomes blocked. Subsequent fault_threads can
        // also become blocked at a page fault, with each unhandled fault
        // queued by the kernel as a pending UFFD message sent over the fd.
        // The single fault handler thread reads the queued faults one at a
        // time, sequentially (though in arbitrary order).  Handling a fault
        // on a later page (in terms of offset) will fill prior pages as well,
        // at least in the current sequential decompressors.  But
        // UFFDIO_COPY_MODE_DONTWAKE, which neither wakes the thread waiting
        // on the page fault, nor removes queued faults on other threads.
        // So those faults are still handled afters, for pages that have now
        // already been filled, and the handler must simply wake their threads
        // rather than attempt to fill their faulted pages again (this resulted
        // in an EEXIST error from the UFFDIO_COPY ioctl, causing the fault
        // handler thread to exit, but this is symptom not the cause of the
        // problem).
        assert_int(pthread_create(&threads[idx], NULL, concurrent_fault_thread, &args[idx]), ==, 0);
    }

    for (int idx = 0; idx < CONCURRENT_FAULT_THREADS; idx++) {
        pthread_join(threads[idx], NULL);
        assert_uint8(args[idx].value, ==, *(expected + (args[idx].addr - data)));
    }

    pthread_barrier_destroy(&barrier);

    const char *error = asdf_error(file);
    if (error)
        munit_logf(MUNIT_LOG_ERROR, "error after concurrent faults: %s", error);
    assert_null(error);

    // The rest of the data is still decompressed correctly
    assert_memory_equal(size, data, expected);
    free(expected);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}
#else
MU_TEST(read_compressed_block_lazy_concurrent_fault) {
    munit_log(
        MUNIT_LOG_INFO,
        "lazy decompression or pthread barriers not available; skipping test...");
    return MUNIT_SKIP;
}
#endif


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
 * Test repeated access to the data of a compressed ndarray
 *
 * Regression test for #274.
 *
 * Every access after the first must still return the decompressed data, not
 * the raw compressed bytes of the block.  Each tile read accesses the
 * ndarray's data anew, so reading tiles one after another is the most common
 * way to hit this.
 */
MU_TEST(read_compressed_block_repeated_access) {
    const char *comp = munit_parameters_get(params, "comp");
    const char *filename = get_fixture_file_path("compressed.asdf");
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

    // Same data layout as in test_compressed_file: 100 pages of the repeating
    // pattern 0 to 255, except the first byte of each page is the page index
    const size_t page_size = 4096;
    const size_t num_pages = 100;
    const uint64_t tile_shape[] = {page_size};
    uint64_t origin[] = {0};
    uint8_t expected[4096] = {0};
    uint8_t *tile = NULL;

    for (size_t idx = 0; idx < page_size; idx++)
        expected[idx] = idx % 256;

    // First access, reading the first page
    expected[0] = 0;
    assert_int(
        asdf_ndarray_read_tile_ndim(ndarray, origin, tile_shape, ASDF_DATATYPE_SOURCE, (void **)&tile),
        ==,
        ASDF_NDARRAY_OK);
    assert_memory_equal(page_size, tile, expected);
    free(tile);
    tile = NULL;

    // Subsequent accesses all return the same decompressed data
    size_t size = 0;
    const void *data = asdf_ndarray_data(ndarray, &size);
    assert_not_null(data);
    assert_size(size, ==, page_size * num_pages);
    size_t size2 = 0;
    assert_ptr_equal(asdf_ndarray_data(ndarray, &size2), data);
    assert_size(size2, ==, size);

    // Read the last page
    origin[0] = page_size * (num_pages - 1);
    expected[0] = num_pages - 1;
    assert_int(
        asdf_ndarray_read_tile_ndim(ndarray, origin, tile_shape, ASDF_DATATYPE_SOURCE, (void **)&tile),
        ==,
        ASDF_NDARRAY_OK);
    assert_memory_equal(page_size, tile, expected);
    free(tile);

    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Test reading a compressed array large enough to span multiple chunks
 *
 * Regression test for #274.
 *
 * The array in ``compressed.asdf`` decompresses in a single chunk (and is a
 * single LZ4 block) so it does not exercise decompression of multiple chunks
 * in lazy mode.  This writes an array larger than the 4 MB LZ4 block size
 * (and the default lazy decompression chunk size), and which is not a
 * multiple of either the block size or the page size, so that the last chunk
 * and the last page are both partial.
 */
MU_TEST(read_compressed_block_multi_chunk) {
    const char *comp = munit_parameters_get(params, "comp");
    /* Large enough to span two LZ4 blocks plus a partial one, and not a
     * multiple of page size */
    const size_t n = 9000000;
    uint8_t *ref = malloc(n);

    if (!ref)
        return MUNIT_ERROR;

    // Use a prime period so the pattern does not line up with page or chunk
    // boundaries
    for (size_t idx = 0; idx < n; idx++)
        ref[idx] = idx % 251;

    const char *path = write_compressed_ndarray_to_file(comp, fixture->tempfile_prefix, ref, n);

    if (!path) {
        free(ref);
        return MUNIT_ERROR;
    }

    asdf_config_t config = {
        .decomp = {
            .mode = decomp_mode_from_param(munit_parameters_get(params, "mode"))
        }
    };
    asdf_file_t *file = asdf_open_file_ex(path, "r", &config);
    assert_not_null(file);
    asdf_ndarray_t *ndarray = NULL;
    assert_int(asdf_get_ndarray(file, "data", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    size_t size = 0;
    const uint8_t *data = asdf_ndarray_data(ndarray, &size);
    assert_not_null(data);
    assert_size(size, ==, n);
    assert_memory_equal(n, data, ref);
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    free(ref);
    free((void *)path);
    return MUNIT_OK;
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
 * Copy a compressed block-backed ndarray into another file
 *
 * asdf_ndarray_copy makes an independent copy of the source ndarray's block,
 * left compressed with the same compressor, into a new block in the destination
 * file:
 *  1. Write a compressed ndarray to a temp file.
 *  2. Re-open it, copy the ndarray into a fresh destination file, and write it.
 *  3. Verify the destination round-trips and the block stayed compressed with
 *     the same compressor.
 */
MU_TEST(copy_compressed_block) {
    const char *comp = munit_parameters_get(params, "comp");
    const size_t n = 4096;

    uint8_t *data = malloc(n);

    if (!data)
        return MUNIT_ERROR;

    for (size_t idx = 0; idx < n; idx++)
        data[idx] = (uint8_t)(idx % 4);

    const char *src_path = write_compressed_ndarray_to_file(
        comp, fixture->tempfile_prefix, data, n);

    if (!src_path) {
        free(data);
        return MUNIT_ERROR;
    }

    const char *dst_path = strdup(get_temp_file_path(fixture->tempfile_prefix, "copy.asdf"));

    if (!dst_path) {
        free(data);
        free((void *)src_path);
        return MUNIT_ERROR;
    }

    asdf_file_t *src_file = asdf_open_file(src_path, "r");
    assert_not_null(src_file);
    asdf_ndarray_t *src = NULL;
    assert_int(asdf_get_ndarray(src_file, "data", &src), ==, ASDF_VALUE_OK);
    assert_not_null(src);

    asdf_file_t *dst_file = asdf_open(NULL);
    assert_not_null(dst_file);
    asdf_ndarray_t *copy = asdf_ndarray_copy(dst_file, src);
    assert_not_null(copy);

    assert_int(asdf_set_ndarray(dst_file, "data", copy), ==, ASDF_VALUE_OK);
    assert_int(asdf_write_to(dst_file, dst_path), ==, 0);
    asdf_ndarray_destroy(copy);
    asdf_close(dst_file);
    asdf_ndarray_destroy(src);
    asdf_close(src_file);

    /* Verify the destination round-trips and stayed compressed with the same compressor */
    dst_file = asdf_open_file(dst_path, "r");
    assert_not_null(dst_file);
    asdf_ndarray_t *dst = NULL;
    assert_int(asdf_get_ndarray(dst_file, "data", &dst), ==, ASDF_VALUE_OK);
    assert_not_null(dst);
    size_t read_size = 0;
    const uint8_t *read_data = asdf_ndarray_data(dst, &read_size);
    assert_not_null(read_data);
    assert_size(read_size, ==, n);
    assert_memory_equal(n, read_data, data);

    asdf_block_t *block = asdf_block_open(dst_file, dst->source);
    assert_not_null(block);
    assert_string_equal(asdf_block_compression(block), comp);
    asdf_block_close(block);

    asdf_ndarray_destroy(dst);
    asdf_close(dst_file);

    free(data);
    free((void *)src_path);
    free((void *)dst_path);
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
    MunitResult res = MUNIT_ERROR;
    const char *path = NULL;

    uint8_t *data = malloc(n);

    if (!data)
        goto done;

    for (size_t idx = 0; idx < n; idx++)
        data[idx] = (uint8_t)(idx % 4);

    path = write_compressed_ndarray_to_file(comp, fixture->tempfile_prefix, data, n);

    if (!path)
        goto done;

    /* re-open, verify checksum, close block */
    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);
    asdf_block_t *block = asdf_block_open(file, 0);
    assert_not_null(block);
    const unsigned char *checksum = asdf_block_checksum(block);
    assert_not_null(checksum);
    unsigned char *empty = calloc(ASDF_BLOCK_CHECKSUM_DIGEST_SIZE, sizeof(unsigned char));

    if (!empty)
        goto done;

    assert_memory_not_equal(ASDF_BLOCK_CHECKSUM_DIGEST_SIZE, checksum, empty);
    free(empty);
    assert_true(asdf_block_checksum_verify(block, NULL));
    asdf_block_close(block);
    asdf_close(file);
    res = MUNIT_OK;
done:
    free(data);
    free((void *)path);
    return res;
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
    MU_RUN_TEST(read_compressed_ndarray_offset, comp_mode_test_params),
    MU_RUN_TEST(read_compressed_block_to_file, comp_test_params),
    MU_RUN_TEST(read_compressed_block_to_file_on_threshold, comp_test_params),
    MU_RUN_TEST(open_close_compressed_block, comp_mode_test_params),
    MU_RUN_TEST(read_compressed_block_lazy_random_access, comp_mode_chunk_test_params),
    MU_RUN_TEST(read_compressed_block_lazy_concurrent_fault, comp_test_params),
    MU_RUN_TEST(compressed_block_no_hang_on_segfault, comp_mode_test_params),
    MU_RUN_TEST(decomp_mode_selection, comp_mode_test_params),
    MU_RUN_TEST(decomp_mode_selection_to_file, comp_mode_test_params),
    MU_RUN_TEST(read_compressed_block_repeated_access, comp_mode_test_params),
    MU_RUN_TEST(read_compressed_block_multi_chunk, comp_mode_test_params),
    MU_RUN_TEST(reemit_compressed_verbatim, comp_test_params),
    MU_RUN_TEST(recompress_block),
    MU_RUN_TEST(copy_compressed_block, comp_test_params),
    MU_RUN_TEST(access_then_write, comp_test_params),
    MU_RUN_TEST(verify_checksum, comp_test_params),
    MU_RUN_TEST(write_compressed_to_mem, comp_test_params),
    MU_RUN_TEST(write_to_mem_large_tree_realloc)
);


MU_RUN_SUITE(compression);
