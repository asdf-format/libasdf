#include "../extension_registry.h"
#include "../log.h"
#include "../util.h"
#include "../value.h"

#include "asdf.h"
#include "extension_metadata.h"
#include "software.h"


#define ASDF_CORE_EXTENSION_METADATA_TAG ASDF_CORE_TAG_PREFIX "extension_metadata-1.0.0"


static asdf_value_t *asdf_extension_metadata_serialize(
    asdf_file_t *file,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    const void *obj,
    UNUSED(const void *userdata)) {

    if (UNLIKELY(!file || !obj))
        return NULL;

    const asdf_extension_metadata_t *extension = obj;
    asdf_value_t *value = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_EMIT_FAILURE;
    asdf_mapping_t *extension_map = NULL;

    if (!extension->extension_class) {
        ASDF_LOG(
            file, ASDF_LOG_WARN, ASDF_CORE_EXTENSION_METADATA_TAG " requires an extension_class");
        goto cleanup;
    }

    extension_map = asdf_mapping_create(file);

    if (!extension_map)
        goto cleanup;

    err = asdf_mapping_set_string0(extension_map, "extension_class", extension->extension_class);

    if (err != ASDF_VALUE_OK)
        goto cleanup;

    if (extension->package) {
        asdf_value_t *package = asdf_value_of_software(file, extension->package);

        if (package)
            err = asdf_mapping_set(extension_map, "package", package);
    }

    if (err != ASDF_VALUE_OK)
        goto cleanup;

    if (extension->metadata) {
        asdf_mapping_iter_t iter = asdf_mapping_iter_init();
        asdf_mapping_item_t *item = NULL;
        while ((item = asdf_mapping_iter(extension->metadata, &iter))) {
            if (strcmp(item->key, "extension_class") == 0)
                continue;

            if (strcmp(item->key, "package") == 0)
                continue;

            err = asdf_mapping_set(extension_map, item->key, asdf_value_clone(item->value));

            if (err != ASDF_VALUE_OK)
                goto cleanup;
        }
    }

    if (err != ASDF_VALUE_OK)
        goto cleanup;

    value = asdf_value_of_mapping(extension_map);
cleanup:
    if (err != ASDF_VALUE_OK) {
        asdf_mapping_destroy(extension_map);
    }

    return value;
}


static asdf_value_err_t asdf_extension_metadata_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    const char *extension_class = NULL;
    asdf_software_t *package = NULL;
    asdf_value_t *prop = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_mapping_t *extension_map = NULL;

    if (asdf_value_as_mapping(value, &extension_map) != ASDF_VALUE_OK)
        goto failure;

    /* extension_class at a minimum is required by the schema; is absent fail to parse */
    if (!(prop = asdf_mapping_get(extension_map, "extension_class")))
        goto failure;

    if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &extension_class))
        goto failure;

    asdf_value_destroy(prop);

    prop = asdf_mapping_get(extension_map, "package");

    if (prop) {
        if (ASDF_VALUE_OK != asdf_value_as_software(prop, &package)) {
            ASDF_LOG(value->file, ASDF_LOG_WARN, "ignoring invalid package in extension_metadata");
            package = NULL;
        }

        asdf_value_destroy(prop);
    }

    asdf_extension_metadata_t *metadata = calloc(1, sizeof(asdf_extension_metadata_t));

    if (!metadata)
        return ASDF_VALUE_ERR_OOM;

    metadata->extension_class = extension_class;
    metadata->package = package;
    // Clone the mapping value into the metadata so that additional properties can be looked up on
    // it
    metadata->metadata = (asdf_mapping_t *)asdf_value_clone(value);
    *out = metadata;
    return ASDF_VALUE_OK;
failure:
    asdf_software_destroy(package);
    asdf_value_destroy(prop);
    return err;
}


static void asdf_extension_metadata_dealloc(void *value) {
    if (!value)
        return;

    asdf_extension_metadata_t *metadata = value;
    if (metadata->package)
        asdf_software_destroy((asdf_software_t *)metadata->package);

    if (metadata->metadata)
        asdf_mapping_destroy(metadata->metadata);

    free(metadata);
}


ASDF_REGISTER_EXTENSION(
    extension_metadata,
    ASDF_CORE_EXTENSION_METADATA_TAG,
    asdf_extension_metadata_t,
    &libasdf_software,
    asdf_extension_metadata_serialize,
    asdf_extension_metadata_deserialize,
    asdf_extension_metadata_dealloc,
    NULL);
