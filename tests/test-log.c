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
 * Open an in-memory file with the given log configuration
 */
static asdf_file_t *open_with_log(asdf_log_cfg_t log_cfg) {
    asdf_config_t config = {.log = log_cfg};
    return asdf_open_mem_ex(NULL, 0, &config);
}


/*
 * Open an in-memory file logging through the default handler to ``stream``
 */
static asdf_file_t *open_with_log_stream(asdf_log_cfg_t log_cfg, FILE *stream) {
    log_cfg.stream = stream;
    return open_with_log(log_cfg);
}


/*
 * An explicitly configured log level takes precedence over ASDF_LOG_LEVEL,
 * including when it equals the built-in default (WARN)
 */
MU_TEST(level_explicit_overrides_env) {
    log_capture_t cap = {0};

    setenv("ASDF_LOG_LEVEL", "DEBUG", 1);
    asdf_file_t *file = open_with_log(LOG_CAPTURE_CFG(&cap, ASDF_LOG_WARN));
    unsetenv("ASDF_LOG_LEVEL");
    assert_not_null(file);

    asdf_file_log(file, ASDF_LOG_INFO, "src.c", 1, "info");
    asdf_file_log(file, ASDF_LOG_WARN, "src.c", 2, "warn");
    assert_size(cap.count, ==, 1);
    assert_int(cap.recs[0].level, ==, ASDF_LOG_WARN);
    assert_string_equal(cap.recs[0].msg, "warn");
    asdf_close(file);
    log_capture_clear(&cap);
    return MUNIT_OK;
}


/*
 * ASDF_LOG_DEFAULT (the zero value) takes the level from ASDF_LOG_LEVEL
 */
MU_TEST(level_default_uses_env) {
    log_capture_t cap = {0};

    setenv("ASDF_LOG_LEVEL", "DEBUG", 1);
    asdf_file_t *file = open_with_log(LOG_CAPTURE_CFG(&cap, ASDF_LOG_DEFAULT));
    unsetenv("ASDF_LOG_LEVEL");
    assert_not_null(file);

    asdf_file_log(file, ASDF_LOG_TRACE, "src.c", 1, "trace");
    asdf_file_log(file, ASDF_LOG_DEBUG, "src.c", 2, "debug");
    assert_size(cap.count, ==, 1);
    assert_int(cap.recs[0].level, ==, ASDF_LOG_DEBUG);
    assert_string_equal(cap.recs[0].msg, "debug");
    asdf_close(file);
    log_capture_clear(&cap);
    return MUNIT_OK;
}


/*
 * ASDF_LOG_NONE in the configuration disables logging entirely, even if
 * ASDF_LOG_LEVEL is set (regression test: it used to be 0 and so was
 * indistinguishable from an unset level)
 */
MU_TEST(level_config_none) {
    log_capture_t cap = {0};

    setenv("ASDF_LOG_LEVEL", "TRACE", 1);
    asdf_file_t *file = open_with_log(LOG_CAPTURE_CFG(&cap, ASDF_LOG_NONE));
    unsetenv("ASDF_LOG_LEVEL");
    assert_not_null(file);

    asdf_file_log(file, ASDF_LOG_FATAL, "src.c", 1, "fatal");
    assert_size(cap.count, ==, 0);
    asdf_close(file);
    log_capture_clear(&cap);
    return MUNIT_OK;
}


/*
 * ASDF_LOG_LEVEL=NONE in the environment disables logging entirely
 */
MU_TEST(level_env_none) {
    log_capture_t cap = {0};

    setenv("ASDF_LOG_LEVEL", "NONE", 1);
    asdf_file_t *file = open_with_log(LOG_CAPTURE_CFG(&cap, ASDF_LOG_DEFAULT));
    unsetenv("ASDF_LOG_LEVEL");
    assert_not_null(file);

    asdf_file_log(file, ASDF_LOG_FATAL, "src.c", 1, "fatal");
    assert_size(cap.count, ==, 0);
    asdf_close(file);
    log_capture_clear(&cap);
    return MUNIT_OK;
}


/*
 * Messages with a level that is not a real severity are dropped rather than
 * looked up out of bounds, and never reach a handler
 */
