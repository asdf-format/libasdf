/*
 * Smoke test over the reference files from the asdf-standard submodule
 *
 * For every reference file, across all standard versions: each tagged value
 * must resolve to a registered extension and deserialize through it, and each
 * binary block carrying a checksum must have a valid one.  Values covered by
 * the exceptions below are skipped.
 */
#include <dirent.h>
#include <limits.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "munit.h"
#include "util.h"

#include "asdf.h"
#include "asdf/extension.h"
#include "asdf/version.h"


/*
 * Tagged values that cannot be read yet because the feature behind them is
 * unimplemented, keyed on the reference file's basename and the value's path
 * in the tree
 */
typedef struct {
    const char *filename;
    const char *path;
    const char *reason;
} unsupported_value_t;


static const unsupported_value_t unsupported_values[] = {
    {"exploded.asdf", "//data", "external arrays (a source referring to another file)"},
    {"stream.asdf", "//my_stream", "streamed arrays (a source of -1)"},
};


/* Returns the matching exception, or NULL if the value is expected to read */
static const unsupported_value_t *get_unsupported(const char *filename, const char *path) {
    size_t nunsupported = sizeof(unsupported_values) / sizeof(unsupported_values[0]);

    for (size_t idx = 0; idx < nunsupported; idx++) {
        const unsupported_value_t *unsupported = &unsupported_values[idx];

        if (0 == strcmp(filename, unsupported->filename) && path &&
            0 == strcmp(path, unsupported->path))
            return unsupported;
    }

    return NULL;
}


static bool value_has_tag(asdf_value_t *value) {
    return NULL != asdf_value_tag(value);
}


/* The checksum field is optional, and is left zeroed when it is not written */
static bool block_has_checksum(const unsigned char *checksum) {
    for (size_t idx = 0; idx < ASDF_BLOCK_CHECKSUM_DIGEST_SIZE; idx++) {
        if (checksum[idx])
            return true;
    }

    return false;
}


/* Returns the number of blocks in the file with an invalid checksum */
static int check_block_checksums(asdf_file_t *file, const char *relative_path) {
    int failures = 0;
    size_t nblocks = asdf_block_count(file);

    for (size_t idx = 0; idx < nblocks; idx++) {
        asdf_block_t *block = asdf_block_open(file, idx);

        if (!block) {
            munit_logf(MUNIT_LOG_WARNING, "%s: could not open block %zu", relative_path, idx);
            failures++;
            continue;
        }

        const unsigned char *checksum = asdf_block_checksum(block);

        if (!checksum || !block_has_checksum(checksum)) {
            munit_logf(
                MUNIT_LOG_INFO, "%s: block %zu has no checksum to verify", relative_path, idx);
            asdf_block_close(block);
            continue;
        }

        if (!asdf_block_checksum_verify(block, NULL)) {
            munit_logf(MUNIT_LOG_WARNING, "%s: block %zu has an invalid checksum",
                relative_path, idx);
            failures++;
        }

        asdf_block_close(block);
    }

    return failures;
}


/*
 * Returns the number of problems found in the file: tagged values that could
 * not be read, plus blocks with an invalid checksum
 */
static int check_reference_file(const char *relative_path) {
    /* The exceptions are keyed on the basename, not the version directory */
    const char *sep = strrchr(relative_path, '/');
    const char *filename = sep ? sep + 1 : relative_path;

    asdf_file_t *file = asdf_open(get_reference_file_path(relative_path), "r");

    if (!file) {
        munit_logf(MUNIT_LOG_WARNING, "%s: could not be opened", relative_path);
        return 1;
    }

    asdf_value_t *root = asdf_get_value(file, "");

    if (!root) {
        munit_logf(MUNIT_LOG_WARNING, "%s: has no tree", relative_path);
        asdf_close(file);
        return 1;
    }

    int failures = 0;
    /* The iterator yields the root itself when it matches the predicate */
    asdf_find_iter_t *iter = asdf_find_iter_init(root, value_has_tag);

    while (iter && asdf_value_find_iter_next(&iter)) {
        asdf_value_t *value = iter->value;
        const char *tag = asdf_value_tag(value);
        const char *path = asdf_value_path(value);

        const unsupported_value_t *unsupported = get_unsupported(filename, path);

        if (unsupported) {
            munit_logf(
                MUNIT_LOG_INFO,
                "%s: skipping %s at %s; libasdf does not support %s",
                relative_path,
                tag,
                path,
                unsupported->reason);
            continue;
        }

        const asdf_extension_t *ext = asdf_extension_get(file, tag);

        if (!ext) {
            munit_logf(
                MUNIT_LOG_WARNING, "%s: no extension for tag %s at %s", relative_path, tag, path);
            failures++;
            continue;
        }

        void *obj = NULL;
        asdf_value_err_t err = asdf_value_as_extension_type(value, ext, &obj);

        if (ASDF_VALUE_OK != err) {
            munit_logf(
                MUNIT_LOG_WARNING,
                "%s: failed to deserialize %s at %s (error %d)",
                relative_path,
                tag,
                path,
                (int)err);
            failures++;
            continue;
        }

        if (ext->vtab && ext->vtab->deinit)
            ext->vtab->deinit(obj);
        free(obj);
    }

    failures += check_block_checksums(file, relative_path);

    asdf_value_destroy(root);
    asdf_close(file);
    return failures;
}


