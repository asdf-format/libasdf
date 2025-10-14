#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libfyaml.h>

#include "asdf/value.h"
#include "error.h"
#include "log.h"
#include "util.h"
#include "value.h"
#include "value_util.h"


static asdf_value_type_t asdf_value_type_from_node(struct fy_node *node) {
    assert(node);
    enum fy_node_type node_type = fy_node_get_type(node);
    switch (node_type) {
    case FYNT_SCALAR:
        return ASDF_VALUE_SCALAR;
    case FYNT_MAPPING:
        return ASDF_VALUE_MAPPING;
    case FYNT_SEQUENCE:
        return ASDF_VALUE_SEQUENCE;
    default:
        UNREACHABLE();
    }
}


/* TODO: Create or return from a freelist that lives on the file */
asdf_value_t *asdf_value_create(asdf_file_t *file, struct fy_node *node) {
    asdf_value_t *value = calloc(1, sizeof(asdf_value_t));

    if (!value) {
        ASDF_ERROR_OOM(file);
        return NULL;
    }

    value->file = file;
    value->type = asdf_value_type_from_node(node);
    value->err = ASDF_VALUE_ERR_UNKNOWN;
    value->node = node;
    value->tag = NULL;
    value->explicit_tag_checked = false;
    value->extension_checked = false;
    value->path = NULL;
    return value;
}


/**
 * Helper to check if a node is the root node of the document it belongs to (if any)
 *
 * Note, this does not include copies of the original root node.  This is effectively equivalent
 * to checking ``node == node->fyd->root``.
 */
static bool is_root_node(struct fy_node *node) {
    if (!node)
        return false;

    struct fy_document *doc = fy_node_document(node);
    return doc && fy_document_root(doc) == node;
}


void asdf_value_destroy(asdf_value_t *value) {
    if (!value)
        return;

    // nodes still attached to a document should not be manually freed
    // Have to include a check if it's the root node as well; it should not be freed
    // see https://github.com/pantoniou/libfyaml/issues/143
    if (!(fy_node_is_attached(value->node) || is_root_node(value->node)))
        fy_node_free(value->node);

    free((char *)value->path);
    free((char *)value->tag);

    // Free the extension data
    // The extension object itself must be freed by the user for now, which is less than ideal.
    // In #34 let's consider some kind of reference counting mechanism for them.
    if (ASDF_VALUE_EXTENSION == value->type) {
        asdf_extension_value_t *extval = value->scalar.ext;
        free(extval);
    }

    ZERO_MEMORY(value, sizeof(asdf_value_t));
    free(value);
}


static asdf_value_t *asdf_value_clone_impl(asdf_value_t *value, bool preserve_type_inference) {
    if (!value)
        return NULL;

    asdf_value_t *new_value = malloc(sizeof(asdf_value_t));

    if (!new_value) {
        ASDF_ERROR_OOM(value->file);
        return NULL;
    }

    struct fy_node *new_node = fy_node_copy(value->file->tree, value->node);

    if (!new_node) {
        free(new_value);
        ASDF_ERROR_OOM(value->file);
        return NULL;
    }

    new_value->file = value->file;
    new_value->node = new_node;

    if (value->tag)
        new_value->tag = strdup(value->tag);
    else
        new_value->tag = NULL;

    new_value->explicit_tag_checked = value->explicit_tag_checked;
    if (value->path)
        new_value->path = strdup(value->path);
    else
        // We must look up the full path of the node to store on the clone or
        // else it will be lost; see issue #69
        new_value->path = fy_node_get_path(value->node);

    if (preserve_type_inference) {
        // If the cloned value is an extension, copy the asdf_extension_value_t, but not the
        // deserialized object--it will be re-deserialized lazily since we don't have a means
        // to clone those currently (would need to at least save their size, which is maybe good
        // to do...)
        if (ASDF_VALUE_EXTENSION == value->type && value->scalar.ext) {
            asdf_extension_value_t *new_ext = calloc(1, sizeof(asdf_extension_value_t));

            if (!new_ext) {
                fy_node_free(new_node);
                free(new_value);
                ASDF_ERROR_OOM(value->file);
                return NULL;
            }

            new_ext->ext = value->scalar.ext->ext;
            new_value->scalar.ext = new_ext;
        }

        new_value->extension_checked = value->extension_checked;
        new_value->type = value->type;
        new_value->err = value->err;
        new_value->scalar = value->scalar;
    } else {
        new_value->type = asdf_value_type_from_node(new_node);
        new_value->err = ASDF_VALUE_ERR_UNKNOWN;
        new_value->extension_checked = false;
        new_value->scalar.ext = NULL;
    }

    return new_value;
}


asdf_value_t *asdf_value_clone(asdf_value_t *value) {
    return asdf_value_clone_impl(value, true);
}


