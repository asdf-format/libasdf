#ifndef ASDF_PARSE_H
#define ASDF_PARSE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <asdf/util.h>


ASDF_BEGIN_DECLS

// NOLINTNEXTLINE(readability-identifier-naming)
#define _ASDF_PARSER_OPTS(X) \
    X(ASDF_PARSER_OPT_EMIT_YAML_EVENTS, 0) \
    X(ASDF_PARSER_OPT_BUFFER_TREE, 1)


typedef enum {
// clang-format off
#define X(flag, bit) flag = (1UL << (bit)),
    _ASDF_PARSER_OPTS(X)
#undef X
    // clang-format on
} asdf_parser_opt_t;


// NOLINTNEXTLINE(readability-magic-numbers)
ASDF_STATIC_ASSERT(ASDF_PARSER_OPT_BUFFER_TREE < (1UL << 63), "too many flags for 64-bit int");


typedef uint64_t asdf_parser_optflags_t;


typedef struct asdf_parser_cfg {
    asdf_parser_optflags_t flags;
} asdf_parser_cfg_t;


typedef struct asdf_parser asdf_parser_t;


/* Forward declaration for asdf_event_t */
typedef struct asdf_event asdf_event_t;


/* Public API functions */
ASDF_EXPORT asdf_parser_t *asdf_parser_create(asdf_parser_cfg_t *config);
ASDF_EXPORT int asdf_parser_set_input_file(asdf_parser_t *parser, const char *filename);
ASDF_EXPORT int asdf_parser_set_input_fp(asdf_parser_t *parser, FILE *fp, const char *filename);
ASDF_EXPORT int asdf_parser_set_input_mem(asdf_parser_t *parser, const void *buf, size_t size);
ASDF_EXPORT asdf_event_t *asdf_parser_parse(asdf_parser_t *parser);
ASDF_EXPORT void asdf_parser_destroy(asdf_parser_t *parser);
ASDF_EXPORT bool asdf_parser_has_error(const asdf_parser_t *parser);
ASDF_EXPORT const char *asdf_parser_get_error(const asdf_parser_t *parser);

ASDF_END_DECLS

#endif /* ASDF_PARSE_H */
