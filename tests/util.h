#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

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
 * In-memory capture of libasdf log output
 *
 * Pass ``stream`` as the ``stream`` of an `asdf_log_cfg_t`.  The stream is not
 * a terminal, so the captured text has no color codes unless color is forced
 * on with ``ASDF_LOG_COLOR=1``.
 */
typedef struct {
    FILE *stream;
    char *buf;
    size_t size;
} log_capture_t;

/** Open a log capture; returns false if the stream could not be opened */
bool log_capture_open(log_capture_t *cap);
/**
 * Return the text logged so far (always NUL-terminated, possibly empty)
 *
 * The stream is not flushed first; this relies on libasdf flushing after each
 * message.
 */
const char *log_capture_text(log_capture_t *cap);
/** Close the capture stream and free the captured text */
void log_capture_close(log_capture_t *cap);
