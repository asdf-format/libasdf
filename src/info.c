#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "block.h"
#include "event.h"
#include "info.h"
#include "parser.h"
#include "yaml.h"


#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define ANSI_DIM "\033[2m"
#define BOLD(str) ANSI_BOLD str ANSI_RESET
#define DIM(str) ANSI_DIM str ANSI_RESET


// clang-format off
typedef enum {
    TREE_SCALAR,
    TREE_MAPPING,
    TREE_SEQUENCE
} tree_node_type_t;
// clang-format on


typedef struct tree_node {
    tree_node_type_t type;

    union {
        char *key;
        size_t index;
    } index;
    char *tag;
    char *value;

    struct tree_node *parent;
    struct tree_node *first_child;
    struct tree_node *last_child;
    struct tree_node *next_sibling;

    union {
        struct {
            char *pending_key;
        } mapping;

        struct {
            size_t index;
        } sequence;
    } state;
} tree_node_t;


typedef struct tree_node_stack {
    tree_node_t *node;
    struct tree_node_stack *next;
} tree_node_stack_t;


static tree_node_t *tree_node_new(tree_node_type_t type, const char *key, size_t index) {
    tree_node_t *node = calloc(1, sizeof(tree_node_t));
    node->type = type;

    if (key) {
        node->index.key = strdup(key);
    } else {
        node->index.index = index;
    }

    switch (type) {
    case TREE_MAPPING:
        node->state.mapping.pending_key = NULL;
        break;
    case TREE_SEQUENCE:
        node->state.sequence.index = 0;
        break;
    default:
        break;
    }

    return node;
}


static void tree_node_add_child(tree_node_t *parent, tree_node_t *child) {
    child->parent = parent;
    if (!parent->first_child) {
        parent->first_child = parent->last_child = child;
    } else {
        parent->last_child->next_sibling = child;
        parent->last_child = child;
    }
}


// NOLINTNEXTLINE(misc-no-recursion)
static void tree_node_free(tree_node_t *node) {
    tree_node_t *child = node->first_child;
    while (child) {
        tree_node_t *next = child->next_sibling;
        tree_node_free(child);
        child = next;
    }

    if (node->parent && node->parent->type == TREE_MAPPING)
        free(node->index.key);

    if (node->type == TREE_MAPPING)
        free(node->state.mapping.pending_key);

    free(node->tag);
    free(node->value);
    free(node);
}


static void tree_node_set_pending_key(tree_node_t *parent, const char *new_key, size_t len) {
    if (!parent || parent->type != TREE_MAPPING)
        return;

    // Free old key if present
    if (parent->state.mapping.pending_key) {
        free(parent->state.mapping.pending_key);
        parent->state.mapping.pending_key = NULL;
    }

    if (new_key && len > 0) {
        parent->state.mapping.pending_key = strndup(new_key, len);
    } else {
        parent->state.mapping.pending_key = NULL;
    }
}


static void stack_push(tree_node_stack_t **stack, tree_node_t *node) {
    tree_node_stack_t *new_item = malloc(sizeof(tree_node_stack_t));
    new_item->node = node;
    new_item->next = *stack;
    *stack = new_item;
}


static tree_node_t *stack_pop(tree_node_stack_t **stack) {
    tree_node_stack_t *top = *stack;

    if (!top)
        return NULL;

    tree_node_t *node = top->node;
    *stack = top->next;
    free(top);
    return node;
}


static tree_node_t *stack_peek(tree_node_stack_t *stack) {
    return stack ? stack->node : NULL;
}