const char *asdf_value_path(asdf_value_t *value) {
    if (!value)
        return NULL;

    if (NULL == value->path) {
        // Get the path and memoize it; it can be freed when the value is freed
        value->path = fy_node_get_path(value->node);
    }

    return value->path;
}


const char *asdf_value_tag(asdf_value_t *value) {
    if (!value)
        return NULL;

    if (!value->explicit_tag_checked) {
        // Get the tag and memoize it; it can be freed when the value is freed
        size_t tag_len = 0;
        const char *maybe_tag = fy_node_get_tag(value->node, &tag_len);

        if (maybe_tag)
            value->tag = strndup(maybe_tag, tag_len);

        value->explicit_tag_checked = true;
    }

    return value->tag;
}


/* Mapping functions */
bool asdf_value_is_mapping(asdf_value_t *value) {
    return value->type == ASDF_VALUE_MAPPING;
}


int asdf_mapping_size(asdf_value_t *mapping) {
    if (UNLIKELY(!mapping) || !asdf_value_is_mapping(mapping))
        return -1;

    return fy_node_mapping_item_count(mapping->node);
}


asdf_value_t *asdf_mapping_get(asdf_value_t *mapping, const char *key) {
    if (mapping->type != ASDF_VALUE_MAPPING) {
#ifdef ASDF_LOG_ENABLED
        ASDF_LOG(mapping->file, ASDF_LOG_WARN, "%s is not a mapping", asdf_value_path(mapping));
#endif
        return NULL;
    }

    struct fy_node *value = fy_node_mapping_lookup_value_by_simple_key(mapping->node, key, -1);

    if (!value)
        return NULL;

    asdf_value_t *child = asdf_value_create(mapping->file, value);

    if (child && mapping->path) {
        char *child_path = NULL;
        if (asprintf(&child_path, "%s/%s", mapping->path, key) == -1) {
            ASDF_LOG(
                mapping->file, ASDF_LOG_WARN, "failure to build value path for %s (OOM?)", key);
        } else {
            child->path = child_path;
        }
    }

    return child;
}


asdf_mapping_iter_t asdf_mapping_iter_init() {
    return NULL;
}


const char *asdf_mapping_item_key(asdf_mapping_item_t *item) {
    return item->key;
}


asdf_value_t *asdf_mapping_item_value(asdf_mapping_item_t *item) {
    return item->value;
}


asdf_mapping_item_t *asdf_mapping_iter(asdf_value_t *mapping, asdf_mapping_iter_t *iter) {
    if (mapping->type != ASDF_VALUE_MAPPING) {
#ifdef ASDF_LOG_ENABLED
        ASDF_LOG(mapping->file, ASDF_LOG_WARN, "%s is not a mapping", asdf_value_path(mapping));
#endif
        return NULL;
    }

    _asdf_mapping_iter_impl_t *impl = *iter;

    if (NULL == impl) {
        impl = calloc(1, sizeof(_asdf_mapping_iter_impl_t));

        if (!impl) {
            ASDF_ERROR_OOM(mapping->file);
            return false;
        }

        *iter = impl;
    }

    struct fy_node_pair *pair = fy_node_mapping_iterate(mapping->node, &impl->iter);

    if (!pair) {
        /* Cleanup and end iteration */
        goto cleanup;
    }

    struct fy_node *key_node = fy_node_pair_key(pair);

    if (UNLIKELY(fy_node_get_type(key_node) != FYNT_SCALAR)) {
#ifdef ASDF_LOG_ENABLED
        ASDF_LOG(
            mapping->file,
            ASDF_LOG_WARN,
            "non-scalar key found in mapping %s; non-scalar "
            "keys are not allowed in ASDF; the value will be returned but the key will "
            "be set to NULL",
            asdf_value_path(mapping));
#endif
        impl->key = NULL;
    } else {
        impl->key = fy_node_get_scalar0(key_node);
    }

    struct fy_node *value_node = fy_node_pair_value(pair);
    asdf_value_t *value = asdf_value_create(mapping->file, value_node);

    if (!value) {
        goto cleanup;
    }

    // Destroy previous value if any
    asdf_value_destroy(impl->value);
    impl->value = value;
    return impl;

cleanup:
    impl->key = NULL;
    asdf_value_destroy(impl->value);
    impl->value = NULL;
    free(impl);
    *iter = NULL;
    return NULL;
}


/* Sequence functions */
bool asdf_value_is_sequence(asdf_value_t *value) {
    return value->type == ASDF_VALUE_SEQUENCE;
}


int asdf_sequence_size(asdf_value_t *sequence) {
    if (UNLIKELY(!sequence) || !asdf_value_is_sequence(sequence)) {
#ifdef ASDF_LOG_ENABLED
        ASDF_LOG(sequence->file, ASDF_LOG_WARN, "%s is not a sequence", asdf_value_path(sequence));
#endif
        return -1;
    }

    return fy_node_sequence_item_count(sequence->node);
}


