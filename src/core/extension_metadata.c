#include "../extension_registry.h"
#include "../log.h"
#include "../util.h"
#include "../value.h"

#include "asdf.h"
#include "extension_metadata.h"
#include "software.h"


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
    ASDF_CORE_TAG_PREFIX "extension_metadata-1.0.0",
    asdf_extension_metadata_t,
    &libasdf_software,
    asdf_extension_metadata_deserialize,
    asdf_extension_metadata_dealloc,
    NULL);