static tree_node_t *build_tree(asdf_parser_t *parser) {
    asdf_event_t *event = NULL;
    tree_node_t *root = NULL;
    tree_node_stack_t *stack = NULL;
    const char *tag = NULL;
    size_t tag_len = 0;

    while ((event = asdf_event_iterate(parser))) {
        asdf_yaml_event_type_t type = asdf_yaml_event_type(event);
        tree_node_t *parent = stack_peek(stack);
        tree_node_t *node = NULL;

        if (type == ASDF_YAML_STREAM_END_EVENT) {
            asdf_event_free(parser, event);
            break;
        }

        switch (type) {
        /* TODO: Handle anchor events */
        case ASDF_YAML_MAPPING_START_EVENT:
        case ASDF_YAML_SEQUENCE_START_EVENT: {
            tree_node_type_t node_type = (type == ASDF_YAML_MAPPING_START_EVENT) ? TREE_MAPPING
                                                                                 : TREE_SEQUENCE;
            const char *key = NULL;
            size_t index = 0;

            if (parent == NULL) {
                root = tree_node_new(node_type, NULL, 0);
                node = root;
            } else {
                switch (parent->type) {
                case TREE_MAPPING:
                    key = parent->state.mapping.pending_key;
                    break;
                case TREE_SEQUENCE:
                    index = parent->state.sequence.index++;
                default:
                    break;
                }
                node = tree_node_new(node_type, key, index);
                tree_node_set_pending_key(parent, NULL, 0);
                tree_node_add_child(parent, node);
            }

            tag = asdf_yaml_event_tag(event, &tag_len);

            if (tag && tag_len > 0) {
                node->tag = strndup(tag, tag_len);
            }

            stack_push(&stack, node);
            break;
        }

        case ASDF_YAML_MAPPING_END_EVENT:
        case ASDF_YAML_SEQUENCE_END_EVENT:
            stack_pop(&stack);
            break;

        case ASDF_YAML_SCALAR_EVENT: {
            if (!parent)
                continue;

            const char *key = NULL;
            size_t index = 0;
            size_t val_len = 0;
            const char *value = asdf_yaml_event_scalar_value(event, &val_len);
            tag = asdf_yaml_event_tag(event, &tag_len);

            if (parent->type == TREE_MAPPING && !parent->state.mapping.pending_key) {
                tree_node_set_pending_key(parent, value, val_len);
                continue;
            }

            switch (parent->type) {
            case TREE_MAPPING:
                key = parent->state.mapping.pending_key;
                break;
            case TREE_SEQUENCE:
                index = parent->state.sequence.index++;
                break;
            default:
                break;
            }

            node = tree_node_new(TREE_SCALAR, key, index);
            tree_node_set_pending_key(parent, NULL, 0);
            node->value = strndup(value, val_len);
            if (tag && tag_len > 0)
                node->tag = strndup(tag, tag_len);
            tree_node_add_child(parent, node);
            break;
        }

        default:
            // ignore other ASDF event types for now
            break;
        }
    }

    // Defensive cleanup of the stack; unlikely to be needed but possible in case of
    // an error or malformed document
    while (stack != NULL)
        stack_pop(&stack);

    return root;
}


// NOLINTNEXTLINE(misc-no-recursion)
static void print_indent(FILE *file, const tree_node_t *node) {
    if (!node)
        return;

    print_indent(file, node->parent);

    if (node->parent && node->parent->parent) {
        if (node->parent->next_sibling)
            fprintf(file, DIM("│") " ");
        else
            fprintf(file, "  ");
    }
}


// NOLINTNEXTLINE(misc-no-recursion)
void print_tree(FILE *file, const tree_node_t *node) {
    if (!node)
        return;

    print_indent(file, node);

    if (node->parent) {
        fprintf(file, node->next_sibling ? DIM("├─") : DIM("└─"));

        switch (node->parent->type) {
        case TREE_MAPPING:
            fprintf(file, BOLD("%s") " ", node->index.key);
            break;
        case TREE_SEQUENCE:
            fprintf(file, "[" BOLD("%zu") "] ", node->index.index);
            break;
        default:
            break;
        }
    } else {
        fprintf(file, BOLD("root") " ");
    }

    if (node->tag) {
        fprintf(file, "(%s)", node->tag);
    } else {
        switch (node->type) {
        case TREE_SCALAR:
            fprintf(file, "(scalar)");
            break;
        case TREE_MAPPING:
            fprintf(file, "(mapping)");
            break;
        case TREE_SEQUENCE:
            fprintf(file, "(sequence)");
            break;
        }
    }

    if (node->value)
        fprintf(file, ": %s", node->value);

    fprintf(file, "\n");

    const tree_node_t *child = node->first_child;
    while (child) {
        print_tree(file, child);
        child = child->next_sibling;
    }
}


// clang-format off
typedef enum {
    TOP,
    MIDDLE,
    BOTTOM
} field_border_t;


typedef enum {
    LEFT,
    CENTER,
} field_align_t;
// clang-format on


#define BOX_WIDTH 50


void print_border(FILE *file, field_border_t border) {
    fprintf(file, ANSI_DIM);
    for (int idx = 0; idx < BOX_WIDTH; idx++) {
        switch (border) {
        case TOP:
            if (idx == 0)
                fprintf(file, "┌");
            else if (idx == BOX_WIDTH - 1)
                fprintf(file, "┐\n");
            else
                fprintf(file, "─");
            break;
        case MIDDLE:
            if (idx == 0)
                fprintf(file, "├");
            else if (idx == BOX_WIDTH - 1)
                fprintf(file, "┤\n");
            else
                fprintf(file, "─");
            break;
        case BOTTOM:
            if (idx == 0)
                fprintf(file, "└");
            else if (idx == BOX_WIDTH - 1)
                fprintf(file, "┘\n");
            else
                fprintf(file, "─");
            break;
        }
    }
    fprintf(file, ANSI_RESET);
}