asdf_value_t *asdf_sequence_get(asdf_value_t *sequence, int index) {
    if (UNLIKELY(!sequence) || !asdf_value_is_sequence(sequence)) {
#ifdef ASDF_LOG_ENABLED
        ASDF_LOG(sequence->file, ASDF_LOG_WARN, "%s is not a sequence", asdf_value_path(sequence));
#endif
        return NULL;
    }

    struct fy_node *value = fy_node_sequence_get_by_index(sequence->node, index);

    if (!value)
        return NULL;

    asdf_value_t *child = asdf_value_create(sequence->file, value);

    if (child && sequence->path) {
        char *child_path = NULL;
        if (asprintf(&child_path, "%s/%d", sequence->path, index) == -1) {
            ASDF_LOG(
                sequence->file, ASDF_LOG_WARN, "failure to build value path for %d (OOM?)", index);
        } else {
            child->path = child_path;
        }
    }

    return child;
}


asdf_sequence_iter_t asdf_sequence_iter_init() {
    return NULL;
}


asdf_value_t *asdf_sequence_iter(asdf_value_t *sequence, asdf_sequence_iter_t *iter) {
    if (sequence->type != ASDF_VALUE_SEQUENCE) {
#ifdef ASDF_LOG_ENABLED
        ASDF_LOG(sequence->file, ASDF_LOG_WARN, "%s is not a sequence", asdf_value_path(sequence));
#endif
        return NULL;
    }

    _asdf_sequence_iter_impl_t *impl = *iter;

    if (NULL == impl) {
        impl = calloc(1, sizeof(_asdf_sequence_iter_impl_t));

        if (!impl) {
            ASDF_ERROR_OOM(sequence->file);
            return false;
        }

        *iter = impl;
    }

    struct fy_node *value_node = fy_node_sequence_iterate(sequence->node, &impl->iter);

    if (!value_node) {
        /* Cleanup and end iteration */
        goto cleanup;
    }

    asdf_value_t *value = asdf_value_create(sequence->file, value_node);

    if (!value) {
        goto cleanup;
    }

    // Destroy previous value if any
    asdf_value_destroy(impl->value);
    impl->value = value;
    return value;

cleanup:
    asdf_value_destroy(impl->value);
    impl->value = NULL;
    free(impl);
    *iter = NULL;
    return NULL;
}


/* Extension value functions */

/**
 * Infer whether the value (scalar, mapping, sequence, doesn't matter) is a registered extension
 * type
 */
static asdf_value_err_t asdf_value_infer_extension_type(asdf_value_t *value) {
    if (UNLIKELY(!value))
        return ASDF_VALUE_ERR_UNKNOWN;

    if (value->extension_checked) {
        if (ASDF_VALUE_EXTENSION != value->type)
            return ASDF_VALUE_ERR_TYPE_MISMATCH;

        return ASDF_VALUE_OK;
    }

    value->extension_checked = true;

    const char *tag = asdf_value_tag(value);

    if (!tag)
        // Not a known extension type
        // TODO: Possibly enable the case of implicit extensions based on path
        return ASDF_VALUE_ERR_TYPE_MISMATCH;

    ASDF_LOG(value->file, ASDF_LOG_DEBUG, "inferring value as extension type for %s", tag);
    const asdf_extension_t *ext = asdf_extension_get(value->file, tag);

    if (!ext)
        return ASDF_VALUE_ERR_TYPE_MISMATCH;

    asdf_extension_value_t *ext_value = calloc(1, sizeof(asdf_extension_value_t));

    if (!ext_value) {
        ASDF_ERROR_OOM(value->file);
        return ASDF_VALUE_ERR_OOM;
    }

    ext_value->ext = ext;
    value->type = ASDF_VALUE_EXTENSION;
    // The extension value is treated as a kind of "scalar"
    value->scalar.ext = ext_value;
    return ASDF_VALUE_OK;
}


bool asdf_value_is_extension_type(asdf_value_t *value, const asdf_extension_t *ext) {
    if (ASDF_VALUE_OK != asdf_value_infer_extension_type(value))
        return false;

    return value->scalar.ext && value->scalar.ext->ext == ext;
}


