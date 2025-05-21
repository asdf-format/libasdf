/**
 * Utilities for unit tests
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef REFERENCE_FILES_DIR
#error "REFERENCE_FILES_DIR not defined"
#endif

#ifndef FIXTURES_DIR
#error "FIXTURES_DIR not defined"
#endif


const char* get_fixture_file_path(const char* relative_path) {
    static char full_path[4096];
    snprintf(full_path, sizeof(full_path), "%s/%s", FIXTURES_DIR, relative_path);
    return full_path;
}


const char* get_reference_file_path(const char* relative_path) {
    static char full_path[4096];
    snprintf(full_path, sizeof(full_path), "%s/%s", REFERENCE_FILES_DIR, relative_path);
    return full_path;
}


char *tail_file(const char *filename, uint32_t skip, size_t *out_len) {
    FILE *file = fopen(filename, "rb");

    if (!file)
        return NULL;

    while (skip--) {
        int c = 0;
        while ((c = fgetc(file)) != EOF && c != '\n');
        if (c == EOF) {
            fclose(file);
            return NULL;
        }
    }

    off_t start = ftello(file);
    fseek(file, 0, SEEK_END);
    off_t end = ftello(file);
    size_t size = end - start;
    fseek(file, start, SEEK_SET);

    char* buf = malloc(size + 1);

    if (!buf) {
        fclose(file);
        return NULL;
    }

    if (fread(buf, 1, size, file) != (size_t)size) {
        fclose(file);
        free(buf);
        return NULL;
    }

    fclose(file);
    buf[size] = '\0';

    if (out_len)
        *out_len = size;

    return buf;
}


char *read_file(const char *filename, size_t *out_len) {
    return tail_file(filename, 0, out_len);
}
