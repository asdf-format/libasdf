#include "../log.h"
#include "../util.h"
#include "../value.h"

#include "asdf.h"


static asdf_value_err_t asdf_software_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    const char *name = NULL;
    const char *version = NULL;
    const char *homepage = NULL;
    const char *author = NULL;
    asdf_mapping_t *software_map = NULL;
    asdf_value_t *prop = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;

    if (asdf_value_as_mapping(value, &software_map) != ASDF_VALUE_OK)
        goto failure;

    /* name and version are required; if missing or the wrong type return parse failure */
    if (!(prop = asdf_mapping_get(software_map, "name")))
        goto failure;

    if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &name))
        goto failure;

    asdf_value_destroy(prop);

    if (!(prop = asdf_mapping_get(software_map, "version")))
        goto failure;

    if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &version))
        goto failure;

    asdf_value_destroy(prop);

    prop = asdf_mapping_get(software_map, "homepage");
    if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &homepage))
        ASDF_LOG(
            value->file, ASDF_LOG_WARN, "ignoring invalid value for for homepage in software tag");

    asdf_value_destroy(prop);

    prop = asdf_mapping_get(software_map, "author");
    if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &author))
        ASDF_LOG(
            value->file, ASDF_LOG_WARN, "ignoring invalid value for for author in software tag");

    asdf_value_destroy(prop);

    asdf_software_t *software = calloc(1, sizeof(asdf_software_t));

    if (!software) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    software->name = name;
    software->version = version;
    software->homepage = homepage;
    software->author = author;
    *out = software;
    return ASDF_VALUE_OK;
failure:
    asdf_value_destroy(prop);
    return err;
}


static void asdf_software_dealloc(void *value) {
    // The strings that we pointed to in the software object are actually managed
    // by libfyaml, so they will be cleaned up automatically when we destroy the
    // fy_node
    //
    // This dealloc method is thus redundant, but included for illustration purposes.
    free(value);
}


ASDF_REGISTER_EXTENSION(
    software,
    ASDF_CORE_TAG_PREFIX "software-1.0.0",
    asdf_software_t,
    &libasdf_software,
    asdf_software_deserialize,
    asdf_software_dealloc,
    NULL);