MU_TEST(message_level_out_of_range) {
    log_capture_t cap = {0};
    asdf_file_t *file = open_with_log(LOG_CAPTURE_CFG(&cap, ASDF_LOG_TRACE));
    assert_not_null(file);

    asdf_file_log(file, ASDF_LOG_DEFAULT, "src.c", 1, "default");
    asdf_file_log(file, ASDF_LOG_NONE, "src.c", 2, "none");
    asdf_file_log(file, (asdf_log_level_t)(ASDF_LOG_NONE + 1), "src.c", 3, "too high");
    assert_size(cap.count, ==, 0);
    asdf_close(file);
    log_capture_clear(&cap);
    return MUNIT_OK;
}


/* A handler that does nothing but clobber errno */
static void clobber_errno_handler(const asdf_log_rec_t *rec, void *userdata) {
    (void)rec;
    (void)userdata;
    errno = EIO;
}


/*
 * Logging a message does not clobber errno, whether through the default
 * handler or a custom handler that changes it
 */
MU_TEST(errno_preserved) {
    asdf_log_cfg_t log_cfg = {.level = ASDF_LOG_WARN};
    log_stream_t stream = {0};
    assert_true(log_stream_open(&stream));
    asdf_file_t *file = open_with_log_stream(log_cfg, stream.stream);
    assert_not_null(file);

    errno = ERANGE;
    asdf_file_log(file, ASDF_LOG_ERROR, "src.c", 1, "message");
    assert_int(errno, ==, ERANGE);
    asdf_close(file);
    log_stream_close(&stream);

    log_cfg.handler = clobber_errno_handler;
    file = open_with_log(log_cfg);
    assert_not_null(file);
    errno = ERANGE;
    asdf_file_log(file, ASDF_LOG_ERROR, "src.c", 1, "message");
    assert_int(errno, ==, ERANGE);
    asdf_close(file);
    return MUNIT_OK;
}


#define RECORDING_MAX_RECS 2
#define RECORDING_MAX_MSG 64


/*
 * Userdata for `recording_handler`: copies of up to two records, made without
 * any help from the test utilities
 */
typedef struct {
    size_t count;
    asdf_log_rec_t recs[RECORDING_MAX_RECS];
    char msgs[RECORDING_MAX_RECS][RECORDING_MAX_MSG];
} recording_t;


static void recording_handler(const asdf_log_rec_t *rec, void *userdata) {
    recording_t *recording = userdata;

    if (recording->count >= RECORDING_MAX_RECS)
        return;

    // The record and its message are only valid during the call, so the
    // message is copied out and the copied record pointed at the copy
    memcpy(&recording->recs[recording->count], rec, sizeof(*rec));
    snprintf(recording->msgs[recording->count], RECORDING_MAX_MSG, "%s", rec->msg);
    recording->recs[recording->count].msg = recording->msgs[recording->count];
    recording->count++;
}


/*
 * A custom handler receives each record at or above the configured level,
 * end-to-end, along with its userdata
 */
MU_TEST(custom_handler) {
    recording_t recording = {0};
    asdf_log_cfg_t log_cfg = {
        .level = ASDF_LOG_INFO, .handler = recording_handler, .handler_userdata = &recording};
    asdf_file_t *file = open_with_log(log_cfg);
    assert_not_null(file);

    asdf_file_log(file, ASDF_LOG_DEBUG, "below.c", 1, "below threshold");
    asdf_file_log(file, ASDF_LOG_WARN, "first.c", 12, "first %s", "message");
    asdf_file_log(file, ASDF_LOG_ERROR, "second.c", 34, "second message %d", 2);
    asdf_close(file);

    assert_size(recording.count, ==, 2);
    assert_int(recording.recs[0].level, ==, ASDF_LOG_WARN);
    assert_string_equal(recording.recs[0].package, PACKAGE_NAME);
    assert_string_equal(recording.recs[0].file, "first.c");
    assert_int(recording.recs[0].lineno, ==, 12);
    assert_string_equal(recording.recs[0].msg, "first message");
    assert_int(recording.recs[1].level, ==, ASDF_LOG_ERROR);
    assert_string_equal(recording.recs[1].package, PACKAGE_NAME);
    assert_string_equal(recording.recs[1].file, "second.c");
    assert_int(recording.recs[1].lineno, ==, 34);
    assert_string_equal(recording.recs[1].msg, "second message 2");
    return MUNIT_OK;
}


