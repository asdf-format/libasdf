#ifndef ASDF_EVENT_H
#define ASDF_EVENT_H

#include <stdbool.h>
#include <stdio.h>

#include <asdf/util.h>

ASDF_BEGIN_DECLS


#define ASDF_EVENT_TYPES(X) \
    X(ASDF_NONE_EVENT) \
    X(ASDF_BEGIN_EVENT) \
    X(ASDF_ASDF_VERSION_EVENT) \
    X(ASDF_STANDARD_VERSION_EVENT) \
    X(ASDF_COMMENT_EVENT) \
    X(ASDF_TREE_START_EVENT) \
    X(ASDF_YAML_EVENT) \
    X(ASDF_TREE_END_EVENT) \
    X(ASDF_BLOCK_EVENT) \
    X(ASDF_PADDING_EVENT) \
    X(ASDF_BLOCK_INDEX_EVENT) \
    X(ASDF_END_EVENT)

typedef enum {
// clang-format off
#define X(member) member,
    ASDF_EVENT_TYPES(X)
#undef X
    ASDF_EVENT_TYPE_COUNT
    // clang-format on
} asdf_event_type_t;


typedef struct asdf_event asdf_event_t;


typedef struct asdf_block_info asdf_block_info_t;


typedef struct asdf_tree_info asdf_tree_info_t;


/* Forward declaration for asdf_parser_t */
typedef struct asdf_parser asdf_parser_t;


/* Public API functions */
ASDF_EXPORT asdf_event_type_t asdf_event_type(asdf_event_t *event);
ASDF_EXPORT const char *asdf_event_comment(const asdf_event_t *event);
ASDF_EXPORT const asdf_tree_info_t *asdf_event_tree_info(const asdf_event_t *event);
ASDF_EXPORT const asdf_block_info_t *asdf_event_block_info(const asdf_event_t *event);
ASDF_EXPORT asdf_event_t *asdf_event_iterate(asdf_parser_t *parser);
ASDF_EXPORT const char *asdf_event_type_name(asdf_event_type_t event_type);
ASDF_EXPORT void asdf_event_print(const asdf_event_t *event, FILE *file, bool verbose);
ASDF_EXPORT void asdf_event_free(asdf_parser_t *parser, asdf_event_t *event);

ASDF_END_DECLS

#endif  /* ASDF_EVENT_H */
