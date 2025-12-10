#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <libfyaml.h>

#include "asdf/extension.h"
#include "asdf/value.h" // IWYU pragma: export

#include "file.h"
#include "util.h"


typedef struct {
    const asdf_extension_t *ext;
    void *object;
} asdf_extension_value_t;


typedef struct asdf_value {
    asdf_file_t *file;
    asdf_value_type_t type;
    /** Preserve the underling YAML type even after type inference; see #75 */
    asdf_value_type_t raw_type;
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
    int index;
    void *iter;
} _asdf_sequence_iter_impl_t;


typedef _asdf_sequence_iter_impl_t *asdf_sequence_iter_t;


typedef struct _asdf_container_iter_impl {
    union {
        const char *key;
        int index;
    } path;
    asdf_value_t *value;
    bool is_mapping;
    union {
        asdf_mapping_iter_t mapping;
        asdf_sequence_iter_t sequence;
    } iter;
} _asdf_container_iter_impl_t;


typedef _asdf_container_iter_impl_t *asdf_container_iter_t;


typedef struct _asdf_container_iter_impl asdf_container_item_t;


ASDF_LOCAL asdf_value_t *asdf_value_create(asdf_file_t *file, struct fy_node *node);


typedef struct {
    asdf_value_t *container;
    // Really an ugly hack that should be fixed
    // We need versions of the mapping/sequence/container_iter routines that
    // don't destroy values between iterations, and/or better memory management
    // for values maybe with reference counting
    bool owns_container;
    bool visited;
    bool is_mapping;
    ssize_t depth;
    asdf_container_iter_t iter;
} _asdf_find_frame_t;


typedef struct _asdf_find_iter_impl {
    asdf_value_t *value;
    bool depth_first;
    asdf_value_pred_t descend_pred;
    ssize_t max_depth;
    _asdf_find_frame_t *frames;
    size_t frame_count;
    size_t frame_cap;
} _asdf_find_iter_impl_t;


typedef _asdf_find_iter_impl_t *asdf_find_iter_t;

typedef struct _asdf_find_iter_impl asdf_find_item_t;
