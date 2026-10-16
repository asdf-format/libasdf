#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asdf/file.h"
#include "asdf/log.h"

#include "config.h"

#include "munit.h"
#include "util.h"


#define ANSI_ESCAPE "\033["


/*
 * Open an in-memory file logging with the given configuration to ``stream``
 */
static asdf_file_t *open_with_log(asdf_log_cfg_t log_cfg, FILE *stream) {
    asdf_config_t config = {0};
    config.log = log_cfg;
    config.log.stream = stream;
    return asdf_open_mem_ex(NULL, 0, &config);
}


/*
 * An explicitly configured log level takes precedence over ASDF_LOG_LEVEL,
 * including when it equals the built-in default (WARN)
 */
MU_TEST(level_explicit_overrides_env) {
    asdf_log_cfg_t log_cfg = {.level = ASDF_LOG_WARN, .fields = ASDF_LOG_FIELD_MSG};
    log_capture_t cap = {0};
    assert_true(log_capture_open(&cap));

    setenv("ASDF_LOG_LEVEL", "DEBUG", 1);
    asdf_file_t *file = open_with_log(log_cfg, cap.stream);
    unsetenv("ASDF_LOG_LEVEL");
    assert_not_null(file);

    asdf_file_log(file, ASDF_LOG_INFO, "src.c", 1, "info");
    asdf_file_log(file, ASDF_LOG_WARN, "src.c", 2, "warn");
    assert_string_equal(log_capture_text(&cap), "warn\n");
    asdf_close(file);
    log_capture_close(&cap);
    return MUNIT_OK;
}


/*
 * ASDF_LOG_DEFAULT (the zero value) takes the level from ASDF_LOG_LEVEL
 */
MU_TEST(level_default_uses_env) {
    asdf_log_cfg_t log_cfg = {.level = ASDF_LOG_DEFAULT, .fields = ASDF_LOG_FIELD_MSG};
    log_capture_t cap = {0};
    assert_true(log_capture_open(&cap));

    setenv("ASDF_LOG_LEVEL", "DEBUG", 1);
    asdf_file_t *file = open_with_log(log_cfg, cap.stream);
    unsetenv("ASDF_LOG_LEVEL");
    assert_not_null(file);

    asdf_file_log(file, ASDF_LOG_TRACE, "src.c", 1, "trace");
    asdf_file_log(file, ASDF_LOG_DEBUG, "src.c", 2, "debug");
    assert_string_equal(log_capture_text(&cap), "debug\n");
    asdf_close(file);
    log_capture_close(&cap);
    return MUNIT_OK;
}


/*
 * ASDF_LOG_NONE in the configuration disables logging entirely, even if
 * ASDF_LOG_LEVEL is set (regression test: it used to be 0 and so was
 * indistinguishable from an unset level)
 */
MU_TEST(level_config_none) {
    asdf_log_cfg_t log_cfg = {.level = ASDF_LOG_NONE};
    log_capture_t cap = {0};
    assert_true(log_capture_open(&cap));

    setenv("ASDF_LOG_LEVEL", "TRACE", 1);
    asdf_file_t *file = open_with_log(log_cfg, cap.stream);
    unsetenv("ASDF_LOG_LEVEL");
    assert_not_null(file);

    asdf_file_log(file, ASDF_LOG_FATAL, "src.c", 1, "fatal");
    assert_string_equal(log_capture_text(&cap), "");
    asdf_close(file);
    log_capture_close(&cap);
    return MUNIT_OK;
}


/*
 * ASDF_LOG_LEVEL=NONE in the environment disables logging entirely
 */
MU_TEST(level_env_none) {
    asdf_log_cfg_t log_cfg = {0};
    log_capture_t cap = {0};
    assert_true(log_capture_open(&cap));

    setenv("ASDF_LOG_LEVEL", "NONE", 1);
    asdf_file_t *file = open_with_log(log_cfg, cap.stream);
    unsetenv("ASDF_LOG_LEVEL");
    assert_not_null(file);

    asdf_file_log(file, ASDF_LOG_FATAL, "src.c", 1, "fatal");
    assert_string_equal(log_capture_text(&cap), "");
    asdf_close(file);
    log_capture_close(&cap);
    return MUNIT_OK;
}


/*
 * Messages with a level that is not a real severity are dropped rather than
 * looked up out of bounds
 */
MU_TEST(message_level_out_of_range) {
    asdf_log_cfg_t log_cfg = {.level = ASDF_LOG_TRACE, .fields = ASDF_LOG_FIELD_ALL};
    log_capture_t cap = {0};
    assert_true(log_capture_open(&cap));
    asdf_file_t *file = open_with_log(log_cfg, cap.stream);
    assert_not_null(file);

    asdf_file_log(file, ASDF_LOG_DEFAULT, "src.c", 1, "default");
    asdf_file_log(file, ASDF_LOG_NONE, "src.c", 2, "none");
    asdf_file_log(file, (asdf_log_level_t)(ASDF_LOG_NONE + 1), "src.c", 3, "too high");
    assert_string_equal(log_capture_text(&cap), "");
    asdf_close(file);
    log_capture_close(&cap);
    return MUNIT_OK;
}


/*
 * Logging a message does not clobber errno
 */
MU_TEST(errno_preserved) {
    asdf_log_cfg_t log_cfg = {.level = ASDF_LOG_WARN};
    log_capture_t cap = {0};
    assert_true(log_capture_open(&cap));
    asdf_file_t *file = open_with_log(log_cfg, cap.stream);
    assert_not_null(file);

    errno = ERANGE;
    asdf_file_log(file, ASDF_LOG_ERROR, "src.c", 1, "message");
    assert_int(errno, ==, ERANGE);
    asdf_close(file);
    log_capture_close(&cap);
    return MUNIT_OK;
}


