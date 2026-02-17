#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

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


static asdf_value_err_t asdf_get_property_impl(
    asdf_mapping_t *mapping, const char *name, asdf_value_type_t type, const char *tag, void *out) {

    asdf_value_t *prop = asdf_mapping_get(mapping, name);

    if (!prop)
        return ASDF_VALUE_ERR_NOT_FOUND;

    asdf_value_type_t prop_type = asdf_value_get_type(prop);

    if (type != ASDF_VALUE_UNKNOWN && type != ASDF_VALUE_EXTENSION) {
        // NOLINTNEXTLINE(readability-suspicious-call-argument)
        if (!is_equivalent_type(prop_type, type)) {
#ifdef ASDF_LOG_ENABLED
            const char *mapping_tag = asdf_value_tag(&mapping->value);
            const char *path = asdf_value_path(prop);
            const char *typestr = asdf_value_type_string(type);
            ASDF_LOG(
                mapping->value.file,
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
        asdf_value_err_t err = asdf_value_as_type(prop, type, out);
        asdf_value_destroy(prop);
        return err;
    }

    if (type == ASDF_VALUE_EXTENSION && tag) {
        const asdf_extension_t *ext = asdf_extension_get(mapping->value.file, tag);

        if (!ext)
            return ASDF_VALUE_ERR_TYPE_MISMATCH;

        if (!asdf_value_is_extension_type(prop, ext)) {
#ifdef ASDF_LOG_ENABLED
            const char *mapping_tag = asdf_value_tag(&mapping->value);
            const char *path = asdf_value_path(prop);
            ASDF_LOG(
                mapping->value.file,
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

        asdf_value_err_t err = asdf_value_as_extension_type(prop, ext, (void **)out);
        asdf_value_destroy(prop);
        return err;
    }

    asdf_value_err_t err = asdf_value_as_type(prop, type, out);
    asdf_value_destroy(prop);
    return err;
}


asdf_value_err_t asdf_get_required_property(
    asdf_mapping_t *mapping, const char *name, asdf_value_type_t type, const char *tag, void *out) {
    asdf_value_err_t err = asdf_get_property_impl(mapping, name, type, tag, out);

#ifdef ASDF_LOG_ENABLED
    if (ASDF_VALUE_ERR_NOT_FOUND == err) {
        const char *mapping_tag = asdf_value_tag(&mapping->value);
        const char *path = asdf_value_path(&mapping->value);
        ASDF_LOG(
            mapping->value.file,
            ASDF_LOG_WARN,
            "required property %s missing from %s at %s",
            name,
            mapping_tag ? mapping_tag : "mapping",
            path);
    }
#endif
    return err;
}


asdf_value_err_t asdf_get_optional_property(
    asdf_mapping_t *mapping, const char *name, asdf_value_type_t type, const char *tag, void *out) {
    asdf_value_err_t err = asdf_get_property_impl(mapping, name, type, tag, out);

#ifdef ASDF_LOG_ENABLED
    if (ASDF_VALUE_ERR_NOT_FOUND == err) {
        const char *mapping_tag = asdf_value_tag(&mapping->value);
        const char *path = asdf_value_path(&mapping->value);
        ASDF_LOG(
            mapping->value.file,
            ASDF_LOG_DEBUG,
            "optional property %s not found in %s at %s",
            name,
            mapping_tag ? mapping_tag : "mapping",
            path);
    }
#endif
    return err;
}


/**
 * Right now we don't don any parsing of the version part of the tag, but this will
 * be useful for when we do...
static bool is_simple_version(const char *version, size_t len) {
    if (!version || !*version)
        return false;

    size_t idx = 0;

    for (int part_idx = 0; part_idx < 3; part_idx++) {
        if (!isdigit(version[idx]))
            return false;

        while (idx < len && isdigit(version[idx]))
            idx++;

        if (part_idx < 2 && (idx >= len || version[idx++] != '.'))
            return false;
    }

    return idx == len;
}
*/


asdf_tag_t *asdf_tag_parse(const char *tag) {
    if (!tag)
        return NULL;

    asdf_tag_t *res = calloc(1, sizeof(asdf_tag_t));

    if (!res)
        return NULL;

    // We assume that the version part of the tag comes after the first -
    // This is the convention that's always been used even though it's not
    // formally specified in the standard:
    // https://github.com/asdf-format/asdf-standard/issues/495
    const char *sep = strchr(tag, '-');

    if (!sep) {
        res->name = strdup(tag);
        res->version = NULL;
        return res;
    }

    size_t name_len = sep - tag;
    const char *version = sep + 1;
    res->name = strndup(tag, name_len);
    res->version = strdup(version);
    return res;
}


void asdf_tag_free(asdf_tag_t *tag) {
    if (!tag)
        return;

    free((char *)tag->name);
    free((char *)tag->version);
    free(tag);
}


void **asdf_array_concat(void **dst, const void **src) {
    size_t dst_len = 0;
    size_t src_len = 0;

    for (const void **p = src; *p; ++p)
        src_len++;

    if (!dst) {
        dst = (void **)malloc((src_len + 1) * sizeof(*src));

        if (!dst)
            return NULL;
    } else {
        for (void **p = dst; *p; ++p)
            dst_len++;
    }

    void *new_dst = realloc((void *)dst, (dst_len + src_len + 1) * sizeof(*dst));
    memcpy(new_dst + (dst_len * sizeof(*dst)), (const void *)src, src_len + 1);
    return (void **)new_dst;
}