void print_field(FILE *file, field_align_t align, const char *fmt, ...) {
    va_list args;
    char field_buf[BOX_WIDTH - 2] = {0};
    va_start(args, fmt);
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    vsnprintf(field_buf, sizeof(field_buf), fmt, args);
    fprintf(file, DIM("│"));
    switch (align) {
    case LEFT: {
        int pad = BOX_WIDTH - (int)strlen(field_buf) - 3;
        fprintf(file, " %s%*s", field_buf, pad, "");
        break;
    }
    case CENTER: {
        int pad = (BOX_WIDTH - (int)strlen(field_buf)) / 2 - 1;
        fprintf(file, "%*s%s%*s", pad, "", field_buf, pad, "");
        break;
    }
    }
    fprintf(file, DIM("│") "\n");
}


void print_block(FILE *file, const asdf_event_t *event, size_t block_idx) {
    const asdf_block_info_t *block = asdf_event_block_info(event);

    if (!block)
        return;

    asdf_block_header_t header = block->header;

    // Print the block header with the block number
    print_border(file, TOP);
    print_field(file, CENTER, "Block #%zu", block_idx);
    print_border(file, MIDDLE);
    print_field(file, LEFT, "flags: 0x%08x", header.flags);
    print_border(file, MIDDLE);
    print_field(
        file, LEFT, "compression: \"%.*s\"", sizeof(header.compression), header.compression);
    print_border(file, MIDDLE);
    print_field(file, LEFT, "allocated_size: %" PRIu64, header.allocated_size);
    print_border(file, MIDDLE);
    print_field(file, LEFT, "used_size: %" PRIu64, header.used_size);
    print_border(file, MIDDLE);
    print_field(file, LEFT, "data_size: %" PRIu64, header.data_size);
    print_border(file, MIDDLE);

    char checksum[ASDF_BLOCK_CHECKSUM_FIELD_SIZE * 2 + 1] = {0};
    char *p = checksum;
    for (int idx = 0; idx < ASDF_BLOCK_CHECKSUM_FIELD_SIZE; idx++) {
        p += sprintf(p, "%02x", header.checksum[idx]);
    }
    print_field(file, LEFT, "checksum: %s", checksum);
    print_border(file, BOTTOM);
}


static const asdf_info_cfg_t asdf_info_default_cfg = {
    .filename = NULL, .print_tree = true, .print_blocks = false};


int asdf_info(FILE *in_file, FILE *out_file, const asdf_info_cfg_t *cfg) {

    if (!cfg)
        cfg = &asdf_info_default_cfg;

    // Current implementation needs YAML events unless no-tree
    asdf_parser_cfg_t parser_cfg = {
        .flags = cfg->print_tree ? ASDF_PARSER_OPT_EMIT_YAML_EVENTS : 0};

    asdf_parser_t *parser = asdf_parser_create(&parser_cfg);

    if (!parser)
        return 1;

    if (asdf_parser_set_input_fp(parser, in_file, cfg->filename) != 0) {
        asdf_parser_destroy(parser);
        return 1;
    }

    asdf_event_t *event = NULL;
    size_t block_count = 0;

    while ((event = asdf_event_iterate(parser))) {
        // Iterate events--if we hit a YAML event start building the tree (
        // build_tree takes over iteration from there) otherwise for block
        // events just show the block header details immediately, if the option
        // is enabled.
        asdf_event_type_t type = asdf_event_type(event);
        switch (type) {
        case ASDF_YAML_EVENT:
            if (cfg->print_tree) {
                tree_node_t *root = build_tree(parser);
                print_tree(out_file, root);
                tree_node_free(root);
            }
            break;
        case ASDF_BLOCK_EVENT:
            if (cfg->print_blocks)
                print_block(out_file, event, block_count);

            block_count++;
            break;
        default:
            break;
        }
    }

    int retval = 0;

    if (asdf_parser_has_error(parser)) {
        const char *error = asdf_parser_get_error(parser);
        // TODO: (#5) Better error formatting / probably go through logging system
        fprintf(stderr, "error: %s\n", error);
        retval = 1;
    }

    asdf_parser_destroy(parser);
    return retval;
}
