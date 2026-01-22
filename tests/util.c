/**
 * Utilities for unit tests
 */

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "config.h"
#ifdef HAVE_STATGRAB
#include <statgrab.h>
#endif

#ifndef REFERENCE_FILES_DIR
#error "REFERENCE_FILES_DIR not defined"
#endif

#ifndef FIXTURES_DIR
#error "FIXTURES_DIR not defined"
#endif

#include "util.h"


size_t get_total_memory(void) {
#ifndef HAVE_STATGRAB
    return 0;
#else
    sg_init(1); // TODO: Maybe move this to somewhere else like during library init
    size_t entries = 0;
    sg_mem_stats *mem_stats = sg_get_mem_stats(&entries);
    sg_shutdown();

    if (!mem_stats || entries < 1)
        return 0;

    return mem_stats->total;
#endif
}


const char* get_fixture_file_path(const char* relative_path) {
    static char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", FIXTURES_DIR, relative_path);
    return full_path;
}


const char* get_reference_file_path(const char* relative_path) {
    static char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), "%s/%s", REFERENCE_FILES_DIR, relative_path);
    return full_path;
}


static void ensure_tmp_dir(void) {
    struct stat st;

    if (stat(TEMP_DIR, &st) == -1)
        mkdir(TEMP_DIR, 0777);
}


const char *get_temp_file_path(const char *prefix, const char *suffix) {
    ensure_tmp_dir();

    static char path[PATH_MAX];
    static char fullpath[PATH_MAX];
    int n = snprintf(path, sizeof(path), TEMP_DIR "/%sXXXXXX", prefix ? prefix : "");

    if (n < 0 || n >= sizeof(path))
        return NULL;

    int fd = mkstemp(path);

    if (fd < 0)
        return NULL;

    close(fd);

    if (suffix) {
        n = snprintf(fullpath, sizeof(fullpath), "%s%s", path, suffix);

        if (n < 0 || n >= sizeof(fullpath))
            return NULL;

        if (rename(path, fullpath) != 0) {
            unlink(path);
            return NULL;
        }

        return fullpath;
    }

    return path;
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


bool compare_files(const char *filename_a, const char *filename_b) {
    size_t len_a = 0;
    char *contents_a = read_file(filename_a, &len_a);
    size_t len_b = 0;
    char *contents_b = read_file(filename_b, &len_b);
    bool ret = false;

    if (contents_a == NULL || contents_b == NULL)
        goto cleanup;

    if (len_a != len_b)
        goto cleanup;

    ret = (memcmp(contents_a, contents_b, len_a) == 0);
cleanup:
    free(contents_a);
    free(contents_b);
    return ret;
}
