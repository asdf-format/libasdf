/**
 * Utilities for unit tests
 */

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
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
    sg_init(1);
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


/**
 * Per-run directory: TEMP_DIR/{:06d}/
 *
 * All test binaries that share a process group (i.e. all binaries launched by
 * a single `make check` invocation) land in the same numbered subdirectory.
 * Sequential numbering makes it easy to compare between recent runs.  The
 * "latest" symlink always points to the most recently started run.
 *
 * A PGID coordination file (TEMP_DIR/.pgid-<PGID>) records the run serial and
 * persists for as long as the process group is alive.  When a test binary
 * starts it checks whether a coordination file exists for its PGID:
 *
 *   - No file (pioneer): create the next numbered run directory, write the
 *     serial to the coordination file, and update the "latest" symlink.
 *   - File present (follower): read the serial and reuse the same directory.
 *
 * At startup each binary also lazily removes coordination files whose process
 * groups are no longer alive (kill(-pgid, 0) == ESRCH).
 *
 * The run directory is never removed while the run is in progress.  It is
 * shared by every binary in the run (and, because munit forks a child per
 * test, by every one of those children).
 *
 * `make distclean` removes tmp/ entirely.
 *
 * Note: in non-interactive shells without job control, `make` inherits the
 * invoking shell's PGID rather than creating its own, so two sequential
 * `make check` calls in the same shell session may share a PGID.  The stale
 * check only fires once the shell exits, so the second run would reuse the
 * first run's directory.  This edge case is uncommon enough in practice to
 * leave unaddressed for now.
 */
static char run_dir_storage[PATH_MAX];

#define PGID_FILE_TEMPLATE ".pgid-%d"


/* Remove coordination files whose process groups no longer exist. */
static void clean_stale_pgid_files(void) {
    DIR *dir = opendir(TEMP_DIR);
    if (!dir)
        return;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        int pgid = 0;
        if (sscanf(ent->d_name, PGID_FILE_TEMPLATE, &pgid) != 1 || pgid <= 0)
            continue;

        if (kill(-(pid_t)pgid, 0) == -1 && errno == ESRCH) {
            char path[PATH_MAX];
            snprintf(path, sizeof(path), TEMP_DIR "/%s", ent->d_name);
            unlink(path);
        }
    }

    closedir(dir);
}


#define TEST_SERIAL_LEN 6
#define TEST_SERIAL_FMT "%06d"
#define TEST_SERIAL_MAX 1000000


/*
 * Try to read the run serial from an existing PGID coordination file.
 * Returns 1 and sets run_dir_storage on success, 0 otherwise.
 */
static int join_existing_run(const char *pgid_file) {
    int fd = open(pgid_file, O_RDONLY);
    if (fd < 0)
        return 0;

    char serial_str[TEST_SERIAL_LEN + 1] = {0};
    ssize_t n = read(fd, serial_str, sizeof(serial_str) - 1);
    close(fd);

    /* A short read means the pioneer has created the coordination file but
     * has not yet finished writing to it; treat it as "not ready". */
    if (n != TEST_SERIAL_LEN)
        return 0;

    snprintf(run_dir_storage, sizeof(run_dir_storage), TEMP_DIR "/%s", serial_str);
    return 1;
}


#define WAIT_FOR_PIONEER_ATTEMPTS 1000
#define WAIT_FOR_PIONEER_DELAY 5000 // usec

#define PIONEER_JOINED 1
#define PIONEER_GONE 0
#define PIONEER_TIMEOUT (-1)


/*
 * Retry joining a run after losing the O_EXCL race to the pioneer.
 * Polls the coordination file with a short backoff until the pioneer
 * writes the serial.
 *
 * Returns PIONEER_JOINED if the serial was read, PIONEER_GONE if the
 * coordination file vanished (the pioneer failed, so the caller should try
 * to claim it), or PIONEER_TIMEOUT if the pioneer never published one.
 */
static int wait_for_pioneer(const char *pgid_file) {
    for (int attempt = 0; attempt < WAIT_FOR_PIONEER_ATTEMPTS; attempt++) {
        usleep(WAIT_FOR_PIONEER_DELAY);
        if (join_existing_run(pgid_file))
            return PIONEER_JOINED;
        if (access(pgid_file, F_OK) == -1 && errno == ENOENT)
            return PIONEER_GONE;
    }
    /* Timed out; get_run_dir() falls back to TEMP_DIR. */
    return PIONEER_TIMEOUT;
}


/*
 * If the "latest" symlink points to an existing empty directory, set
 * run_dir_storage to that directory and copy its serial into serial_out.
 * Returns 1 on success (caller should reuse the directory), 0 otherwise.
 */
static int try_reuse_latest(char serial_out[TEST_SERIAL_LEN + 1]) {
    char latest_link[PATH_MAX];
    snprintf(latest_link, sizeof(latest_link), TEMP_DIR "/latest");

    char serial_str[TEST_SERIAL_LEN + 1] = {0};
    if (readlink(latest_link, serial_str, sizeof(serial_str) - 1) <= 0)
        return 0;

    char candidate[PATH_MAX];
    snprintf(candidate, sizeof(candidate), TEMP_DIR "/%s", serial_str);

    DIR *dir = opendir(candidate);
    if (!dir)
        return 0;

    int is_empty = 1;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") != 0 && strcmp(ent->d_name, "..") != 0) {
            is_empty = 0;
            break;
        }
    }
    closedir(dir);

    if (!is_empty)
        return 0;

    snprintf(run_dir_storage, sizeof(run_dir_storage), "%s", candidate);
    snprintf(serial_out, TEST_SERIAL_LEN + 1, "%s", serial_str);
    return 1;
}


