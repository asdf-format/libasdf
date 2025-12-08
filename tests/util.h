#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
const char *get_temp_file_path(const char *prefix, const char *suffix);
char *read_file(const char *filename, size_t *out_len);
char *tail_file(const char *filename, uint32_t skip, size_t *out_len);
/** Compare the contents of two files byte-for-byte */
bool compare_files(const char *filename_a, const char *filename_b);