asdf_value_err_t asdf_value_as_extension_type(
    asdf_value_t *value, const asdf_extension_t *ext, void **out) {
    if (!asdf_value_is_extension_type(value, ext))
        return ASDF_VALUE_ERR_TYPE_MISMATCH;

    asdf_extension_value_t *extval = value->scalar.ext;

    if (extval->object) {
        *out = extval->object;
        return ASDF_VALUE_OK;
    }

    assert(ext->deserialize);
    // Clone the raw value without existing extension inference to pass to the the extension's
    // deserialize method.
    asdf_value_t *raw_value = asdf_value_clone_impl(value, false);

    if (!raw_value) {
        ASDF_ERROR_OOM(value->file);
        return ASDF_VALUE_ERR_OOM;
    }

    asdf_value_err_t err = ext->deserialize(raw_value, extval->object, out);
    asdf_value_destroy(raw_value);

    if (ASDF_VALUE_OK == err)
        extval->object = *out;

    value->err = err;
    return err;
}


/* Scalar functions */
static bool is_yaml_null(const char *s, size_t len) {
    return (
        !s || len == 0 ||
        (len == 4 && ((strncmp(s, "null", len) == 0) || (strncmp(s, "Null", len) == 0) ||
                      (strncmp(s, "NULL", len) == 0))) ||
        (len == 1 && s[0] == '~'));
}


static bool is_yaml_bool(const char *s, size_t len, bool *value) {
    if (!s)
        return false;

    /* Allow 0 and 1 tagged as bool */
    if (len == 1) {
        if (s[0] == '0') {
            *value = false;
            return true;
        } else if (s[0] == '1') {
            *value = true;
            return true;
        }
    }

    if (len == 5 && ((0 == strncmp(s, "false", len)) || (0 == strncmp(s, "False", len)) ||
                     (0 == strncmp(s, "FALSE", len)))) {
        *value = false;
        return true;
    } else if (
        len == 4 && ((0 == strncmp(s, "true", len)) || (0 == strncmp(s, "True", len)) ||
                     (0 == strncmp(s, "TRUE", len)))) {
        *value = true;
        return true;
    }

    return false;
}


static asdf_value_err_t is_yaml_signed_int(
    const char *s, size_t len, int64_t *value, asdf_value_type_t *type) {
    if (!s)
        return ASDF_VALUE_ERR_UNKNOWN;

    char *is = strndup(s, len);

    if (!is)
        return ASDF_VALUE_ERR_UNKNOWN;

    errno = 0;
    char *end = NULL;
    int64_t v = strtoll(is, &end, 0);

    if (errno == ERANGE) {
        free(is);
        return ASDF_VALUE_ERR_OVERFLOW;
    } else if (errno || *end) {
        free(is);
        return ASDF_VALUE_ERR_PARSE_FAILURE;
    }

    /* choose smallest int that fits */
    if (v >= INT8_MIN && v <= INT8_MAX)
        *type = ASDF_VALUE_INT8;
    else if (v >= INT16_MIN && v <= INT16_MAX)
        *type = ASDF_VALUE_INT16;
    else if (v >= INT32_MIN && v <= INT32_MAX)
        *type = ASDF_VALUE_INT32;
    else
        *type = ASDF_VALUE_INT64;

    *value = v;
    free(is);
    return ASDF_VALUE_OK;
}


static asdf_value_err_t is_yaml_unsigned_int(
    const char *s, size_t len, uint64_t *value, asdf_value_type_t *type) {
    if (!s)
        return ASDF_VALUE_ERR_UNKNOWN;

    char *us = strndup(s, len);

    if (!us)
        return ASDF_VALUE_ERR_UNKNOWN;

    errno = 0;
    char *end = NULL;
    uint64_t v = strtoull(us, &end, 0);

    if (errno == ERANGE) {
        free(us);
        return ASDF_VALUE_ERR_OVERFLOW;
    } else if (errno || *end) {
        free(us);
        return ASDF_VALUE_ERR_PARSE_FAILURE;
    }

    /* choose smallest int that fits */
    if (v <= UINT8_MAX)
        *type = ASDF_VALUE_UINT8;
    else if (v <= UINT16_MAX)
        *type = ASDF_VALUE_UINT16;
    else if (v <= UINT32_MAX)
        *type = ASDF_VALUE_UINT32;
    else
        *type = ASDF_VALUE_UINT64;

    *value = v;
    free(us);
    return ASDF_VALUE_OK;
}


static asdf_value_err_t is_yaml_float(
    const char *s, size_t len, double *value, asdf_value_type_t *type) {
    if (!s)
        return ASDF_VALUE_ERR_UNKNOWN;

    char *fs = strndup(s, len);

    if (!fs)
        return ASDF_VALUE_ERR_UNKNOWN;

    errno = 0;
    char *end = NULL;
    double v = strtod(fs, &end);

    if (errno == ERANGE) {
        free(fs);
        *type = ASDF_VALUE_DOUBLE;
        return ASDF_VALUE_ERR_OVERFLOW;
    } else if (errno || *end) {
        free(fs);
        return ASDF_VALUE_ERR_PARSE_FAILURE;
    }

    float fv = (float)v;

    if ((double)fv == v)
        *type = ASDF_VALUE_FLOAT;
    else
        *type = ASDF_VALUE_DOUBLE;

    *value = v;
    free(fs);
    return ASDF_VALUE_OK;
}


