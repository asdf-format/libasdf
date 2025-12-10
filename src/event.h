/**
 * Parser events
 */
#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "asdf/event.h" // IWYU pragma: export

#include "types/asdf_block_index.h"
#include "util.h"


static const char *const asdf_event_type_names[] = {
#define X(name) #name,
    ASDF_EVENT_TYPES(X)
#undef X
};


typedef struct {
    char *version;
} asdf_version_t;


typedef struct asdf_tree_info {
    size_t start;
    size_t end;
    const char *buf;
} asdf_tree_info_t;


typedef struct fy_event asdf_yaml_event_t;


typedef struct asdf_event {
    asdf_event_type_t type;
    union {
        // Only if ASDF_*_VERSION_EVENT
        asdf_version_t *version;
        // Only if ASDF_COMMENT_EVENT
        char *comment;
        // Only if ASDF_TREE_*_EVENT
        asdf_tree_info_t *tree;
        // Only if ASDF_YAML_EVENT
        asdf_yaml_event_t *yaml;
        // Only if ASDF_BLOCK_EVENT
        const asdf_block_info_t *block;
        // Only if ASDF_BLOCK_INDEX_EVENT
        asdf_block_index_t *block_index;
    } payload;
} asdf_event_t;


ASDF_LOCAL char *asdf_event_summary(const asdf_event_t *event);
