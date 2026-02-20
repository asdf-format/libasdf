/**
 * Thin wrapper around munit to make some common tasks quicker
 *
 * I am truly sorry to anyone who maintains this in the future.
 */

#include <dirent.h>
#include <execinfo.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

// Workaround to likely GCC bug:
// When STC is imported it typically pushes/pops some warning diagnostics,
// but it seems that this results in -Wdiscarded-qualifiers remaining "stuck"
// on in some GCCs.  This would be fine (it's a useful warning), except it
// causes problems with munit which omits const qualifiers in some of its
// structs.
#if defined(__GNUC__) && !defined(__clang__)
// Seems to be a bug in gcc that the warning diagnostics set by STC are not
// all restored on #pragma GCC diagnostic pop
// This makes munit sad, but for the tests we can safely ignore.
#pragma GCC diagnostic ignored "-Wdiscarded-qualifiers"
#endif

#define MUNIT_ENABLE_ASSERT_ALIASES
#include "munit/munit.h"

#include "util.h"


#if defined(__GNUC__) || defined(__clang__)
#define UNUSED(x) x __attribute__((unused))
#else
#define UNUSED(x) (void)(x)
#endif



static int orig_stderr;


/** Enable backtraces from tests when they segfault */
static void crash_handler(int sig) {
    void *bt[64];
    int n = write(orig_stderr, "\n", 1);
    n = backtrace(bt, 64);
    backtrace_symbols_fd(bt, n, orig_stderr);
    _exit(128 + sig);
}


__attribute__((constructor))
static void install_crash_handler(void) {
    orig_stderr = dup(STDERR_FILENO);
    signal(SIGBUS, crash_handler);
    signal(SIGSEGV, crash_handler);
    signal(SIGABRT, crash_handler);
}


typedef struct {
    const char *suite_name;
    const char *test_name;
    const char *tempfile_prefix;
} fixtures;


static inline fixtures *mu_test_init_fixtures(fixtures *suite_fixture, const char *test_name) {
    fixtures *fix = calloc(1, sizeof(fixtures));

    if (!fix)
        return NULL;

    fix->suite_name = suite_fixture->suite_name;
    fix->test_name = test_name;

    size_t suite_name_len = strlen(fix->suite_name);
    size_t test_name_len = strlen(fix->test_name);
    size_t tempfile_prefix_len = suite_name_len + 1 + test_name_len + 2;
    char *tempfile_prefix = malloc(tempfile_prefix_len);

    if (!tempfile_prefix) {
        free(fix);
        return NULL;
    }

    int n = snprintf(
        tempfile_prefix, tempfile_prefix_len, "%s-%s-", fix->suite_name, fix->test_name);

    if (n < 0) {
        free(tempfile_prefix);
        free(fix);
        return NULL;
    }

    fix->tempfile_prefix = tempfile_prefix;
    return fix;
}


static inline void mu_test_free_fixtures(fixtures *fixture) {
    if (!fixture)
        return;

    free((char *)fixture->tempfile_prefix);
    free(fixture);
}


/**
 * Test teardown to clean up all temp files created by the test
 *
 * Could fail if multiple instances of the test suite are running in parallel;
 * currently there's nothing to ensure per-process uniqueness.
 */
static inline void mu_cleanup_tempfiles(const fixtures *fix) {
    if (getenv("ASDF_TEST_KEEP_TEMP"))
        return;

    DIR *d = opendir(TEMP_DIR);
    if (!d)
        return;

    const char *prefix = fix->tempfile_prefix;
    size_t prefix_len = strlen(prefix);

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strncmp(ent->d_name, prefix, prefix_len) == 0) {
            char fullpath[PATH_MAX];
            snprintf(fullpath, sizeof(fullpath), "%s/%s", TEMP_DIR, ent->d_name);
            unlink(fullpath);  /* ignore errors */
        }
    }

    closedir(d);
}


/** Generic teardown to use for all tests */
static inline void mu_test_teardown(void *fixture) {
    mu_cleanup_tempfiles((const fixtures *)fixture);
    mu_test_free_fixtures((fixtures *)fixture);
}


/**
 * Wrapper for declaring test cases:
 *
 * Generates a function called ``name`` that's the same signature as an munit test case, except
 * that the fixtures argument is typed as ``fixture *`` so that tests doing have to bother with
 * casting since right now all tests use the same ``fixtures`` struct.
 *
 * Also generates a per-test setup function named ``<name>_setup`` that initializes the fixtures
 * for that test, including the name of the test, its tempfile_prefix, etc.
 *
 * Those are used when declaring the test to munit in ``MU_RUN_TEST()``.
 */
#define MU_TEST(name) \
    MunitResult name(UNUSED(const MunitParameter params[]), UNUSED(fixtures *fixture)); \
    void * name##_setup(UNUSED(const MunitParameter params[]), void *fixture) { \
        fixtures *fix = mu_test_init_fixtures((fixtures *)fixture, #name); \
        return fix; \
    } \
    MunitResult name##_wrapper(const MunitParameter params[], void *fixture) { \
        return name(params, fixture); \
    } \
MunitResult name(UNUSED(const MunitParameter params[]), UNUSED(fixtures *fixture))


#define __MU_RUN_TEST_DISPATCH(_1, _2, NAME, ...) NAME
#define __MU_RUN_TEST_2(name, params) \
    { "/" #name, name##_wrapper, name##_setup, mu_test_teardown, MUNIT_TEST_OPTION_NONE, (params) }
#define __MU_RUN_TEST_1(name) __MU_RUN_TEST_2(name, NULL)

// Macro to declare tests to run within a test suite
#define MU_RUN_TEST(...) \
    __MU_RUN_TEST_DISPATCH( \
        __VA_ARGS__, \
        __MU_RUN_TEST_2, \
        __MU_RUN_TEST_1, \
    )(__VA_ARGS__)


// Helper to create a null-terminated array of tests from varargs
#define MU_TESTS(...) \
    static MunitTest __tests[] = { __VA_ARGS__, { NULL, NULL, NULL, NULL, MUNIT_TEST_OPTION_NONE, NULL }}

// Macro to declare a test suite with a given name and tests inside
#define MU_TEST_SUITE(suite, ...) \
    MU_TESTS(__VA_ARGS__); \
    static const MunitSuite suite = { \
        "/" #suite, \
        __tests, \
        NULL, \
        1, \
        MUNIT_SUITE_OPTION_NONE \
    };


#define MU_RUN_SUITE(suite) \
    static const fixtures suite##_fixtures = { .suite_name = #suite }; \
    int main(int argc, char *argv[]) { \
        return munit_suite_main(&suite, (void *)&suite##_fixtures, argc, argv); \
    }