/*
 * Each combination of log fields is separated by exactly one space, with no
 * color codes since the stream is not a terminal (regression test).
 */
MU_TEST(fields_format) {
    struct {
        asdf_log_fields_t fields;
        const char *expected;
    } cases[] = {
        {ASDF_LOG_FIELD_ALL, "[ERROR] (" PACKAGE_NAME ")src.c:12: message\n"},
        {ASDF_LOG_FIELD_LEVEL | ASDF_LOG_FIELD_MSG, "[ERROR] message\n"},
        {ASDF_LOG_FIELD_FILE | ASDF_LOG_FIELD_LINE | ASDF_LOG_FIELD_MSG, "src.c:12: message\n"},
        {ASDF_LOG_FIELD_MSG, "message\n"},
        {ASDF_LOG_FIELD_LEVEL | ASDF_LOG_FIELD_FILE, "[ERROR] src.c:\n"},
    };
    log_capture_t cap = {0};

    for (size_t idx = 0; idx < sizeof(cases) / sizeof(cases[0]); idx++) {
        asdf_log_cfg_t log_cfg = {.level = ASDF_LOG_WARN, .fields = cases[idx].fields};
        assert_true(log_capture_open(&cap));
        asdf_file_t *file = open_with_log(log_cfg, cap.stream);
        assert_not_null(file);
        asdf_file_log(file, ASDF_LOG_ERROR, "src.c", 12, "message");
        assert_string_equal(log_capture_text(&cap), cases[idx].expected);
        asdf_close(file);
        log_capture_close(&cap);
    }

    return MUNIT_OK;
}


/*
 * Log one message to ``stream`` with the given ``no_color`` setting and
 * ASDF_LOG_COLOR value (NULL to leave it unset), returning whether it was
 * colorized
 *
 * ``read_fd`` is where the output can be read back from: the master side of a
 * pseudo-terminal, or -1 to read it from ``cap`` instead.
 */
static bool log_is_colored(
    FILE *stream, int read_fd, log_capture_t *cap, bool no_color, const char *env_color) {
    asdf_log_cfg_t log_cfg = {.level = ASDF_LOG_WARN, .no_color = no_color};
    char buf[512] = {0};
    const char *text = buf;

    if (env_color)
        setenv("ASDF_LOG_COLOR", env_color, 1);

    asdf_file_t *file = open_with_log(log_cfg, stream);
    unsetenv("ASDF_LOG_COLOR");
    assert_not_null(file);
    asdf_file_log(file, ASDF_LOG_ERROR, "src.c", 1, "message");
    asdf_close(file);

    if (read_fd >= 0)
        assert_int(read(read_fd, buf, sizeof(buf) - 1), >, 0);
    else
        text = log_capture_text(cap);

    return strstr(text, ANSI_ESCAPE) != NULL;
}


/*
 * Color is decided by ``no_color``, then the ASDF_LOG_COLOR environment
 * variable, then whether the stream is a terminal; builds without color
 * support never colorize
 */
MU_TEST(color_not_tty) {
    log_capture_t cap = {0};
    bool colored = false;

    assert_true(log_capture_open(&cap));
    colored = log_is_colored(cap.stream, -1, &cap, false, NULL);
    log_capture_close(&cap);
    assert_false(colored);

    assert_true(log_capture_open(&cap));
    colored = log_is_colored(cap.stream, -1, &cap, false, "1");
    log_capture_close(&cap);
#ifdef ASDF_LOG_COLOR
    assert_true(colored);
#else
    assert_false(colored);
#endif

    // An explicit no_color wins over ASDF_LOG_COLOR=1 (regression test)
    assert_true(log_capture_open(&cap));
    colored = log_is_colored(cap.stream, -1, &cap, true, "1");
    log_capture_close(&cap);
    assert_false(colored);

    assert_true(log_capture_open(&cap));
    colored = log_is_colored(cap.stream, -1, &cap, false, "0");
    log_capture_close(&cap);
    assert_false(colored);
    return MUNIT_OK;
}


MU_TEST(color_tty) {
    int master_fd = posix_openpt(O_RDWR | O_NOCTTY);

    if (master_fd < 0 || grantpt(master_fd) != 0 || unlockpt(master_fd) != 0) {
        if (master_fd >= 0)
            close(master_fd);
        return MUNIT_SKIP;
    }

    int slave_fd = open(ptsname(master_fd), O_RDWR | O_NOCTTY);
    assert_int(slave_fd, >=, 0);
    FILE *stream = fdopen(slave_fd, "w");
    assert_not_null(stream);

#ifdef ASDF_LOG_COLOR
    assert_true(log_is_colored(stream, master_fd, NULL, false, NULL));
    assert_true(log_is_colored(stream, master_fd, NULL, false, "1"));
#else
    assert_false(log_is_colored(stream, master_fd, NULL, false, NULL));
    assert_false(log_is_colored(stream, master_fd, NULL, false, "1"));
#endif
    assert_false(log_is_colored(stream, master_fd, NULL, true, NULL));
    assert_false(log_is_colored(stream, master_fd, NULL, true, "1"));
    assert_false(log_is_colored(stream, master_fd, NULL, false, "0"));

    fclose(stream);
    close(master_fd);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    logging,
    MU_RUN_TEST(level_explicit_overrides_env),
    MU_RUN_TEST(level_default_uses_env),
    MU_RUN_TEST(level_config_none),
    MU_RUN_TEST(level_env_none),
    MU_RUN_TEST(message_level_out_of_range),
    MU_RUN_TEST(errno_preserved),
    MU_RUN_TEST(fields_format),
    MU_RUN_TEST(color_not_tty),
    MU_RUN_TEST(color_tty)
);


MU_RUN_SUITE(logging);