/* Scan TEMP_DIR and return one past the highest existing numbered run dir. */
static int next_run_number(void) {
    int max_num = 0;
    DIR *dir = opendir(TEMP_DIR);
    if (!dir)
        return 1;

    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL) {
        int num = 0;
        if (strlen(ent->d_name) == TEST_SERIAL_LEN
                && sscanf(ent->d_name, TEST_SERIAL_FMT, &num) == 1
                && num > max_num)
            max_num = num;
    }
    closedir(dir);
    return max_num + 1;
}


/*
 * Pioneer: create the run directory, write the serial to the coordination
 * file, and update the "latest" symlink.  Cleans up pgid_file on failure.
 */
static void pioneer_setup(int fd_create, const char *pgid_file) {
    char serial_str[TEST_SERIAL_LEN + 1] = {0};

    /* Reuse the previous run directory if it is still empty. */
    if (try_reuse_latest(serial_str)) {
        if (write(fd_create, serial_str, strlen(serial_str)) < 0)
            goto failure;
        close(fd_create);
        return;
    }

    /* Find and create the next sequential run directory. */
    for (int run_num = next_run_number(); run_num < TEST_SERIAL_MAX; run_num++) {
        int n = snprintf(run_dir_storage, sizeof(run_dir_storage),
                         TEMP_DIR "/" TEST_SERIAL_FMT, run_num);
        if (n < 0 || n >= (int)sizeof(run_dir_storage))
            break;
        if (mkdir(run_dir_storage, 0777) == 0) {
            /* Use a larger buffer to avoid format-truncation: run_num is
             * bounded by TEST_SERIAL_MAX (1000000) so the output is always
             * TEST_SERIAL_LEN digits, but GCC sees the full int range. */
            char serial_buf[32];
            snprintf(serial_buf, sizeof(serial_buf), TEST_SERIAL_FMT, run_num);
            memcpy(serial_str, serial_buf, TEST_SERIAL_LEN + 1);
            if (write(fd_create, serial_str, strlen(serial_str)) < 0)
                goto failure;
            close(fd_create);
            char latest[PATH_MAX];
            snprintf(latest, sizeof(latest), TEMP_DIR "/latest");
            unlink(latest);
            int rc = symlink(serial_str, latest);  /* best-effort */
            (void)rc;
            return;
        }
        if (errno != EEXIST)
            break;
    }

failure:
    /* Failed to create a run directory or write the serial; clean up. */
    run_dir_storage[0] = '\0';
    close(fd_create);
    unlink(pgid_file);
}


#define CLAIM_RUN_ATTEMPTS 3


__attribute__((constructor))
static void init_run_dir(void) {
    ensure_tmp_dir();
    clean_stale_pgid_files();

    pid_t pgid = getpgrp();
    char pgid_file[PATH_MAX];
    snprintf(pgid_file, sizeof(pgid_file), TEMP_DIR "/" PGID_FILE_TEMPLATE, (int)pgid);

    for (int attempt = 0; attempt < CLAIM_RUN_ATTEMPTS; attempt++) {
        /* Follower: join an existing run for this PGID. */
        if (join_existing_run(pgid_file))
            return;

        /* Pioneer: atomically claim the coordination file. */
        int fd_create = open(pgid_file, O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd_create >= 0) {
            pioneer_setup(fd_create, pgid_file);
            return;
        }
        if (errno != EEXIST)
            return;

        /* Lost the race; wait for the pioneer to publish the serial.  If the
         * pioneer failed it removes the coordination file, in which case we
         * loop around and try to claim it ourselves. */
        if (wait_for_pioneer(pgid_file) != PIONEER_GONE)
            return;
    }
}


const char *get_run_dir(void) {
    /* Fallback to TEMP_DIR if the run directory could not be created. */
    return run_dir_storage[0] ? run_dir_storage : TEMP_DIR;
}


const char *get_temp_file_path(const char *prefix, const char *suffix) {
    static char fullpath[PATH_MAX];

    /* When prefix ends with '-' and suffix starts with '.' (e.g. ".asdf"),
     * strip the trailing '-' so we get "test-name.asdf" not "test-name-.asdf" */
    size_t plen = prefix ? strlen(prefix) : 0;
    if (plen > 0 && prefix[plen - 1] == '-' && suffix && suffix[0] == '.')
        plen--;

    int n = snprintf(fullpath, sizeof(fullpath), "%s/%.*s%s",
                     get_run_dir(), (int)plen, prefix ? prefix : "",
                     suffix ? suffix : "");

    if (n < 0 || n >= (int)sizeof(fullpath))
        return NULL;

    /* Create the file so it exists (matching the old mkstemp-based behaviour).
     * The run directory should already exist, but recreate it and retry once
     * if something outside the test run removed it. */
    int fd = open(fullpath, O_CREAT | O_WRONLY, 0600);
    if (fd < 0 && errno == ENOENT) {
        mkdir(get_run_dir(), 0777);
        fd = open(fullpath, O_CREAT | O_WRONLY, 0600);
    }
    if (fd >= 0)
        close(fd);

    return fullpath;
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
