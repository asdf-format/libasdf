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

#include "error.h"
#include "file.h"
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


/**
 * Internal asdf_value_create that also takes the value's parent value
 *
 * This is needed to work around issues with value path resolution discussed
 * in #69.  This workaround is hopefully not permanent; I need to think of a
 * better solution to the problem.
 */
static asdf_value_t *asdf_value_create_ex(
    asdf_file_t *file, struct fy_node *node, asdf_value_t *parent, const char *key, int index) {
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

    if (parent && parent->path) {
        char *path = NULL;
        if (key) {
            if (asprintf(&path, "%s/%s", parent->path, key) == -1) {
                ASDF_LOG(file, ASDF_LOG_WARN, "failure to build value path for %s (OOM?)", key);
            } else {
                value->path = path;
            }
        } else if (index >= 0) {
            if (asprintf(&path, "%s/%d", parent->path, index) == -1) {
                ASDF_LOG(file, ASDF_LOG_WARN, "failure to build value path for %d (OOM?)", index);
            } else {
                value->path = path;
            }
        }
    }

    return value;
}


/* TODO: Create or return from a freelist that lives on the file */
asdf_value_t *asdf_value_create(asdf_file_t *file, struct fy_node *node) {
    return asdf_value_create_ex(file, node, NULL, NULL, -1);
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

    return asdf_value_create_ex(mapping->file, value, mapping, key, -1);
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
    asdf_value_t *value = asdf_value_create_ex(mapping->file, value_node, mapping, impl->key, -1);

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

    return asdf_value_create_ex(sequence->file, value, sequence, NULL, index);
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

        impl->index = -1;
        *iter = impl;
    }

    struct fy_node *value_node = fy_node_sequence_iterate(sequence->node, &impl->iter);

    if (!value_node) {
        /* Cleanup and end iteration */
        goto cleanup;
    }

    int index = ++impl->index;
    asdf_value_t *value = asdf_value_create_ex(sequence->file, value_node, sequence, NULL, index);

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


/** Generic container functions */
asdf_container_iter_t asdf_container_iter_init() {
    return NULL;
}


const char *asdf_container_item_key(asdf_container_item_t *item) {
    return item->is_mapping ? item->path.key : NULL;
}


int asdf_container_item_index(asdf_container_item_t *item) {
    return item->is_mapping ? -1 : item->path.index;
}


asdf_value_t *asdf_container_item_value(asdf_container_item_t *item) {
    return item->value;
}


void asdf_container_item_destroy(asdf_container_item_t *item) {
    if (!item)
        return;

    if (item->is_mapping) {
        item->path.key = NULL;
        free(item->iter.mapping);
    } else {
        item->path.index = -1;
        free(item->iter.sequence);
    }

    item->value = NULL;
    free(item);
}


asdf_container_item_t *asdf_container_iter(asdf_value_t *container, asdf_container_iter_t *iter) {
    if (container->type != ASDF_VALUE_MAPPING && container->type != ASDF_VALUE_SEQUENCE) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(container);
        ASDF_LOG(container->file, ASDF_LOG_WARN, "%s is not a container", path);
#endif
        return NULL;
    }

    _asdf_container_iter_impl_t *impl = *iter;

    if (NULL == impl) {
        impl = calloc(1, sizeof(_asdf_container_iter_impl_t));

        if (!impl) {
            ASDF_ERROR_OOM(container->file);
            return false;
        }

        if (ASDF_VALUE_MAPPING == container->type) {
            impl->is_mapping = true;
            impl->iter.mapping = NULL;
            impl->path.key = NULL;
        } else {
            impl->is_mapping = false;
            impl->iter.sequence = NULL;
            impl->path.index = -1;
        }

        *iter = impl;
    }

    if (impl->is_mapping) {
        asdf_mapping_item_t *item = asdf_mapping_iter(container, &impl->iter.mapping);

        if (!item)
            goto cleanup;

        impl->path.key = item->key;
        impl->value = item->value;
        return impl;
    }

    // Sequence case
    asdf_value_t *value = asdf_sequence_iter(container, &impl->iter.sequence);

    if (!value)
        goto cleanup;

    impl->path.index++;
    impl->value = value;
    return impl;

cleanup:
    asdf_container_item_destroy(impl);
    *iter = NULL;
    return NULL;
}