static asdf_value_err_t asdf_value_infer_null(asdf_value_t *value, const char *s, size_t len) {
    if (is_yaml_null(s, len)) {
        value->type = ASDF_VALUE_NULL;
        value->err = ASDF_VALUE_OK;
        return ASDF_VALUE_OK;
    }

    value->type = ASDF_VALUE_UNKNOWN;
    value->err = ASDF_VALUE_ERR_PARSE_FAILURE;
    return ASDF_VALUE_ERR_PARSE_FAILURE;
}


static asdf_value_err_t asdf_value_infer_bool(asdf_value_t *value, const char *s, size_t len) {
    bool b_val = false;
    if (is_yaml_bool(s, len, &b_val)) {
        value->type = ASDF_VALUE_BOOL;
        value->scalar.b = b_val;
        value->err = ASDF_VALUE_OK;
        return ASDF_VALUE_OK;
    }

    value->type = ASDF_VALUE_UNKNOWN;
    value->err = ASDF_VALUE_ERR_PARSE_FAILURE;
    return ASDF_VALUE_ERR_PARSE_FAILURE;
}


static asdf_value_err_t asdf_value_infer_int(asdf_value_t *value, const char *s, size_t len) {
    uint64_t u_val = 0;
    asdf_value_type_t type = ASDF_VALUE_UNKNOWN;
    asdf_value_err_t err = is_yaml_unsigned_int(s, len, &u_val, &type);

    if (ASDF_VALUE_OK == err) {
        value->type = type;
        value->scalar.u = u_val;
    } else {
        int64_t i_val = 0;
        err = is_yaml_signed_int(s, len, &i_val, &type);

        if (ASDF_VALUE_OK == err) {
            value->type = type;
            value->scalar.i = i_val;
        } else {
            value->type = ASDF_VALUE_UNKNOWN;
        }
    }
    value->err = err;
    return err;
}


static asdf_value_err_t asdf_value_infer_float(asdf_value_t *value, const char *s, size_t len) {
    double d_val = 0.0;
    asdf_value_type_t type = ASDF_VALUE_UNKNOWN;
    asdf_value_err_t err = is_yaml_float(s, len, &d_val, &type);
    if (ASDF_VALUE_OK == err || ASDF_VALUE_ERR_OVERFLOW == err) {
        value->type = type;
        value->scalar.d = d_val;
    } else {
        value->type = ASDF_VALUE_UNKNOWN;
    }
    value->err = err;
    return err;
}


