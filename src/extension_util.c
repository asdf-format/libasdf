#include <stdbool.h>

#include "asdf/value.h"
#include "extension_util.h"
#include "log.h"
#include "value.h"


/**
 * .. todo::
 *
 *   This routine might be useful in the public API
 */
static bool is_equivalent_type(asdf_value_type_t type, asdf_value_type_t expected) {
    switch (expected) {
    case ASDF_VALUE_UINT64:
        switch (type) {
        case ASDF_VALUE_UINT64:
        case ASDF_VALUE_UINT32:
        case ASDF_VALUE_UINT16:
        case ASDF_VALUE_UINT8:
            return true;
        default:
            return false;
        }
    case ASDF_VALUE_INT64:
        switch (type) {
        case ASDF_VALUE_INT64:
        case ASDF_VALUE_UINT32:
        case ASDF_VALUE_INT32:
        case ASDF_VALUE_UINT16:
        case ASDF_VALUE_INT16:
        case ASDF_VALUE_UINT8:
        case ASDF_VALUE_INT8:
            return true;
        default:
            return false;
        }
    case ASDF_VALUE_UINT32:
        switch (type) {
        case ASDF_VALUE_UINT32:
        case ASDF_VALUE_UINT16:
        case ASDF_VALUE_UINT8:
            return true;
        default:
            return false;
        }
    case ASDF_VALUE_INT32:
        switch (type) {
        case ASDF_VALUE_INT32:
        case ASDF_VALUE_UINT16:
        case ASDF_VALUE_INT16:
        case ASDF_VALUE_UINT8:
        case ASDF_VALUE_INT8:
            return true;
        default:
            return false;
        }
    case ASDF_VALUE_UINT16:
        switch (type) {
        case ASDF_VALUE_UINT16:
        case ASDF_VALUE_UINT8:
            return true;
        default:
            return false;
        }
    case ASDF_VALUE_INT16:
        switch (type) {
        case ASDF_VALUE_INT16:
        case ASDF_VALUE_UINT8:
        case ASDF_VALUE_INT8:
            return true;
        default:
            return false;
        }
    case ASDF_VALUE_UINT8:
        switch (type) {
        case ASDF_VALUE_UINT8:
            return true;
        default:
            return false;
        }
    case ASDF_VALUE_INT8:
        switch (type) {
        case ASDF_VALUE_INT8:
            return true;
        default:
            return false;
        }
    case ASDF_VALUE_DOUBLE:
        switch (type) {
        case ASDF_VALUE_DOUBLE:
        case ASDF_VALUE_FLOAT:
            return true;
        default:
            return false;
        }
    default:
        return type == expected;
    }
}


asdf_value_err_t asdf_get_required_property(
    asdf_value_t *mapping,
    const char *name,
    asdf_value_type_t type,
    const char *tag,
    asdf_value_t **out) {
    asdf_value_err_t err = asdf_get_optional_property(mapping, name, type, tag, out);

#ifdef ASDF_LOG_ENABLED
    if (ASDF_VALUE_ERR_NOT_FOUND == err) {
        const char *mapping_tag = asdf_value_tag(mapping);
        const char *path = asdf_value_path(mapping);
        ASDF_LOG(
            mapping->file,
            ASDF_LOG_WARN,
            "required property %s missing from %s at %s",
            name,
            mapping_tag ? mapping_tag : "mapping",
            path);
#endif
    }
    return err;
}


asdf_value_err_t asdf_get_optional_property(
    asdf_value_t *mapping,
    const char *name,
    asdf_value_type_t type,
    const char *tag,
    asdf_value_t **out) {

    asdf_value_t *prop = asdf_mapping_get(mapping, name);

    if (!prop)
        return ASDF_VALUE_ERR_NOT_FOUND;

    asdf_value_type_t prop_type = asdf_value_get_type(prop);

    if (type != ASDF_VALUE_UNKNOWN && !is_equivalent_type(prop_type, type)) {
#ifdef ASDF_LOG_ENABLED
        const char *mapping_tag = asdf_value_tag(mapping);
        const char *path = asdf_value_path(prop);
        const char *typestr = asdf_value_type_string(type);
        ASDF_LOG(
            mapping->file,
            ASDF_LOG_WARN,
            "property %s from %s at %s does not have the type required by "
            "schema: %s",
            name,
            mapping_tag ? mapping_tag : "mapping",
            path,
            typestr);
#endif
        asdf_value_destroy(prop);
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }

    // TODO: This doesn't work yet because asdf_value_t don't have type == ASDF_VALUE_EXTENSION
    // unless it's already been inferred as an extension type.  Oops!
    if (type == ASDF_VALUE_EXTENSION && tag) {
        const char *prop_tag = asdf_value_tag(prop);
        if (0 != strcmp(prop_tag, tag)) {
#ifdef ASDF_LOG_ENABLED
            const char *mapping_tag = asdf_value_tag(mapping);
            const char *path = asdf_value_path(prop);
            ASDF_LOG(
                mapping->file,
                ASDF_LOG_WARN,
                "property %s from %s at %s does not have the tag required by "
                "schema: %s",
                name,
                mapping_tag ? mapping_tag : "mapping",
                path,
                tag);
#endif
            asdf_value_destroy(prop);
            return ASDF_VALUE_ERR_TYPE_MISMATCH;
        }
    }

    *out = prop;
    return ASDF_VALUE_OK;
}
