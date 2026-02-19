#include <assert.h>
#include <stddef.h>
#include <stdint.h>

#include "munit.h"
#include "util.h"

#include <asdf/core/asdf.h>
#include <asdf/core/datatype.h>
#include <asdf/core/extension_metadata.h>
#include <asdf/core/history_entry.h>
#include <asdf/core/ndarray.h>
#include <asdf/core/software.h>
#include <asdf/extension.h>
#include <asdf/file.h>
#include <asdf/value.h>


/* TODO: Should have more tests for this, for now just using one test case that's already lying
 * around...
 */
MU_TEST(extension_metadata) {
    const char *path = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);
    asdf_value_t *value = asdf_get_value(file, "history/extensions/0");
    assert_not_null(value);
    assert_true(asdf_value_is_extension_metadata(value));
    asdf_extension_metadata_t *metadata = NULL;
    assert_int(asdf_value_as_extension_metadata(value, &metadata), ==, ASDF_VALUE_OK);
    assert_not_null(metadata);
    assert_string_equal(metadata->extension_class, "asdf.extension._manifest.ManifestExtension");
    assert_null(metadata->package);
    assert_not_null(metadata->metadata);

    asdf_value_t *prop = asdf_mapping_get(metadata->metadata, "extension_uri");
    assert_not_null(prop);
    const char *s = NULL;
    assert_int(asdf_value_as_string0(prop, &s), ==, ASDF_VALUE_OK);
    assert_not_null(s);
    assert_string_equal(s, "asdf://asdf-format.org/core/extensions/core-1.6.0");
    asdf_value_destroy(prop);

    prop = asdf_mapping_get(metadata->metadata, "manifest_software");
    assert_not_null(prop);
    assert_true(asdf_value_is_software(prop));
    asdf_software_t *software = NULL;
    assert_int(asdf_value_as_software(prop, &software), ==, ASDF_VALUE_OK);
    assert_not_null(software);
    assert_string_equal(software->name, "asdf_standard");
    assert_string_equal(software->version, "1.1.1");
    asdf_software_destroy(software);
    asdf_value_destroy(prop);

    prop = asdf_mapping_get(metadata->metadata, "software");
    assert_not_null(prop);
    assert_true(asdf_value_is_software(prop));
    software = NULL;
    assert_int(asdf_value_as_software(prop, &software), ==, ASDF_VALUE_OK);
    assert_not_null(software);
    assert_string_equal(software->name, "asdf");
    assert_string_equal(software->version, "4.1.0");
    asdf_software_destroy(software);
    asdf_value_destroy(prop);

    asdf_extension_metadata_destroy(metadata);
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


// TODO: Maybe useful to have custom comparators for extension types as part
// of the extension API?
static void assert_software_equal(const asdf_software_t *software0, const asdf_software_t *software1) {
    assert_string_equal(software0->name, software1->name);
    assert_string_equal(software0->version, software1->version);
    assert_string_equal(software0->author, software1->author);
    assert_string_equal(software0->homepage, software1->homepage);
}


static void assert_extension_metadata_equal(const asdf_extension_metadata_t *extension0, const asdf_extension_metadata_t *extension1) {
    assert_string_equal(extension0->extension_class, extension1->extension_class);
    assert_software_equal(extension0->package, extension1->package);
    size_t size0 = asdf_mapping_size(extension0->metadata);
    size_t size1 = asdf_mapping_size(extension1->metadata);
    size_t size_diff = size0 > size1 ? size0 - size1 : size1 - size0;
    assert_int(size_diff, <=, 2);

    asdf_mapping_iter_t iter = asdf_mapping_iter_init();
    asdf_mapping_item_t *item = NULL;
    while ((item = asdf_mapping_iter(extension0->metadata, &iter))) {
        // TODO: Definitely could use a generic value comparison function
        // without this we can't generally compare two mappings or sequences
        // For the purposes of this test just compare strings
        const char *key = asdf_mapping_item_key(item);

        if (strcmp(key, "extension_class") == 0)
            continue;

        if (strcmp(key, "package") == 0)
            continue;

        asdf_value_t *value = asdf_mapping_item_value(item);
        asdf_value_t *other = asdf_mapping_get(extension1->metadata, key);
        assert_not_null(other);
        assert_int(asdf_value_get_type(value), ==, asdf_value_get_type(other));
        if (asdf_value_is_string(value)) {
            const char *str = NULL;
            const char *other_str = NULL;
            assert_int(asdf_value_as_string0(value, &str), ==, ASDF_VALUE_OK);
            assert_int(asdf_value_as_string0(other, &other_str), ==, ASDF_VALUE_OK);
            assert_string_equal(str, other_str);
        }

        asdf_value_destroy(other);
    }
}


