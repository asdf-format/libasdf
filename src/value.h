#pragma once

#include <limits.h>
#include <math.h>
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


typedef struct asdf_mapping {
    asdf_value_t value;
} asdf_mapping_t;


typedef struct _asdf_mapping_iter_impl {
    const char *key;
    asdf_value_t *value;
    void *iter;
} _asdf_mapping_iter_impl_t;


typedef _asdf_mapping_iter_impl_t *asdf_mapping_iter_t;


typedef struct _asdf_mapping_iter_impl asdf_mapping_item_t;

typedef struct asdf_sequence {
    asdf_value_t value;
} asdf_sequence_t;

typedef struct asdf_sequence_iter_impl {
    asdf_value_t *value;
    int index;
    void *iter;
} asdf_sequence_iter_impl_t;


typedef void *asdf_sequence_iter_t;


/**
 * Contains both the iterator state and the current item of the iterator
 */
typedef struct asdf_container_item {
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
} asdf_container_item_t;


typedef void *asdf_container_iter_t;


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
} asdf_find_frame_t;


typedef struct asdf_find_item {
    asdf_value_t *value;
    bool depth_first;
    asdf_value_pred_t descend_pred;
    ssize_t max_depth;
    asdf_find_frame_t *frames;
    size_t frame_count;
    size_t frame_cap;
} asdf_find_item_t;


typedef void *asdf_find_iter_t;


/**
 * Inline functions pertaining to creating / writing values (specifically for
 * building scalar nodes)
 */
static inline struct fy_node *asdf_node_of_string(
    struct fy_document *doc, const char *str, size_t len) {
    if (len > INT_MAX)
        return NULL;

    return fy_node_create_scalarf(doc, "%.*s", (int)len, str);
}


static inline struct fy_node *asdf_node_of_string0(struct fy_document *doc, const char *str) {
    return fy_node_create_scalarf(doc, "%s", str);
}


static inline struct fy_node *asdf_node_of_bool(struct fy_document *doc, bool val) {
    return fy_node_create_scalarf(doc, "%s", val ? "true" : "false");
}


static inline struct fy_node *asdf_node_of_null(struct fy_document *doc) {
    return fy_node_create_scalarf(doc, "%s", "null");
}


/** Helper macro to produce asdf_node_of_<typ> functions for different scalar types */
#define ASDF_NODE_OF_VALUE_TYPE(typ, value_type, fmt) \
    static inline struct fy_node *asdf_node_of_##typ(struct fy_document *doc, value_type val) { \
        return fy_node_create_scalarf((doc), (fmt), (val)); \
    }


ASDF_NODE_OF_VALUE_TYPE(int8, int8_t, "%" PRIi8)
ASDF_NODE_OF_VALUE_TYPE(int16, int16_t, "%" PRIi16)
ASDF_NODE_OF_VALUE_TYPE(int32, int32_t, "%" PRIi32)
ASDF_NODE_OF_VALUE_TYPE(int64, int64_t, "%" PRIi64)
ASDF_NODE_OF_VALUE_TYPE(uint8, uint8_t, "%" PRIu8)
ASDF_NODE_OF_VALUE_TYPE(uint16, uint16_t, "%" PRIu16)
ASDF_NODE_OF_VALUE_TYPE(uint32, uint32_t, "%" PRIu32)
ASDF_NODE_OF_VALUE_TYPE(uint64, uint64_t, "%" PRIu64)

// Here there be Dragons!
// TODO: For now we pick some reasonable defaults to get up and running with,
// though it might be better to use something like dtoa.c https://netlib.sandia.gov/fp/dtoa.c
// which also happens to be what CPython uses
#define ASDF_NODE_OF_FLOAT_VALUE_TYPE(typ, value_type, fmt) \
    static inline struct fy_node *asdf_node_of_##typ(struct fy_document *doc, value_type val) { \
        if (isnan(val)) \
            return fy_node_create_scalarf(doc, ".nan"); \
        if (isinf(val)) { \
            if (signbit(val)) \
                return fy_node_create_scalarf(doc, "-.inf"); \
            return fy_node_create_scalarf(doc, ".inf"); \
        } \
        return fy_node_create_scalarf((doc), (fmt), (val)); \
    }


ASDF_NODE_OF_FLOAT_VALUE_TYPE(float, float, "%.9g")
ASDF_NODE_OF_FLOAT_VALUE_TYPE(double, double, "%.17g")


ASDF_LOCAL asdf_value_err_t asdf_node_insert_at(
    struct fy_document *doc, const char *path, struct fy_node *node, bool materialize);
