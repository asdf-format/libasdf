#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include <asdf/log.h>

#ifndef REFERENCE_FILES_DIR
#error "REFERENCE_FILES_DIR not defined"
#endif

#ifndef FIXTURES_DIR
#error "FIXTURES_DIR not defined"
#endif

#ifndef TEMP_DIR
#error "TEMP_DIR not defined"
#endif

size_t get_total_memory(void);
const char *get_fixture_file_path(const char *relative_path);
const char *get_reference_file_path(const char *relative_path);
const char *get_run_dir(void);
const char *get_temp_file_path(const char *prefix, const char *suffix);
char *read_file(const char *filename, size_t *out_len);
char *tail_file(const char *filename, uint32_t skip, size_t *out_len);
/** Compare the contents of two files byte-for-byte */
bool compare_files(const char *filename_a, const char *filename_b);


/**
 * Capture of libasdf log records through a custom log handler
 *
 * Use `LOG_CAPTURE_CFG` as the ``log`` of an `asdf_config_t` (or as the log
 * config of a parser) to collect every record logged at or above a level into
 * ``cap``, then inspect ``cap->recs`` or search them with `log_capture_find`.
 * Free the records with `log_capture_clear`.
 */
typedef struct {
    /** Copies of the captured records; their strings are owned by the capture */
    asdf_log_rec_t *recs;
    size_t count;
} log_capture_t;

/** `asdf_log_handler_t` appending a copy of each record to a `log_capture_t` */
void log_capture_handler(const asdf_log_rec_t *rec, void *userdata);

/** An `asdf_log_cfg_t` capturing records at or above ``level`` into ``cap`` */
#define LOG_CAPTURE_CFG(cap, lvl) \
    ((asdf_log_cfg_t){.level = (lvl), .handler = log_capture_handler, .handler_userdata = (cap)})

/** Return the first captured record whose message contains ``substr``, or NULL */
const asdf_log_rec_t *log_capture_find(const log_capture_t *cap, const char *substr);

/** Free all captured records, leaving ``cap`` empty and reusable */
void log_capture_clear(log_capture_t *cap);


/**
 * In-memory capture of the default log handler's formatted output
 *
 * Pass ``stream`` as the ``stream`` of an `asdf_log_cfg_t`.  The stream is not
 * a terminal, so the captured text has no color codes unless color is forced
 * on with ``ASDF_LOG_COLOR=1``.  Prefer `log_capture_t` unless the formatted
 * output itself is under test.
 */
typedef struct {
    FILE *stream;
    char *buf;
    size_t size;
} log_stream_t;

/** Open a log stream capture; returns false if the stream could not be opened */
bool log_stream_open(log_stream_t *cap);
/**
 * Return the text logged so far (always NUL-terminated, possibly empty)
 *
 * The stream is not flushed first; this relies on libasdf flushing after each
 * message.
 */
const char *log_stream_text(log_stream_t *cap);
/** Close the capture stream and free the captured text */
void log_stream_close(log_stream_t *cap);