static asdf_mapping_t *make_extra_meta(asdf_file_t *file, asdf_software_t* software) {
    asdf_value_t *manifest_software_val = asdf_value_of_software(file, software);
    assert_not_null(manifest_software_val);
    asdf_mapping_t *extra_meta = asdf_mapping_create(file);
    assert_not_null(extra_meta);
    asdf_value_err_t err = asdf_mapping_set_string0(
        extra_meta, "extension_uri", "asdf://asdf-format.org/core/extensions/core-1.6.0");
    assert_int(err, ==, ASDF_VALUE_OK);
    err = asdf_mapping_set(extra_meta, "manifest_software", manifest_software_val);
    assert_int(err, ==, ASDF_VALUE_OK);
    return extra_meta;
}


MU_TEST(extension_metadata_serialize) {
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    asdf_software_t manifest_software = {
        .name = "asdf_standard", .version = "1.1.1"};
    asdf_mapping_t *extra_meta = make_extra_meta(file, &manifest_software);
    asdf_extension_metadata_t extension = {
        .metadata = extra_meta, .extension_class = "asdf.extension._manifest.ManifestExtension",
        .package = &libasdf_software};
    assert_int(asdf_set_extension_metadata(file, "extension", &extension), ==, ASDF_VALUE_OK);
    asdf_mapping_destroy(extra_meta);
    assert_int(asdf_write_to(file, path), ==, 0);
    asdf_close(file);

    // Re-open file and see if it round-tripped
    file = asdf_open(path, "r");
    assert_not_null(file);

    // This is a bit of a hack but, the original extension.metadata was
    // associated with the now closed original file, so trying to read it again
    // will invoke undefined behavior.  We have to recreated it first on the
    // new file handle.  Tried earlier to write a helper for cloning values
    // between values but it turns out to be quite dangerous especially
    // after the source file is closed.
    extra_meta = make_extra_meta(file, &manifest_software);
    extension.metadata = extra_meta;
    assert_true(asdf_is_extension_metadata(file, "extension"));
    asdf_extension_metadata_t *extension_in = NULL;
    asdf_value_err_t err = asdf_get_extension_metadata(file, "extension", &extension_in);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_extension_metadata_equal(extension_in, &extension);
    asdf_mapping_destroy(extension.metadata);
    asdf_extension_metadata_destroy(extension_in);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(history_entry) {
    const char *path = get_fixture_file_path("255.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);
    asdf_value_t *value = asdf_get_value(file, "history/entries/0");
    assert_not_null(value);
    assert_true(asdf_value_is_history_entry(value));
    asdf_history_entry_t *entry = NULL;
    assert_int(asdf_value_as_history_entry(value, &entry), ==, ASDF_VALUE_OK);
    assert_not_null(entry);
    assert_string_equal(entry->description, "test file containing integers from 0 to 255 in the "
                        "block data, for simple tests against known data");
    assert_int(entry->time.tv_sec, ==, 1753271775);
    assert_int(entry->time.tv_nsec, ==, 0);
    // TODO: Oops, current test case does not include software used to write the history entry
    // A little strange that Python asdf excludes it...maybe no one cares because it's the only
    // code writing asdf history entries?  Need more test cases...
    assert_null((void *)entry->software);
    asdf_value_destroy(value);
    asdf_history_entry_destroy(entry);
    asdf_close(file);
    return MUNIT_OK;
}


static void assert_history_entry_equal(const asdf_history_entry_t *entry0, const asdf_history_entry_t *entry1) {
    assert_string_equal(entry0->description, entry1->description);
    assert_true((entry0->software == NULL) == (entry1->software == NULL));

    if (entry0->software == NULL)
        return;

    const asdf_software_t **sp0 = entry0->software;
    const asdf_software_t **sp1 = entry1->software;

    while (*sp0 || *sp1) {
        assert_true((*sp0 == NULL) == (*sp1 == NULL));
        if (*sp0 == NULL || *sp1 == NULL)
            break;
        assert_software_equal(*sp0, *sp1);
        sp0++;
        sp1++;
    }
}


MU_TEST(history_entry_serialize) {
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    // TODO: Test time serialization
    const asdf_software_t *software[2] = {&libasdf_software, NULL};
    asdf_history_entry_t entry = {.description = "description", .software = software};
    assert_int(asdf_set_history_entry(file, "entry", &entry), ==, ASDF_VALUE_OK);
    assert_int(asdf_write_to(file, path), ==, 0);
    asdf_close(file);

    // Re-open file and see if it round-tripped
    file = asdf_open(path, "r");
    assert_not_null(file);
    assert_true(asdf_is_history_entry(file, "entry"));
    asdf_history_entry_t *entry_in = NULL;
    asdf_value_err_t err = asdf_get_history_entry(file, "entry", &entry_in);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_history_entry_equal(entry_in, &entry);
    asdf_history_entry_destroy(entry_in);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(meta) {
    const char *path = get_fixture_file_path("255.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);
    // Get the root of the tree as an asdf_value_t;
    asdf_value_t *root = asdf_get_value(file, "");
    assert_not_null(root);
    assert_true(asdf_value_is_meta(root));

    asdf_meta_t *meta = NULL;
    assert_int(asdf_value_as_meta(root, &meta), ==, ASDF_VALUE_OK);
    assert_not_null(meta);
    assert_not_null(meta->asdf_library);
    assert_string_equal(meta->asdf_library->name, "asdf");

    assert_not_null((void *)meta->history.extensions);
    size_t count = 0;
    const asdf_extension_metadata_t **ep = meta->history.extensions;
    while (*ep++) {
        count++;
        assert_int(count, <=, 1);
    }
    assert_string_equal(meta->history.extensions[0]->extension_class,
                        "asdf.extension._manifest.ManifestExtension");

    assert_not_null((void *)meta->history.entries);
    count = 0;
    const asdf_history_entry_t **hep = meta->history.entries;
    while (*hep++) {
        count++;
        assert_int(count, <=, 1);
    }
    assert_string_equal(meta->history.entries[0]->description,
                        "test file containing integers from 0 to 255 in the "
                        "block data, for simple tests against known data");
    asdf_meta_destroy(meta);
    asdf_value_destroy(root);
    asdf_close(file);
    return MUNIT_OK;
}


static void assert_meta_equal(asdf_meta_t *meta0, asdf_meta_t *meta1) {
    assert_software_equal(meta0->asdf_library, meta1->asdf_library);

    assert_true((meta0->history.extensions == NULL) == (meta1->history.extensions == NULL));
    assert_true((meta0->history.entries == NULL) == (meta1->history.entries == NULL));

    if (meta0->history.extensions != NULL) {
        const asdf_extension_metadata_t **sp0 = meta0->history.extensions;
        const asdf_extension_metadata_t **sp1 = meta1->history.extensions;

        while (*sp0 || *sp1) {
            assert_true((*sp0 == NULL) == (*sp1 == NULL));
            if (*sp0 == NULL || *sp1 == NULL)
                break;
            assert_extension_metadata_equal(*sp0, *sp1);
            sp0++;
            sp1++;
        }
    }

    if (meta0->history.entries != NULL) {
        const asdf_history_entry_t **sp0 = meta0->history.entries;
        const asdf_history_entry_t **sp1 = meta1->history.entries;

        while (*sp0 || *sp1) {
            assert_true((*sp0 == NULL) == (*sp1 == NULL));
            if (*sp0 == NULL || *sp1 == NULL)
                break;
            assert_history_entry_equal(*sp0, *sp1);
            sp0++;
            sp1++;
        }
    }
}


MU_TEST(meta_serialize) {
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(NULL);

    assert_not_null(file);
    asdf_software_t manifest_software = {
        .name = "asdf_standard", .version = "1.1.1"};
    asdf_mapping_t *extra_meta = make_extra_meta(file, &manifest_software);
    assert_not_null(extra_meta);
    asdf_extension_metadata_t extension = {
        .metadata = extra_meta, .extension_class = "asdf.extension._manifest.ManifestExtension",
        .package = &libasdf_software};

    const asdf_software_t *software[2] = {&libasdf_software, NULL};
    asdf_history_entry_t entry = {.description = "description", .software = software};

    const asdf_extension_metadata_t *extensions[2] = {&extension, NULL};
    const asdf_history_entry_t *entries[2] = {&entry, NULL};
    asdf_meta_t meta = {.asdf_library = &libasdf_software, .history = {
        .extensions = extensions, .entries = entries}};

    asdf_value_err_t err = asdf_set_meta(file, "", &meta);
    assert_int(err, ==, ASDF_VALUE_OK);
    asdf_mapping_destroy(extension.metadata);
    assert_int(asdf_write_to(file, path), ==, 0);
    asdf_close(file);

    file = asdf_open(path, "r");
    assert_not_null(file);

    extra_meta = make_extra_meta(file, &manifest_software);
    assert_not_null(extra_meta);
    extension.metadata = extra_meta;

    assert_true(asdf_is_meta(file, ""));
    asdf_meta_t *meta_in = NULL;
    err = asdf_get_meta(file, "", &meta_in);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_meta_equal(meta_in, &meta);
    asdf_meta_destroy(meta_in);
    asdf_mapping_destroy(extension.metadata);
    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Parse a couple example datatypes in files
 *
 * Here we use `asdf_value_as_extension_type` since in these examples files the
 * datatype object is just embedded in an ndarray and not explicitly tagged
 */
MU_TEST(datatype) {
    const char *path = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);
    const asdf_extension_t *ext = asdf_extension_get(file, ASDF_CORE_DATATYPE_TAG);
    assert_not_null(ext);
    asdf_datatype_t *datatype = NULL;
    // We can't use asdf_get_extension_type because it *assumes* the value is
    // tagged, and so doesn't support implicitly tagged objects
    // Might be useful to have a variant of this that explicitly supports
    // implicit tags
    asdf_value_t *value = asdf_get_value(file, "data/datatype");
    assert_not_null(value);
    asdf_value_err_t err = ext->deserialize(value, NULL, (void **)&datatype);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(datatype);
    asdf_value_destroy(value);

    // asdf_datatype_t for a scalar can be cast direcyly to an asdf_scalar_datatype_t
    // TODO: Should maybe have a helper function for checking if a datatype is scalar
    assert_int(*((asdf_scalar_datatype_t *)datatype), ==, ASDF_DATATYPE_INT64);
    assert_int(datatype->size, ==, asdf_scalar_datatype_size(ASDF_DATATYPE_INT64));
    assert_null(datatype->name);
    assert_int(datatype->byteorder, ==, ASDF_BYTEORDER_LITTLE);
    assert_int(datatype->ndim, ==, 0);
    assert_null(datatype->shape);
    assert_int(datatype->nfields, ==, 0);
    assert_null(datatype->fields);
    asdf_datatype_destroy(datatype);
    asdf_close(file);

    // Test a more complex datatype
    path = get_reference_file_path("1.6.0/structured.asdf");
    file = asdf_open(path, "r");
    assert_not_null(file);
    datatype = NULL;
    value = asdf_get_value(file, "structured/datatype");
    assert_not_null(value);
    err = ext->deserialize(value, NULL, (void **)&datatype);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(datatype);
    asdf_value_destroy(value);

    assert_int(datatype->type, ==, ASDF_DATATYPE_STRUCTURED);
    // uint8 + ascii(3) + float32
    assert_int(datatype->size, ==, 1 + 3 + 4);
    assert_null(datatype->name);
    // Default to little but ignored in favor of the individual fields'
    // byte orders
    assert_int(datatype->byteorder, ==, ASDF_BYTEORDER_LITTLE);
    assert_int(datatype->ndim, ==, 0);
    assert_null(datatype->shape);
    assert_int(datatype->nfields, ==, 3);
    assert_not_null(datatype->fields);

    const asdf_datatype_t *field = &datatype->fields[0];
    assert_int(field->type, ==, ASDF_DATATYPE_UINT8);
    assert_int(field->size, ==, 1);
    assert_string_equal(field->name, "a");
    assert_int(field->byteorder, ==, ASDF_BYTEORDER_BIG);
    assert_int(field->ndim, ==, 0);
    assert_null(field->shape);
    assert_int(field->nfields, ==, 0);
    assert_null(field->fields);

    field = &datatype->fields[1];
    assert_int(field->type, ==, ASDF_DATATYPE_ASCII);
    assert_int(field->size, ==, 3);
    assert_string_equal(field->name, "b");
    assert_int(field->byteorder, ==, ASDF_BYTEORDER_BIG);
    assert_int(field->ndim, ==, 0);
    assert_null(field->shape);
    assert_int(field->nfields, ==, 0);
    assert_null(field->fields);

    field = &datatype->fields[2];
    assert_int(field->type, ==, ASDF_DATATYPE_FLOAT32);
    assert_int(field->size, ==, 4);
    assert_string_equal(field->name, "c");
    assert_int(field->byteorder, ==, ASDF_BYTEORDER_LITTLE);
    assert_int(field->ndim, ==, 0);
    assert_null(field->shape);
    assert_int(field->nfields, ==, 0);
    assert_null(field->fields);

    asdf_datatype_destroy(datatype);
    asdf_close(file);
    return MUNIT_OK;
}


static void assert_byteorder_equal(asdf_byteorder_t byteorder0, asdf_byteorder_t byteorder1) {
    byteorder0 = byteorder0 == ASDF_BYTEORDER_DEFAULT ? ASDF_BYTEORDER_LITTLE : byteorder0;
    byteorder1 = byteorder1 == ASDF_BYTEORDER_DEFAULT ? ASDF_BYTEORDER_LITTLE : byteorder1;
    assert_int(byteorder0, ==, byteorder1);
    assert_true(byteorder0 == ASDF_BYTEORDER_LITTLE || byteorder0 == ASDF_BYTEORDER_BIG);
    assert_true(byteorder1 == ASDF_BYTEORDER_LITTLE || byteorder1 == ASDF_BYTEORDER_BIG);
}


// NOLINTNEXTLINE(misc-no-recursion)
static void assert_datatype_equal(const asdf_datatype_t *datatype0, const asdf_datatype_t *datatype1) {
    assert_int(datatype0->type, ==, datatype1->type);
    // Only compare the .size field if either both are 0 or both are non-zero
    // or if the type is ASCII or UCS4; at present user-defined datatypes
    // don't require the size field to be set unless it is a string type
    if ((datatype0->size == 0 && datatype1->size == 0) ||
        (datatype0->size != 0 && datatype1->size != 0) ||
        (datatype0->type == ASDF_DATATYPE_ASCII) ||
        (datatype0->type == ASDF_DATATYPE_UCS4))
        assert_int(datatype0->size, ==, datatype1->size);

    assert_true((datatype0->name == NULL) == (datatype1->name == NULL));

    if (datatype0->name && datatype1->name)
        assert_string_equal(datatype0->name, datatype1->name);

    assert_byteorder_equal(datatype0->byteorder, datatype1->byteorder);
    assert_int(datatype0->ndim, ==, datatype1->ndim);

    if (datatype0->ndim > 0) {
        for (uint32_t idx = 0; idx < datatype0->ndim; idx++)
            assert_int(datatype0->shape[idx], ==, datatype1->shape[idx]);
    }

    assert_int(datatype0->nfields, ==, datatype1->nfields);

    if (datatype0->nfields > 0) {
        for (uint32_t idx = 0; idx < datatype0->nfields; idx++)
            assert_datatype_equal(&datatype0->fields[idx], &datatype1->fields[idx]);
    }
}


MU_TEST(datatype_serialize) {
    const asdf_datatype_t datatype = {
        .type = ASDF_DATATYPE_STRUCTURED,
        .nfields = 4,
        .fields = (const asdf_datatype_t []){
            {.name = "string", .type = ASDF_DATATYPE_ASCII, .size = 4, .byteorder = ASDF_BYTEORDER_BIG},
            {.name = "unicode", .type = ASDF_DATATYPE_UCS4, .size = 16, .byteorder = ASDF_BYTEORDER_LITTLE},
            {.name = "int", .type = ASDF_DATATYPE_INT16, .byteorder = ASDF_BYTEORDER_BIG},
            {.name = "matrix", .type = ASDF_DATATYPE_FLOAT32, .byteorder = ASDF_BYTEORDER_LITTLE,
             .ndim=2, .shape=(const uint64_t[]){3, 3}}
        }
    };
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    assert_int(asdf_set_datatype(file, "datatype", &datatype), ==, ASDF_VALUE_OK);
    assert_int(asdf_write_to(file, path), ==, 0);
    asdf_close(file);

    file = asdf_open(path, "r");
    assert_not_null(file);
    asdf_datatype_t *datatype_in = NULL;
    assert_int(asdf_get_datatype(file, "datatype", &datatype_in), ==, ASDF_VALUE_OK);
    assert_not_null(datatype_in);
    assert_datatype_equal(datatype_in, &datatype);
    asdf_datatype_destroy(datatype_in);
    asdf_close(file);

    // Test expected output bytewise to check node style formatting
    // TODO: Re-enable this; currently byte-for-byte perfect formatting isn't achievable
    // reliably: https://github.com/asdf-format/libasdf/issues/149
    //const char *fixture_filename = get_fixture_file_path("datatypes-out.asdf");
    //assert_true(compare_files(filename, fixture_filename));
    return MUNIT_OK;
}


/*
 * Very basic test of ndarray parsing; will have more comprehensive ndarray tests in their own suite
 */
MU_TEST(ndarray) {
    const char *path = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);
    assert_true(asdf_is_ndarray(file, "data"));
    asdf_ndarray_t *ndarray = NULL;
    assert_int(asdf_get_ndarray(file, "data", &ndarray), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray);
    assert_int(ndarray->source, ==, 0);
    assert_int(ndarray->ndim, ==, 1);
    assert_int(ndarray->shape[0], ==, 8);
    assert_int(ndarray->datatype.type, ==, ASDF_DATATYPE_INT64);
    assert_int(ndarray->byteorder, ==, ASDF_BYTEORDER_LITTLE);
    assert_int(ndarray->offset, ==, 0);
    assert_null(ndarray->strides);

    size_t size = 0;
    const void *data = asdf_ndarray_data_raw(ndarray, &size);
    assert_not_null(data);
    assert_int(size, ==, sizeof(int64_t) * 8);
    // The actual array in this file is just 64-bit ints 0 through 7
    for (int64_t idx = 0; idx < 8; idx++) {
        assert_int(((int64_t *)data)[idx], ==, idx);
    }
    asdf_ndarray_destroy(ndarray);
    asdf_close(file);
    return MUNIT_OK;
}


static void assert_ndarray_equal(const asdf_ndarray_t *ndarray0, const asdf_ndarray_t *ndarray1) {
    assert_int(ndarray0->source, ==, ndarray1->source);
    assert_int(ndarray0->ndim, ==, ndarray1->ndim);

    for (uint32_t idx = 0; idx < ndarray0->ndim; idx++)
        assert_int(ndarray0->shape[idx], ==, ndarray1->shape[idx]);

    assert_datatype_equal(&ndarray0->datatype, &ndarray1->datatype);
    assert_byteorder_equal(ndarray0->byteorder, ndarray1->byteorder);
    assert_int(ndarray0->offset, ==, ndarray1->offset);

    // Strides are optional
    assert_true((ndarray0->strides == NULL) == (ndarray1->strides == NULL));

    if (ndarray0->strides) {
        for (uint32_t idx = 0; idx < ndarray0->ndim; idx++)
            assert_int(ndarray0->strides[idx], ==, ndarray1->strides[idx]);
    }
}


/*
 * Basic test of ndarray serialization; this does not test the block data, just
 * serialization of the ndarray metadata to the tree
 */
MU_TEST(ndarray_serialize) {
    asdf_ndarray_t ndarray = {
        .source = 0,
        .datatype = (asdf_datatype_t){.type = ASDF_DATATYPE_INT8},
        .ndim = 2,
        .shape = (const uint64_t[]){5, 5},
        .offset = 256,
        .strides = (const int64_t[]){-1, 1}
    };

    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    void *data = asdf_ndarray_data_alloc(&ndarray);
    assert_not_null(data);
    assert_int(asdf_set_ndarray(file, "data", &ndarray), ==, ASDF_VALUE_OK);
    assert_int(asdf_write_to(file, path), ==, 0);
    asdf_close(file);
    asdf_ndarray_data_dealloc(&ndarray);

    file = asdf_open(path, "r");
    assert_not_null(file);
    asdf_ndarray_t *ndarray_in = NULL;
    assert_int(asdf_get_ndarray(file, "data", &ndarray_in), ==, ASDF_VALUE_OK);
    assert_not_null(ndarray_in);
    assert_ndarray_equal(ndarray_in, &ndarray);
    asdf_ndarray_destroy(ndarray_in);
    asdf_close(file);

    // Test expected output bytewise to check node style formatting
    // TODO: Re-enable this; currently byte-for-byte perfect formatting isn't achievable
    // reliably: https://github.com/asdf-format/libasdf/issues/149
    //const char *fixture_filename = get_fixture_file_path("ndarray-out.asdf");
    //assert_true(compare_files(filename, fixture_filename));
    return MUNIT_OK;
}


MU_TEST(software) {
    const char *path = get_reference_file_path("1.6.0/basic.asdf");
    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);
    assert_true(asdf_is_software(file, "asdf_library"));
    asdf_software_t *software = NULL;
    assert_int(asdf_get_software(file, "asdf_library", &software), ==, ASDF_VALUE_OK);
    assert_not_null(software);
    assert_string_equal(software->name, "asdf");
    assert_string_equal(software->version, "4.1.0");
    assert_string_equal(software->homepage, "http://github.com/asdf-format/asdf");
    assert_string_equal(software->author, "The ASDF Developers");
    asdf_software_destroy(software);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(software_serialize) {
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    assert_int(asdf_set_software(file, "software", &libasdf_software), ==, ASDF_VALUE_OK);
    assert_int(asdf_write_to(file, path), ==, 0);
    asdf_close(file);

    // Re-open file and see if it round-tripped
    file = asdf_open(path, "r");
    assert_not_null(file);
    assert_true(asdf_is_software(file, "software"));
    asdf_software_t *software_in = NULL;
    asdf_value_err_t err = asdf_get_software(file, "software", &software_in);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_software_equal(software_in, &libasdf_software);
    asdf_software_destroy(software_in);
    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST(test_asdf_history_entry_add) {
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);
    assert_int(asdf_history_entry_add(file, "A long time ago..."), ==, 0);
    void *buf = NULL;
    size_t size = 0;

    // Write and save the file
    asdf_write_to(file, &buf, &size);
    asdf_close(file);

    // Re-open and add another entry
    file = asdf_open((const void *)buf, size);
    assert_not_null(file);
    assert_int(asdf_history_entry_add(file, "In a galaxy far, far away..."), ==, 0);
    asdf_write_to(file, &buf, &size);
    asdf_close(file);

    // Re-open once more and read out the history entries
    // TODO: Might be useful to have a shortcut specifically for listing the
    // main history entries...
    file = asdf_open((const void *)buf, size);
    assert_not_null(file);
    asdf_meta_t *meta = NULL;
    assert_int(asdf_get_meta(file, "/", &meta), ==, ASDF_VALUE_OK);
    assert_not_null(meta);
    assert_not_null((const void *)meta->history.entries);
    assert_string_equal(meta->history.entries[0]->description, "A long time ago...");
    assert_string_equal(meta->history.entries[1]->description, "In a galaxy far, far away...");
    assert_null(meta->history.entries[2]);
    asdf_meta_destroy(meta);
    asdf_close(file);
    free(buf);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    core_extensions,
    MU_RUN_TEST(extension_metadata),
    MU_RUN_TEST(extension_metadata_serialize),
    MU_RUN_TEST(history_entry),
    MU_RUN_TEST(history_entry_serialize),
    MU_RUN_TEST(meta),
    MU_RUN_TEST(meta_serialize),
    MU_RUN_TEST(datatype),
    MU_RUN_TEST(datatype_serialize),
    MU_RUN_TEST(ndarray),
    MU_RUN_TEST(ndarray_serialize),
    MU_RUN_TEST(software),
    MU_RUN_TEST(software_serialize),
    MU_RUN_TEST(test_asdf_history_entry_add)
);


MU_RUN_SUITE(core_extensions);
