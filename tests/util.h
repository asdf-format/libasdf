#include <stddef.h>
#include <stdint.h>

size_t get_total_memory(void);
const char *get_fixture_file_path(const char *relative_path);
const char *get_reference_file_path(const char *relative_path);
char *read_file(const char *filename, size_t *out_len);
char *tail_file(const char *filename, uint32_t skip, size_t *out_len);