static int compare_names(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}


/*
 * Returns a sorted, NULL-terminated array of the names in ``path`` for which
 * ``accept`` returns true; the caller frees it with free_names
 */
static char **list_dir(const char *path, bool (*accept)(const struct dirent *)) {
    DIR *dir = opendir(path);

    if (!dir)
        return NULL;

    size_t count = 0;
    size_t capacity = 16;
    char **names = malloc(capacity * sizeof(char *));

    if (!names) {
        closedir(dir);
        return NULL;
    }

    const struct dirent *entry = NULL;

    while ((entry = readdir(dir))) {
        if (!accept(entry))
            continue;

        if (count + 2 > capacity) {
            capacity *= 2;
            char **grown = realloc(names, capacity * sizeof(char *));

            if (!grown)
                break;

            names = grown;
        }

        names[count++] = strdup(entry->d_name);
    }

    closedir(dir);
    names[count] = NULL;
    qsort(names, count, sizeof(char *), compare_names);
    return names;
}


static void free_names(char **names) {
    for (char **name = names; name && *name; name++)
        free(*name);

    free(names);
}


static bool is_version_dir(const struct dirent *entry) {
    if (DT_DIR != entry->d_type)
        return false;

    /* The reference file directories are named for the standard version */
    asdf_version_t *version = asdf_version_parse(entry->d_name);

    if (!version)
        return false;

    /* A name that is not MAJOR.MINOR.PATCH is left all-zero */
    bool is_version = version->major || version->minor || version->patch;
    asdf_version_destroy(version);
    return is_version;
}


static bool is_asdf_file(const struct dirent *entry) {
    const char *ext = strrchr(entry->d_name, '.');
    return ext && 0 == strcmp(ext, ".asdf");
}

#define DEFINE_INT_WITH_LINENO(name, value) \
    enum { name = (value), name##_LINENO = __LINE__ }

/*
 * The reference files are discovered at runtime rather than listed here, so
 * that new ones are picked up automatically.  They are stored statically: the
 * parameter array must outlive main, and freeing it from a destructor would
 * run after LeakSanitizer has already reported it.
 */
DEFINE_INT_WITH_LINENO(MAX_REFERENCE_FILES, 256);
#define MAX_RELATIVE_PATH (NAME_MAX * 2 + 2)


static char reference_file_paths[MAX_REFERENCE_FILES][MAX_RELATIVE_PATH];
static char *reference_file_values[MAX_REFERENCE_FILES + 1];
static size_t reference_file_count = 0;
static bool reference_files_overflowed = false;


static MunitParameterEnum reference_file_params[] = {
    {"file", NULL},
    {NULL, NULL},
};


__attribute__((constructor)) static void collect_reference_files(void) {
    char **versions = list_dir(REFERENCE_FILES_DIR, is_version_dir);

    if (!versions)
        return;

    for (char **version = versions; *version; version++) {
        char **filenames = list_dir(get_reference_file_path(*version), is_asdf_file);

        if (!filenames)
            continue;

        for (char **filename = filenames; *filename; filename++) {
            if (reference_file_count >= MAX_REFERENCE_FILES) {
                reference_files_overflowed = true;
            }

            if (!reference_files_overflowed) {
                char *path = reference_file_paths[reference_file_count];
                snprintf(path, MAX_RELATIVE_PATH, "%s/%s", *version, *filename);
                reference_file_values[reference_file_count] = path;
            }
            reference_file_count++;
        }

        free_names(filenames);
    }

    free_names(versions);
    reference_file_values[reference_file_count] = NULL;
    reference_file_params[0].values = reference_file_values;
}


/* Guards against the reference files having gone missing, or outgrown the array
 *
 * If this test fails it means the number of reference files has outgrown
 * the `MAX_REFERENCE_FILES` defined above.  In that case just increase
 * `MAX_REFERENCE_FILES`.
 */
MU_TEST(reference_files_found) {
    assert_size(reference_file_count, >, 0);
    if (!reference_files_overflowed)
        return MUNIT_OK;

    munit_logf(MUNIT_LOG_ERROR, "number of reference files in %s has outgrown "
               "MAX_REFERENCE_FILES defined at %s:%d (%zu > %d); in this case "
               "the value of MAX_REFERENCE_FILES must be increased and the "
               "test recompiled", REFERENCE_FILES_DIR, __FILE__,
               MAX_REFERENCE_FILES_LINENO, reference_file_count,
               MAX_REFERENCE_FILES);
    return MUNIT_FAIL;
}


MU_TEST(reference_file) {
    const char *relative_path = munit_parameters_get(params, "file");
    assert_not_null(relative_path);
    assert_int(check_reference_file(relative_path), ==, 0);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    reference_files,
    MU_RUN_TEST(reference_files_found),
    MU_RUN_TEST(reference_file, reference_file_params)
);


MU_RUN_SUITE(reference_files);