static asdf_value_err_t asdf_value_infer_scalar_type(asdf_value_t *value) {
    if (!value)
        return ASDF_VALUE_ERR_UNKNOWN;

    switch (value->type) {
    case ASDF_VALUE_SEQUENCE:
    case ASDF_VALUE_MAPPING:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    default:
        break;
    }

    if (value->type != ASDF_VALUE_SCALAR) {
        /* Has already been inferred */
        return ASDF_VALUE_OK;
    } else if (!((ASDF_VALUE_ERR_UNKNOWN == value->err) || (ASDF_VALUE_OK == value->err))) {
        return value->err;
    }

    enum fy_node_style style = fy_node_get_style(value->node);

    /* Quoted / literal styles -> always string */
    switch (style) {
    case FYNS_SINGLE_QUOTED:
    case FYNS_DOUBLE_QUOTED:
    case FYNS_LITERAL:
    case FYNS_FOLDED:
        value->type = ASDF_VALUE_STRING;
        value->err = ASDF_VALUE_OK;
        return ASDF_VALUE_OK;
    default:
        break;
    }

    size_t len = 0;
    const char *s = fy_node_get_scalar(value->node, &len);
    const char *tag_str = asdf_value_tag(value);

    /* If Common Schema tag explicitly present, honor it (but verify) */
    if (tag_str) {
        asdf_yaml_common_tag_t tag = asdf_common_tag_get(tag_str);
        asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;

#ifdef ASDF_LOG_ENABLED
        if (tag != ASDF_YAML_COMMON_TAG_UNKNOWN) {
            ASDF_LOG(
                value->file,
                ASDF_LOG_DEBUG,
                "inferring value type from YAML Common Schema tag: %s",
                tag_str);
        }
#endif

        switch (tag) {
        case ASDF_YAML_COMMON_TAG_UNKNOWN:
            break;
        case ASDF_YAML_COMMON_TAG_NULL:
            err = asdf_value_infer_null(value, s, len);
            break;
        case ASDF_YAML_COMMON_TAG_BOOL: {
            err = asdf_value_infer_bool(value, s, len);
            break;
        }
        case ASDF_YAML_COMMON_TAG_INT: {
            err = asdf_value_infer_int(value, s, len);
            break;
        }
        case ASDF_YAML_COMMON_TAG_FLOAT: {
            err = asdf_value_infer_float(value, s, len);
            break;
        }
        case ASDF_YAML_COMMON_TAG_STR:
            /* Explicitly tagged as str--just set the value type to str, no need to parse */
            value->type = ASDF_VALUE_STRING;
            err = ASDF_VALUE_OK;
            break;
        }

        if (ASDF_YAML_COMMON_TAG_UNKNOWN != tag)
            return err;
    }

    /* Untagged -> core schema heuristics (plain style only) */
    if (ASDF_VALUE_OK == asdf_value_infer_null(value, s, len)) {
        ASDF_LOG(value->file, ASDF_LOG_DEBUG, "inferred %.*s as null", len, s);
        return ASDF_VALUE_OK;
    }

    asdf_value_err_t err = asdf_value_infer_int(value, s, len);

    if (ASDF_VALUE_OK == err || ASDF_VALUE_ERR_OVERFLOW == err) {
#ifdef ASDF_LOG_ENABLED
        if (ASDF_VALUE_ERR_OVERFLOW == err)
            ASDF_LOG(value->file, ASDF_LOG_DEBUG, "inferred %.*s as int (with overflow)", len, s);
        else
            ASDF_LOG(value->file, ASDF_LOG_DEBUG, "inferred %.*s as int", len, s);
#endif
        return err;
    }

    if (ASDF_VALUE_OK == asdf_value_infer_bool(value, s, len)) {
        ASDF_LOG(value->file, ASDF_LOG_DEBUG, "inferred %.*s as bool", len, s);
        return ASDF_VALUE_OK;
    }

    err = asdf_value_infer_float(value, s, len);

    if (ASDF_VALUE_OK == err || ASDF_VALUE_ERR_OVERFLOW == err) {
#ifdef ASDF_LOG_ENABLED
        if (ASDF_VALUE_ERR_OVERFLOW == err)
            ASDF_LOG(value->file, ASDF_LOG_DEBUG, "inferred %.*s as float (with overflow)", len, s);
        else
            ASDF_LOG(value->file, ASDF_LOG_DEBUG, "inferred %.*s as float", len, s);
#endif
        return err;
    }

    /* Otherwise treat as a string */
    ASDF_LOG(value->file, ASDF_LOG_DEBUG, "inferred %.*s as string", len, s);
    value->type = ASDF_VALUE_STRING;
    value->err = ASDF_VALUE_OK;
    return ASDF_VALUE_OK;
}


asdf_value_type_t asdf_value_get_type(asdf_value_t *value) {
    if (!value)
        return ASDF_VALUE_UNKNOWN;

    if (value->type != ASDF_VALUE_SCALAR)
        return value->type;

    asdf_value_infer_scalar_type(value);
    return value->type;
}


const char *asdf_value_type_string(asdf_value_type_t type) {
    switch (type) {
    default:
    case ASDF_VALUE_UNKNOWN:
        return "<unknown>";
    case ASDF_VALUE_SEQUENCE:
        return "sequence";
    case ASDF_VALUE_MAPPING:
        return "mapping";
    case ASDF_VALUE_SCALAR:
        return "scalar";
    case ASDF_VALUE_STRING:
        return "string";
    case ASDF_VALUE_BOOL:
        return "bool";
    case ASDF_VALUE_NULL:
        return "null";
    case ASDF_VALUE_INT8:
        return "int8";
    case ASDF_VALUE_INT16:
        return "int16";
    case ASDF_VALUE_INT32:
        return "int32";
    case ASDF_VALUE_INT64:
        return "int64";
    case ASDF_VALUE_UINT8:
        return "uint8";
    case ASDF_VALUE_UINT16:
        return "uint16";
    case ASDF_VALUE_UINT32:
        return "uint32";
    case ASDF_VALUE_UINT64:
        return "uint64";
    case ASDF_VALUE_FLOAT:
        return "float";
    case ASDF_VALUE_DOUBLE:
        return "double";
    case ASDF_VALUE_EXTENSION:
        return "<extension>";
    }
}


/* Helper to define the asdf_value_is_(type) checkers */
#define __ASDF_VALUE_IS_SCALAR_TYPE(typ, value_type) \
    bool asdf_value_is_##typ(asdf_value_t *value) { \
        if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK) \
            return false; \
        return value->type == (value_type); \
    }


__ASDF_VALUE_IS_SCALAR_TYPE(string, ASDF_VALUE_STRING)


asdf_value_err_t asdf_value_as_string(asdf_value_t *value, const char **out, size_t *len) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    if (value->type != ASDF_VALUE_STRING)
        return ASDF_VALUE_ERR_TYPE_MISMATCH;

    *out = fy_node_get_scalar(value->node, len);
    return ASDF_VALUE_OK;
}


