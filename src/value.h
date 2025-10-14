#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <libfyaml.h>

#include <asdf/extension.h>
#include <asdf/util.h>
#include <asdf/value.h>

#include "file.h"


typedef struct {
    const asdf_extension_t *ext;
    void *object;
} asdf_extension_value_t;


typedef struct asdf_value {
    asdf_file_t *file;
    asdf_value_type_t type;
    asdf_value_err_t err;
    struct fy_node *node;
    const char *tag;
    bool explicit_tag_checked;
    bool extension_checked;
    union {
        bool b;
        int64_t i;
        uint64_t u;
        double d;
        asdf_extension_value_t *ext;
    } scalar;
    const char *path;
} asdf_value_t;


typedef struct _asdf_mapping_iter_impl {
    const char *key;
    asdf_value_t *value;
    void *iter;
} _asdf_mapping_iter_impl_t;


typedef _asdf_mapping_iter_impl_t *asdf_mapping_iter_t;


typedef struct _asdf_mapping_iter_impl asdf_mapping_item_t;


typedef struct _asdf_sequence_iter_impl {
    asdf_value_t *value;
    void *iter;
} _asdf_sequence_iter_impl_t;


typedef _asdf_sequence_iter_impl_t *asdf_sequence_iter_t;


ASDF_LOCAL asdf_value_t *asdf_value_create(asdf_file_t *file, struct fy_node *node);
