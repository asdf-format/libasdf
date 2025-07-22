#ifndef ASDF_FILE_H
#define ASDF_FILE_H

#include <stddef.h>
#include <stdio.h>

#include <asdf/util.h>
#include <asdf/value.h>

/* Forward declaration */
typedef struct asdf_file asdf_file_t;

ASDF_EXPORT asdf_file_t *asdf_open_file(const char *filename, const char* mode);
ASDF_EXPORT asdf_file_t *asdf_open_fp(FILE *fp, const char *filename);
ASDF_EXPORT asdf_file_t *asdf_open_mem(const void *buf, size_t size);
ASDF_EXPORT void asdf_close(asdf_file_t *file);

/* As a convenience, asdf_open() is available as an alias for asdf_open_file */
inline asdf_file_t *asdf_open(const char *filename, const char *mode) {
    return asdf_open_file(filename, mode);
}

/* Value getters */
ASDF_EXPORT asdf_value_t *asdf_get_value(asdf_file_t *file, const char *path);
ASDF_EXPORT bool asdf_is_string(asdf_file_t *file, const char *path);
ASDF_EXPORT asdf_value_err_t asdf_get_string(asdf_file_t *file, const char *path, const char **out, size_t *out_len);
ASDF_EXPORT asdf_value_err_t asdf_get_string0(asdf_file_t *file, const char *path, const char **out);
ASDF_EXPORT bool asdf_is_scalar(asdf_file_t *file, const char *path);
ASDF_EXPORT asdf_value_err_t asdf_get_scalar(asdf_file_t *file, const char *path, const char **out, size_t *out_len);
ASDF_EXPORT asdf_value_err_t asdf_get_scalar0(asdf_file_t *file, const char *path, const char **out);
ASDF_EXPORT bool asdf_is_bool(asdf_file_t *file, const char *path);
ASDF_EXPORT asdf_value_err_t asdf_get_bool(asdf_file_t *file, const char *path, bool *out);
ASDF_EXPORT bool asdf_is_null(asdf_file_t *file, const char *path);
ASDF_EXPORT bool asdf_is_int(asdf_file_t *file, const char *path);
ASDF_EXPORT bool asdf_is_int8(asdf_file_t *file, const char *path);
ASDF_EXPORT asdf_value_err_t asdf_get_int8(asdf_file_t *file, const char *path, int8_t *out);
ASDF_EXPORT bool asdf_is_int16(asdf_file_t *file, const char *path);
ASDF_EXPORT asdf_value_err_t asdf_get_int16(asdf_file_t *file, const char *path, int16_t *out);
ASDF_EXPORT bool asdf_is_int32(asdf_file_t *file, const char *path);
ASDF_EXPORT asdf_value_err_t asdf_get_int32(asdf_file_t *file, const char *path, int32_t *out);
ASDF_EXPORT bool asdf_is_int64(asdf_file_t *file, const char *path);
ASDF_EXPORT asdf_value_err_t asdf_get_int64(asdf_file_t *file, const char *path, int64_t *out);
ASDF_EXPORT bool asdf_is_uint8(asdf_file_t *file, const char *path);
ASDF_EXPORT asdf_value_err_t asdf_get_uint8(asdf_file_t *file, const char *path, uint8_t *out);
ASDF_EXPORT bool asdf_is_uint16(asdf_file_t *file, const char *path);
ASDF_EXPORT asdf_value_err_t asdf_get_uint16(asdf_file_t *file, const char *path, uint16_t *out);
ASDF_EXPORT bool asdf_is_uint32(asdf_file_t *file, const char *path);
ASDF_EXPORT asdf_value_err_t asdf_get_uint32(asdf_file_t *file, const char *path, uint32_t *out);
ASDF_EXPORT bool asdf_is_uint64(asdf_file_t *file, const char *path);
ASDF_EXPORT asdf_value_err_t asdf_get_uint64(asdf_file_t *file, const char *path, uint64_t *out);
ASDF_EXPORT bool asdf_is_float(asdf_file_t *file, const char *path);
ASDF_EXPORT asdf_value_err_t asdf_get_float(asdf_file_t *file, const char *path, float *out);
ASDF_EXPORT bool asdf_is_double(asdf_file_t *file, const char *path);
ASDF_EXPORT asdf_value_err_t asdf_get_double(asdf_file_t *file, const char *path, double *out);

#endif /* ASDF_FILE_H */