/*
 * Messages are delivered whole whether they fit in the internal on-stack
 * buffer (512 bytes, including the terminator) or not
 */
MU_TEST(long_message) {
    size_t lengths[] = {511, 512, 4096};
    log_capture_t cap = {0};
    char *long_msg = NULL;

    asdf_file_t *file = open_with_log(LOG_CAPTURE_CFG(&cap, ASDF_LOG_WARN));
    assert_not_null(file);

    for (size_t idx = 0; idx < sizeof(lengths) / sizeof(lengths[0]); idx++) {
        long_msg = malloc(lengths[idx] + 1);
        assert_not_null(long_msg);
        memset(long_msg, 'x', lengths[idx]);
        long_msg[lengths[idx]] = '\0';
        asdf_file_log(file, ASDF_LOG_WARN, "src.c", 1, "%s", long_msg);
        assert_size(cap.count, ==, idx + 1);
        assert_string_equal(cap.recs[idx].msg, long_msg);
        free(long_msg);
    }

    asdf_close(file);
    log_capture_clear(&cap);
    return MUNIT_OK;
}


/*
 * The default handler always gets the file's effective configuration as its
 * userdata, whether it is selected explicitly or by leaving the handler NULL;
 * any userdata supplied alongside it is ignored
 */
MU_TEST(default_handler_ignores_userdata) {
    log_stream_t stream = {0};
    log_stream_t decoy = {0};
    asdf_log_handler_t handlers[] = {NULL, asdf_log_default_handler};

    assert_true(log_stream_open(&decoy));
    asdf_log_cfg_t decoy_cfg = {.stream = decoy.stream, .fields = ASDF_LOG_FIELD_ALL};

    for (size_t idx = 0; idx < sizeof(handlers) / sizeof(handlers[0]); idx++) {
        asdf_log_cfg_t log_cfg = {
            .level = ASDF_LOG_WARN,
            .fields = ASDF_LOG_FIELD_MSG,
            .handler = handlers[idx],
            .handler_userdata = &decoy_cfg};
        assert_true(log_stream_open(&stream));
        asdf_file_t *file = open_with_log_stream(log_cfg, stream.stream);
        assert_not_null(file);
        asdf_file_log(file, ASDF_LOG_ERROR, "src.c", 1, "message");
        assert_string_equal(log_stream_text(&stream), "message\n");
        asdf_close(file);
        log_stream_close(&stream);
    }

    assert_string_equal(log_stream_text(&decoy), "");
    log_stream_close(&decoy);
    return MUNIT_OK;
}


/*
 * The default handler can be called directly with a configuration of one's
 * own, e.g. by a custom handler that adds to the standard output
 */
MU_TEST(default_handler_direct) {
    log_stream_t stream = {0};
    assert_true(log_stream_open(&stream));
    asdf_log_cfg_t log_cfg = {
        .stream = stream.stream, .fields = ASDF_LOG_FIELD_LEVEL | ASDF_LOG_FIELD_MSG};
    asdf_log_rec_t rec = {
        .level = ASDF_LOG_INFO,
        .package = PACKAGE_NAME,
        .file = "src.c",
        .lineno = 1,
        .msg = "message"};

    asdf_log_default_handler(&rec, &log_cfg);
    // Not a real severity, so ignored
    rec.level = ASDF_LOG_NONE;
    asdf_log_default_handler(&rec, &log_cfg);
    // The stream is not a terminal, so no color, even though no_color is unset
    assert_string_equal(log_stream_text(&stream), "[INFO ] message\n");
    log_stream_close(&stream);
    return MUNIT_OK;
}


/* Userdata for `reentrant_handler` */
typedef struct {
    asdf_file_t *file;
    log_capture_t cap;
} reentrant_t;