asdf_value_err_t asdf_value_as_string0(asdf_value_t *value, const char **out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    if (value->type != ASDF_VALUE_STRING)
        return ASDF_VALUE_ERR_TYPE_MISMATCH;

    *out = fy_node_get_scalar0(value->node);
    return ASDF_VALUE_OK;
}


bool asdf_value_is_scalar(asdf_value_t *value) {
    return (asdf_value_infer_scalar_type(value) == ASDF_VALUE_OK);
}


/**
 * Like `asdf_value_as_string` but returns the unparsed text of a scalar value, ignoring type
 * inference entirely
 */
asdf_value_err_t asdf_value_as_scalar(asdf_value_t *value, const char **out, size_t *len) {
    if (!value)
        return ASDF_VALUE_ERR_UNKNOWN;

    switch (value->type) {
    case ASDF_VALUE_MAPPING:
    case ASDF_VALUE_SEQUENCE:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    default:
        break;
    }

    *out = fy_node_get_scalar(value->node, len);
    return ASDF_VALUE_OK;
}


asdf_value_err_t asdf_value_as_scalar0(asdf_value_t *value, const char **out) {
    if (!value)
        return ASDF_VALUE_ERR_UNKNOWN;

    switch (value->type) {
    case ASDF_VALUE_MAPPING:
    case ASDF_VALUE_SEQUENCE:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    default:
        break;
    }

    *out = fy_node_get_scalar0(value->node);
    return ASDF_VALUE_OK;
}


bool asdf_value_is_bool(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_BOOL:
        return true;
    case ASDF_VALUE_UINT8:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT64:
        return value->scalar.u == 0 || value->scalar.u == 1;
    case ASDF_VALUE_INT8:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT64:
        return value->scalar.i == 0 || value->scalar.i == 1;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_bool(asdf_value_t *value, bool *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    /* Allow casting plain 0/1 (strictly) to bool */
    if (value->type == ASDF_VALUE_UINT8) {
        if (value->scalar.u == 0) {
            *out = value->scalar.b;
            return ASDF_VALUE_OK;
        } else if (value->scalar.u == 1) {
            *out = value->scalar.b;
            return ASDF_VALUE_OK;
        }
    } else if (value->type != ASDF_VALUE_BOOL) {
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }

    *out = value->scalar.b;
    return ASDF_VALUE_OK;
}


__ASDF_VALUE_IS_SCALAR_TYPE(null, ASDF_VALUE_NULL)


bool asdf_value_is_int(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_INT8:
    case ASDF_VALUE_UINT8:
        return true;
    default:
        return false;
    }
}