bool asdf_value_is_container(asdf_value_t *value) {
    return value->type == ASDF_VALUE_MAPPING || value->type == ASDF_VALUE_SEQUENCE;
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


bool asdf_value_is_type(asdf_value_t *value, asdf_value_type_t type) {
    switch (type) {
    case ASDF_VALUE_UNKNOWN:
        return false;
    case ASDF_VALUE_SEQUENCE:
        return asdf_value_is_sequence(value);
    case ASDF_VALUE_MAPPING:
        return asdf_value_is_mapping(value);
    case ASDF_VALUE_SCALAR:
        return asdf_value_is_scalar(value);
    case ASDF_VALUE_STRING:
        return asdf_value_is_string(value);
    case ASDF_VALUE_BOOL:
        return asdf_value_is_bool(value);
    case ASDF_VALUE_NULL:
        return asdf_value_is_null(value);
    case ASDF_VALUE_INT8:
        return asdf_value_is_int8(value);
    case ASDF_VALUE_INT16:
        return asdf_value_is_int16(value);
    case ASDF_VALUE_INT32:
        return asdf_value_is_int32(value);
    case ASDF_VALUE_INT64:
        return asdf_value_is_int64(value);
    case ASDF_VALUE_UINT8:
        return asdf_value_is_uint8(value);
    case ASDF_VALUE_UINT16:
        return asdf_value_is_uint16(value);
    case ASDF_VALUE_UINT32:
        return asdf_value_is_uint32(value);
    case ASDF_VALUE_UINT64:
        return asdf_value_is_uint64(value);
    case ASDF_VALUE_FLOAT:
        return asdf_value_is_float(value);
    case ASDF_VALUE_DOUBLE:
        return asdf_value_is_double(value);
    case ASDF_VALUE_EXTENSION:
        return (ASDF_VALUE_OK == asdf_value_infer_extension_type(value));
    }
    return false;
}


asdf_value_err_t asdf_value_as_type(asdf_value_t *value, asdf_value_type_t type, void *out) {
    switch (type) {
    case ASDF_VALUE_UNKNOWN:
        (*(asdf_value_t **)out) = asdf_value_clone(value);
        return ASDF_VALUE_OK;
    case ASDF_VALUE_SEQUENCE:
        if (!asdf_value_is_sequence(value))
            return ASDF_VALUE_ERR_TYPE_MISMATCH;

        (*(asdf_value_t **)out) = asdf_value_clone(value);
        return ASDF_VALUE_OK;
    case ASDF_VALUE_MAPPING:
        if (!asdf_value_is_mapping(value))
            return ASDF_VALUE_ERR_TYPE_MISMATCH;

        (*(asdf_value_t **)out) = asdf_value_clone(value);
        return ASDF_VALUE_OK;
    case ASDF_VALUE_SCALAR:
        return asdf_value_as_scalar0(value, (const char **)out);
    case ASDF_VALUE_STRING:
        return asdf_value_as_string0(value, (const char **)out);
    case ASDF_VALUE_BOOL:
        return asdf_value_as_bool(value, (bool *)out);
    case ASDF_VALUE_NULL:
        if (!asdf_value_is_null(value))
            return ASDF_VALUE_ERR_TYPE_MISMATCH;

        return ASDF_VALUE_OK;
    case ASDF_VALUE_INT8:
        return asdf_value_as_int8(value, (int8_t *)out);
    case ASDF_VALUE_INT16:
        return asdf_value_as_int16(value, (int16_t *)out);
    case ASDF_VALUE_INT32:
        return asdf_value_as_int32(value, (int32_t *)out);
    case ASDF_VALUE_INT64:
        return asdf_value_as_int64(value, (int64_t *)out);
    case ASDF_VALUE_UINT8:
        return asdf_value_as_uint8(value, (uint8_t *)out);
    case ASDF_VALUE_UINT16:
        return asdf_value_as_uint16(value, (uint16_t *)out);
    case ASDF_VALUE_UINT32:
        return asdf_value_as_uint32(value, (uint32_t *)out);
    case ASDF_VALUE_UINT64:
        return asdf_value_as_uint64(value, (uint64_t *)out);
    case ASDF_VALUE_FLOAT:
        return asdf_value_as_float(value, (float *)out);
    case ASDF_VALUE_DOUBLE:
        return asdf_value_as_double(value, (double *)out);
    case ASDF_VALUE_EXTENSION:
        // Not supported through this function so we return ERR_TYPE_MISMATCH
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }
    return ASDF_VALUE_ERR_TYPE_MISMATCH;
}

/**
 * Implementation details for `asdf_value_find_iter_ex` which is the workhorse
 * for all the other variants (`asdf_value_find_iter`, `asdf_value_find_ex`,
 * `asdf_value_find`)
 */
static _asdf_find_iter_impl_t *asdf_value_find_iter_create(
    bool depth_first, asdf_value_pred_t descend_pred, ssize_t max_depth) {
    _asdf_find_iter_impl_t *it = calloc(1, sizeof(_asdf_find_iter_impl_t));

    if (!it)
        return NULL;

    it->depth_first = depth_first;
    it->descend_pred = descend_pred;
    it->max_depth = max_depth;
    it->value = NULL;
    // Initial small frame stack, though we can also use max_depth as a
    // heuristic
    it->frame_cap = (max_depth < 0) ? 8 : ((max_depth > 256) ? 256 : max_depth + 1);
    it->frames = calloc(it->frame_cap, sizeof(_asdf_find_frame_t));
    return it;
}


/** Just doubles the frame capacity */
static inline void asdf_find_iter_push_frame(
    _asdf_find_iter_impl_t *it, asdf_value_t *container, ssize_t depth) {
    // Refuse to push a new frame if we are already at max-depth or the new
    // container doesn't match the descend predicate
    // Always allow push though if frame_count == 0; that is, the root node is
    // always searched through regardless of descend_prod
    if ((it->max_depth >= 0 && depth > it->max_depth) ||
        (it->frame_count > 0 && it->descend_pred && !it->descend_pred(container)))
        return;

    if (it->frame_count == it->frame_cap) {
        size_t new_frame_cap = it->frame_cap * 2;
        _asdf_find_frame_t *new_frames =
            realloc(it->frames, new_frame_cap * sizeof(_asdf_find_frame_t));

        if (!new_frames) {
            ASDF_ERROR_OOM(container->file);
            return;
        }

        it->frames = new_frames;
        it->frame_cap = new_frame_cap;
    }

    _asdf_find_frame_t *frame = &it->frames[it->frame_count++];
    ZERO_MEMORY(frame, sizeof(_asdf_find_frame_t));
    frame->container = container;
    frame->iter = asdf_container_iter_init();
    frame->is_mapping = container->type == ASDF_VALUE_MAPPING;
    frame->depth = depth;
}


/**
 * Pop the idx-th frame from the stack
 *
 * Breadth-first traversal can be slightly more expensive here since we have
 * to shift all the frames down
 */
static inline void asdf_find_iter_pop_frame(_asdf_find_iter_impl_t *it, size_t idx) {
    _asdf_find_frame_t *frame = &it->frames[idx];

    asdf_container_item_destroy(frame->iter);

    // Hack needed for BFS
    // TODO: Should be fixed
    if (frame->owns_container)
        asdf_value_destroy(frame->container);

    memset(frame, 0, sizeof(_asdf_find_frame_t));

    if (idx != it->frame_count - 1) {
        // Normally we only pop from the bottom in BFS (idx == 0), but let's
        // handle generic idx
        memmove(
            &it->frames[idx],
            &it->frames[idx + 1],
            (it->frame_count - idx - 1) * sizeof(_asdf_find_frame_t));
    }

    it->frame_count--;
}


/** DFS strategy for `asdf_find_iter_next` */
static asdf_value_t *asdf_find_iter_next_dfs(_asdf_find_iter_impl_t *it) {
    assert(it->frame_count > 0);
    _asdf_find_frame_t *frame = &it->frames[it->frame_count - 1];

    if (!frame->visited) {
        frame->visited = true;
        return frame->container;
    }

    asdf_container_item_t *item = NULL;
    while ((item = asdf_container_iter(frame->container, &frame->iter))) {
        if (asdf_value_is_container(item->value)) {
            asdf_find_iter_push_frame(it, item->value, frame->depth + 1);
            return NULL;
        }

        return item->value;
    }
    asdf_find_iter_pop_frame(it, it->frame_count - 1);
    return NULL;
}


/** BFS strategy for `asdf_find_iter_next` */
static asdf_value_t *asdf_find_iter_next_bfs(_asdf_find_iter_impl_t *it) {
    assert(it->frame_count > 0);
    _asdf_find_frame_t *frame = &it->frames[0];

    if (!frame->visited) {
        frame->visited = true;
        return frame->container;
    }

    asdf_container_item_t *item = NULL;
    while ((item = asdf_container_iter(frame->container, &frame->iter))) {
        if (asdf_value_is_container(item->value)) {
            // In the BFS case it is important to *clone* the value before
            // pushing it onto the stack, because the next call of
            // asdf_container_iter will destroy the original; a design
            // choice that makes sense most of the time but is a foot-gun
            // here.  Would be better if we boxed values with reference
            // counting
            asdf_find_iter_push_frame(it, asdf_value_clone(item->value), frame->depth + 1);
            it->frames[it->frame_count - 1].visited = true;
            it->frames[it->frame_count - 1].owns_container = true;
        }

        return item->value;
    }
    asdf_find_iter_pop_frame(it, 0);
    return NULL;
}

/**
 * The core of asdf_value_find_iter_ex
 *
 * We maintain our own little stack of values being descended into (similar
 * to the implementation of `asdf_info`) so that we can traverse the tree
 * to arbitrary (modulo system resource) depths without blowing the C stack.
 *
 * This is probably unnecessary for most files though may be useful in some
 * cases.  In any case I usually prefer such an approach over brute recursion.
 *
 * In fact, `asdf_info` could, and later should, be rewritten on top of this
 * if possible. `asdf_info` was written very early in the project when I was
 * just trying to get a handle on libfyaml.
 */
static asdf_value_t *asdf_find_iter_next(_asdf_find_iter_impl_t *it) {
    if (it->value && !asdf_value_is_container(it->value))
        return it->value;

    if (!it->frame_count)
        return NULL;

    // Originally had this all combined together, but I think the logic is
    // clearer if we split the DFS and BFS versions into separate subroutines
    // even though there's a lot of overlap.
    if (it->depth_first)
        return asdf_find_iter_next_dfs(it);

    return asdf_find_iter_next_bfs(it);
}


asdf_find_iter_t asdf_find_iter_init_ex(
    bool depth_first, asdf_value_pred_t descend_pred, ssize_t max_depth) {
    _asdf_find_iter_impl_t *it = asdf_value_find_iter_create(depth_first, descend_pred, max_depth);
    return it;
}


asdf_find_iter_t asdf_find_iter_init(void) {
    return asdf_find_iter_init_ex(false, NULL, -1);
}


void asdf_find_item_destroy(asdf_find_item_t *item) {
    if (!item)
        return;

    asdf_value_destroy(item->value);

    // Pop all remaining frames off the stack if any
    // This is important to do before freeing item->frames since sometimes
    // individual frames need cleanup too
    while (item->frame_count > 0)
        asdf_find_iter_pop_frame(item, item->frame_count - 1);

    free(item->frames);
    free(item);
}


asdf_find_item_t *asdf_value_find_iter(
    asdf_value_t *root, asdf_value_pred_t pred, asdf_find_iter_t *iter) {

    _asdf_find_iter_impl_t *it = *iter;

    if (!it) {
        ASDF_ERROR_OOM(root->file);
        return NULL;
    }

    if (it->frame_count == 0) {
        // Subsequent calls with the same iterator but a different root result
        // in undefined behavior.  Push the root node onto the stack to begin
        if (root->type == ASDF_VALUE_MAPPING || root->type == ASDF_VALUE_SEQUENCE)
            asdf_find_iter_push_frame(it, root, 0);
        else
            // Special case when we are given a scalar "root" value
            it->value = root;
    } else {
        it->value = NULL;
    }

    asdf_value_t *current = NULL;

    while (it->frame_count > 0 || it->value) {
        current = asdf_find_iter_next(it);
        if (current && (!pred || pred(current))) {
            // wrap in find_item_t and return
            it->value = current;
            // TODO: Build path
            return (asdf_find_item_t *)it;
        }
        it->value = NULL;
    }
    asdf_find_item_destroy(it);
    return NULL;
}


asdf_find_item_t *asdf_value_find_ex(
    asdf_value_t *root,
    asdf_value_pred_t pred,
    bool depth_first,
    asdf_value_pred_t descend_pred,
    ssize_t max_depth) {
    asdf_find_iter_t iter = asdf_find_iter_init_ex(depth_first, descend_pred, max_depth);
    return asdf_value_find_iter(root, pred, &iter);
}


asdf_find_item_t *asdf_value_find(asdf_value_t *root, asdf_value_pred_t pred) {
    return asdf_value_find_ex(root, pred, false, NULL, -1);
}


const char *asdf_find_item_path(asdf_find_item_t *item) {
    if (!item->value)
        return NULL;

    return asdf_value_path(item->value);
}


asdf_value_t *asdf_find_item_value(asdf_find_item_t *item) {
    return item->value;
}