/* A handler that logs a second message from inside itself, once */
static void reentrant_handler(const asdf_log_rec_t *rec, void *userdata) {
    reentrant_t *reentrant = userdata;
    log_capture_handler(rec, &reentrant->cap);

    if (reentrant->cap.count == 1)
        asdf_file_log(reentrant->file, ASDF_LOG_WARN, "src.c", 2, "nested");
}


/*
 * A handler may itself log through libasdf without corrupting the record it
 * was called with
 */
MU_TEST(handler_reentrant) {
    reentrant_t reentrant = {0};
    asdf_log_cfg_t log_cfg = {
        .level = ASDF_LOG_WARN, .handler = reentrant_handler, .handler_userdata = &reentrant};
    reentrant.file = open_with_log(log_cfg);
    assert_not_null(reentrant.file);

    asdf_file_log(reentrant.file, ASDF_LOG_WARN, "src.c", 1, "outer");
    assert_size(reentrant.cap.count, ==, 2);
    assert_string_equal(reentrant.cap.recs[0].msg, "outer");
    assert_string_equal(reentrant.cap.recs[1].msg, "nested");
    asdf_close(reentrant.file);
    log_capture_clear(&reentrant.cap);
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
    log_stream_t cap = {0};

    for (size_t idx = 0; idx < sizeof(cases) / sizeof(cases[0]); idx++) {
        asdf_log_cfg_t log_cfg = {.level = ASDF_LOG_WARN, .fields = cases[idx].fields};
        assert_true(log_stream_open(&cap));
        asdf_file_t *file = open_with_log_stream(log_cfg, cap.stream);
        assert_not_null(file);
        asdf_file_log(file, ASDF_LOG_ERROR, "src.c", 12, "message");
        assert_string_equal(log_stream_text(&cap), cases[idx].expected);
        asdf_close(file);
        log_stream_close(&cap);
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
    FILE *stream, int read_fd, log_stream_t *cap, bool no_color, const char *env_color) {
    asdf_log_cfg_t log_cfg = {.level = ASDF_LOG_WARN, .no_color = no_color};
    char buf[512] = {0};
    const char *text = buf;

    if (env_color)
        setenv("ASDF_LOG_COLOR", env_color, 1);

    asdf_file_t *file = open_with_log_stream(log_cfg, stream);
    unsetenv("ASDF_LOG_COLOR");
    assert_not_null(file);
    asdf_file_log(file, ASDF_LOG_ERROR, "src.c", 1, "message");
    asdf_close(file);

    if (read_fd >= 0)
        assert_int(read(read_fd, buf, sizeof(buf) - 1), >, 0);
    else
        text = log_stream_text(cap);

    return strstr(text, ANSI_ESCAPE) != NULL;
}


/*
 * Color is decided by ``no_color``, then the ASDF_LOG_COLOR environment
 * variable, then whether the stream is a terminal; builds without color
 * support never colorize
 */
MU_TEST(color_not_tty) {
    log_stream_t cap = {0};
    bool colored = false;

    assert_true(log_stream_open(&cap));
    colored = log_is_colored(cap.stream, -1, &cap, false, NULL);
    log_stream_close(&cap);
    assert_false(colored);

    assert_true(log_stream_open(&cap));
    colored = log_is_colored(cap.stream, -1, &cap, false, "1");
    log_stream_close(&cap);
#ifdef ASDF_LOG_COLOR
    assert_true(colored);
#else
    assert_false(colored);
#endif

    // An explicit no_color wins over ASDF_LOG_COLOR=1 (regression test)
    assert_true(log_stream_open(&cap));
    colored = log_is_colored(cap.stream, -1, &cap, true, "1");
    log_stream_close(&cap);
    assert_false(colored);

    assert_true(log_stream_open(&cap));
    colored = log_is_colored(cap.stream, -1, &cap, false, "0");
    log_stream_close(&cap);
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
    MU_RUN_TEST(custom_handler),
    MU_RUN_TEST(long_message),
    MU_RUN_TEST(default_handler_ignores_userdata),
    MU_RUN_TEST(default_handler_direct),
    MU_RUN_TEST(handler_reentrant),
    MU_RUN_TEST(fields_format),
    MU_RUN_TEST(color_not_tty),
    MU_RUN_TEST(color_tty)
);


MU_RUN_SUITE(logging);
