#include <stdlib.h>

#include <asdf/core/asdf.h>
#include <asdf/extension.h>
#include <asdf/gwcs/gwcs.h>
#include <asdf/value.h>

#include "../log.h"
#include "../util.h"
#include "../value.h"


/* Helper to look up required properties and log a warning if missing */
static asdf_value_t *get_required_property(
    asdf_value_t *mapping, const char *name, asdf_value_type_t type, const char *tag) {
    asdf_value_t *prop = asdf_mapping_get(mapping, name);
    if (!prop) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(prop);
        ASDF_LOG(
            mapping->file,
            ASDF_LOG_WARN,
            "required property %s missing from wcs at %s",
            name,
            path);
#endif
        return NULL;
    }

    if (type != ASDF_VALUE_UNKNOWN && asdf_value_get_type(prop) != type) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(prop);
        const char *typestr = asdf_value_type_string(type);
        ASDF_LOG(
            mapping->file,
            ASDF_LOG_WARN,
            "property %s from wcs at %s does not have the type required by "
            "schema: %s",
            name,
            path,
            typestr);
#endif
        return NULL;
    }

    if (type == ASDF_VALUE_EXTENSION && tag) {
        const char *prop_tag = asdf_value_tag(prop);
        if (0 != strcmp(prop_tag, tag)) {
#ifdef ASDF_LOG_ENABLED
            const char *path = asdf_value_path(prop);
            ASDF_LOG(
                mapping->file,
                ASDF_LOG_WARN,
                "property %s from wcs at %s does not have the tag required by "
                "schema: %s",
                name,
                path,
                tag);
#endif
            return NULL;
        }
    }

    return prop;
}


static asdf_value_err_t asdf_gwcs_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;

    if (!asdf_value_is_mapping(value))
        goto failure;

    asdf_gwcs_t *gwcs = calloc(1, sizeof(asdf_gwcs_t));

    if (!gwcs) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    // The name property is required
    if (!(prop = get_required_property(value, "name", ASDF_VALUE_STRING, NULL)))
        goto failure;

    if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &gwcs->name))
        goto failure;

    asdf_value_destroy(prop);

    *out = gwcs;
    return ASDF_VALUE_OK;
failure:
    asdf_value_destroy(prop);
    return err;
}


static void asdf_gwcs_dealloc(void *value) {
    if (!value)
        return;

    free(value);
}


ASDF_REGISTER_EXTENSION(
    gwcs,
    ASDF_GWCS_TAG_PREFIX "wcs-1.4.0",
    asdf_gwcs_t,
    &libasdf_software,
    asdf_gwcs_deserialize,
    asdf_gwcs_dealloc,
    NULL);