bool asdf_value_is_int8(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_INT8:
        return true;
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT64:
        return value->scalar.i >= INT8_MIN && value->scalar.u <= INT8_MAX;
    case ASDF_VALUE_UINT8:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT64:
        return value->scalar.u <= INT8_MAX;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_int8(asdf_value_t *value, int8_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        // Return anyways but indicate an overflow
        *out = (int8_t)value->scalar.i;

        if (value->scalar.i > INT8_MAX || value->scalar.i < INT8_MIN)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        *out = (int8_t)value->scalar.u;

        // Return anyways but indicate an overflow
        if (value->scalar.u > INT8_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_int16(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
    case ASDF_VALUE_UINT8:
        return true;
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT64:
        return value->scalar.i >= INT16_MIN && value->scalar.u <= INT16_MAX;
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT64:
        return value->scalar.u <= INT16_MAX;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_int16(asdf_value_t *value, int16_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        *out = (int16_t)value->scalar.i;

        if (value->scalar.i > INT16_MAX || value->scalar.i < INT16_MIN)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        *out = (int16_t)value->scalar.u;

        if (value->scalar.u > INT16_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_int32(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
    case ASDF_VALUE_UINT8:
    case ASDF_VALUE_UINT16:
        return true;
    case ASDF_VALUE_INT64:
        return value->scalar.i >= INT32_MIN && value->scalar.u <= INT32_MAX;
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT64:
        return value->scalar.u <= INT32_MAX;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_int32(asdf_value_t *value, int32_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        *out = (int32_t)value->scalar.i;

        if (value->scalar.i > INT32_MAX || value->scalar.i < INT32_MIN)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        // Return anyways but indicate an overflow
        *out = (int32_t)value->scalar.u;

        if (value->scalar.u > INT32_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_int64(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        return true;
    case ASDF_VALUE_UINT64:
        return value->scalar.u <= INT64_MAX;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_int64(asdf_value_t *value, int64_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        *out = value->scalar.i;
        return ASDF_VALUE_OK;
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        *out = (int64_t)value->scalar.u;

        if (value->scalar.u > INT64_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_uint8(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_UINT8:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT64:
        return true;
    case ASDF_VALUE_INT8:
        return value->scalar.i >= 0;
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT64:
        return value->scalar.i >= 0 && value->scalar.i <= UINT8_MAX;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_uint8(asdf_value_t *value, uint8_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_UINT8:
        *out = value->scalar.u;
        return ASDF_VALUE_OK;
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
        // Return anyways but indicate an overflow
        *out = (int64_t)value->scalar.u;

        if (value->scalar.u > UINT8_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_INT8:
        // Allow if it doesn't underflow, otherwise still return the value but
        // indicate underflow (but we use OVERFLOW here; doesn't distinguish)
        *out = value->scalar.i;

        if (value->scalar.i < 0)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
        // Return anyways but indicate an overflow
        *out = (int64_t)value->scalar.i;

        if (value->scalar.i < 0 || value->scalar.i > UINT8_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_ERR_OVERFLOW;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_uint16(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        return true;
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT64:
        return value->scalar.u <= UINT16_MAX;
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        return value->scalar.i >= 0;
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT64:
        return value->scalar.i >= 0 && value->scalar.i <= UINT16_MAX;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_uint16(asdf_value_t *value, uint16_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        *out = (uint16_t)value->scalar.u;
        return ASDF_VALUE_OK;
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
        // Return anyways but indicate an overflow
        *out = (uint16_t)value->scalar.u;

        if (value->scalar.u > UINT16_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        // Allow if it doesn't underflow, otherwise still return the value but
        // indicate underflow (but we use OVERFLOW here; doesn't distinguish)
        *out = (uint16_t)value->scalar.i;

        if (value->scalar.i < 0)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
        // Return anyways but indicate an overflow
        *out = (uint16_t)value->scalar.i;

        if (value->scalar.i < 0 || value->scalar.i > UINT16_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_uint32(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        return true;
    case ASDF_VALUE_UINT64:
        return value->scalar.u <= UINT32_MAX;
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        return value->scalar.i >= 0;
    case ASDF_VALUE_INT64:
        return value->scalar.i >= 0 && value->scalar.i <= UINT16_MAX;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_uint32(asdf_value_t *value, uint32_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        *out = (uint32_t)value->scalar.u;
        return ASDF_VALUE_OK;
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        // Allow if it doesn't underflow, otherwise still return the value but
        // indicate underflow (but we use OVERFLOW here; doesn't distinguish)
        *out = (uint32_t)value->scalar.i;

        if (value->scalar.i < 0)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_INT64:
        // Return anyways but indicate an overflow
        *out = (uint32_t)value->scalar.u;

        if (value->scalar.u > UINT32_MAX)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_uint64(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        return true;
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        return value->scalar.i >= 0;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_uint64(asdf_value_t *value, uint64_t *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_UINT64:
    case ASDF_VALUE_UINT32:
    case ASDF_VALUE_UINT16:
    case ASDF_VALUE_UINT8:
        *out = (uint64_t)value->scalar.u;
        return ASDF_VALUE_OK;
    case ASDF_VALUE_INT64:
    case ASDF_VALUE_INT32:
    case ASDF_VALUE_INT16:
    case ASDF_VALUE_INT8:
        // Allow if it doesn't underflow, otherwise still return the value but
        // indicate underflow (but we use OVERFLOW here; doesn't distinguish)
        *out = (uint64_t)value->scalar.i;

        if (value->scalar.i < 0)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


bool asdf_value_is_float(asdf_value_t *value) {
    if (asdf_value_infer_scalar_type(value) != ASDF_VALUE_OK)
        return false;

    switch (value->type) {
    case ASDF_VALUE_DOUBLE:
    case ASDF_VALUE_FLOAT:
        return true;
    default:
        return false;
    }
}


asdf_value_err_t asdf_value_as_float(asdf_value_t *value, float *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_FLOAT:
        *out = (float)value->scalar.d;
        return ASDF_VALUE_OK;
    case ASDF_VALUE_DOUBLE:
        *out = (float)value->scalar.d;

        if ((float)value->scalar.d != value->scalar.d)
            return ASDF_VALUE_ERR_OVERFLOW;

        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}


__ASDF_VALUE_IS_SCALAR_TYPE(double, ASDF_VALUE_DOUBLE)


asdf_value_err_t asdf_value_as_double(asdf_value_t *value, double *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_UNKNOWN;
    if ((err = asdf_value_infer_scalar_type(value)) != ASDF_VALUE_OK)
        return err;

    switch (value->type) {
    case ASDF_VALUE_DOUBLE:
    case ASDF_VALUE_FLOAT:
        *out = value->scalar.d;
        return ASDF_VALUE_OK;
    default:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
}
