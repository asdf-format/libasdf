#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <assert.h>
#include <stdint.h>

#include <libfyaml.h>

#include "asdf/parser.h" // IWYU pragma: export

#include "context.h"
#include "event.h"
#include "stream.h"
#include "types/asdf_block_info_vec.h"


#define ASDF_COMMENT_CHAR '#'
#define ASDF_ASDF_VERSION_BUFFER_SIZE 16
#define ASDF_STANDARD_VERSION_BUFFER_SIZE 16
#define ASDF_PARSER_READ_BUFFER_INIT_SIZE 512


#define ASDF_DEFAULT_BLOCK_INDEX_SIZE 8


typedef enum {
    ASDF_PARSER_STATE_INITIAL,
    ASDF_PARSER_STATE_ASDF_VERSION,
    ASDF_PARSER_STATE_STANDARD_VERSION,
    ASDF_PARSER_STATE_COMMENT,
    // This state comes after comment parsing, and tries to determine
    // if we have a YAML tree or of the next thing in the file is a block
    ASDF_PARSER_STATE_TREE_OR_BLOCK,
    // This state means we have found the start of the YAML tree
    ASDF_PARSER_STATE_TREE,
    // This state means we are generating YAML events
    ASDF_PARSER_STATE_YAML,
    ASDF_PARSER_STATE_BLOCK,
    ASDF_PARSER_STATE_PADDING,
    ASDF_PARSER_STATE_BLOCK_INDEX,
    ASDF_PARSER_STATE_END,
    ASDF_PARSER_STATE_ERROR
} asdf_parser_state_t;


typedef struct asdf_parser_tree_info {
    off_t start;
    off_t end;
    uint8_t *buf;
    size_t size;
    // Found the full YAML tree
    bool found;
    // Indicates whether the file even has a YAML tree; rare to be false but possible
    // in the case of exploded files.  This is a trinary value with a negative indicating
    // unknown, 0 false, >= 1 true.
    int8_t has_tree;
} asdf_parser_tree_info_t;


/* Structure for asdf_event_t freelist */
struct asdf_event_p {
    asdf_event_t event;
    struct asdf_event_p *next;
};


typedef struct asdf_parser_block_info {
    // Array of block infos for all blocks
    asdf_block_info_vec_t infos;
    asdf_block_index_t index;
    bool has_index;
    // Number of valid blocks
    size_t count;
    // Number of blocks found so far by the parser state machine
    // This may exclude blocks that were already found during block index validation.
    size_t found;
} asdf_parser_block_info_t;


typedef struct asdf_parser {
    asdf_base_t base;
    asdf_parser_cfg_t config;
    asdf_parser_state_t state;
    asdf_stream_t *stream;
    struct asdf_event_p *event_freelist;
    struct asdf_event_p *current_event_p;
    char asdf_version[ASDF_ASDF_VERSION_BUFFER_SIZE];
    char standard_version[ASDF_STANDARD_VERSION_BUFFER_SIZE];
    struct fy_parser *yaml_parser;
    asdf_parser_tree_info_t tree;
    asdf_parser_block_info_t block;
    bool done;
} asdf_parser_t;


static inline bool asdf_parser_has_opt(asdf_parser_t *parser, asdf_parser_opt_t opt) {
    assert(parser);
    return ((parser->config.flags & opt) == opt);
}
