#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "file.h"
#include "info.h"
#include "value.h"


#define ANSI_RESET "\033[0m"
#define ANSI_BOLD "\033[1m"
#define ANSI_DIM "\033[2m"
#define BOLD(str) ANSI_BOLD str ANSI_RESET
#define DIM(str) ANSI_DIM str ANSI_RESET

#define COLOR_GREEN "\033[32m"
#define COLOR_RED "\033[31m"
#define COLOR(color, str) color str ANSI_RESET

#define UTF8_LEADING_PREFIX 0xc0
#define UTF8_CONTINUATION_PREFIX 0x80

/** Determines how much indentation to reserve initially, but can be increased */
#define INITIAL_MAX_DEPTH 16


typedef struct {
    size_t depth;
    size_t max_depth;
    bool *active_levels;
    bool is_mapping;
    bool is_leaf;
} node_state_t;


typedef struct {
    const char *key;
    int index;
} node_index_t;


static void print_indent(FILE *out, node_state_t *state) {
    if (state->depth < 1)
        return;

    fputs(ANSI_DIM, out);

    for (size_t idx = 0; idx < state->depth; idx++) {
        if (idx == state->depth - 1) {
            fputs(state->is_leaf ? "└─" : "├─", out);
        } else {
            if (state->active_levels[idx])
                fputs("│ ", out);
            else
                fputs("  ", out);
        }
    }

    fputs(ANSI_RESET, out);
}


// TODO: Make this non-recursive using a stack of iterators / states
// Easy-enough conversion though unlikely to find many files deep enough to blow the C stack

// NOLINTNEXTLINE(misc-no-recursion)
static int print_node(FILE *out, asdf_value_t *node, node_index_t *index, node_state_t *state) {
    if (UNLIKELY(!out || !node || !index || !state))
        return -1;

    const char *tag = asdf_value_tag(node);

    if (!tag) {
        if (asdf_value_is_mapping(node)) {
            tag = "mapping";
        } else if (asdf_value_is_sequence(node)) {
            tag = "sequence";
        } else {
            tag = "scalar";
        }
    }

    print_indent(out, state);

    if (state->is_mapping) {
        fprintf(out, BOLD("%s") " (%s)", index->key, tag);
    } else {
        // Print node in sequence
        fprintf(out, DIM("[") BOLD("%d") DIM("]") " (%s)", index->index, tag);
    }

    if (!asdf_value_is_container(node)) {
        const char *value = NULL;
        asdf_value_as_scalar0(node, &value);
        fprintf(out, ": %s", value);
        fputc('\n', out);
        return 0;
    }

    fputc('\n', out);
    bool is_mapping = asdf_value_is_mapping(node);
    asdf_container_iter_t iter = asdf_container_iter_init();
    asdf_container_item_t *item = NULL;
    int size = asdf_container_size(node);
    int idx = 0;

    if (state->depth > state->max_depth - 1) {
        // Grow the maximum depth linerally
        size_t new_max_depth = state->max_depth + INITIAL_MAX_DEPTH;
        bool *new_active_levels = realloc(state->active_levels, new_max_depth * sizeof(bool));

        if (!new_active_levels)
            return -1;

        memcpy(new_active_levels, state->active_levels, state->max_depth);
        state->active_levels = new_active_levels;
        state->max_depth = new_max_depth;
    }

    state->active_levels[state->depth] = true;

    node_state_t child_state = {
        .depth = state->depth + 1,
        .max_depth = state->max_depth,
        .active_levels = state->active_levels,
        .is_mapping = is_mapping,
        .is_leaf = false};

    while ((item = asdf_container_iter(node, &iter))) {
        if (idx == size - 1) {
            child_state.is_leaf = true;
            state->active_levels[state->depth] = false;
        }

        node_index_t child_index = {0};

        if (is_mapping)
            child_index.key = asdf_container_item_key(item);
        else
            child_index.index = asdf_container_item_index(item);

        if (print_node(out, asdf_container_item_value(item), &child_index, &child_state) != 0) {
            asdf_container_item_destroy(item);
            return -1;
        }

        idx++;
    }

    return 0;
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


static void print_border(FILE *out, field_border_t border) {
    fprintf(out, ANSI_DIM);
    for (int idx = 0; idx < BOX_WIDTH; idx++) {
        switch (border) {
        case TOP:
            if (idx == 0)
                fprintf(out, "┌");
            else if (idx == BOX_WIDTH - 1)
                fprintf(out, "┐\n");
            else
                fprintf(out, "─");
            break;
        case MIDDLE:
            if (idx == 0)
                fprintf(out, "├");
            else if (idx == BOX_WIDTH - 1)
                fprintf(out, "┤\n");
            else
                fprintf(out, "─");
            break;
        case BOTTOM:
            if (idx == 0)
                fprintf(out, "└");
            else if (idx == BOX_WIDTH - 1)
                fprintf(out, "┘\n");
            else
                fprintf(out, "─");
            break;
        }
    }
    fprintf(out, ANSI_RESET);
}


/**
 * Calculate expected display width of a string, excluding ANSI escape sequences
 * and treating UTF-8 characters as one character width
 *
 * This may not be incorrect for non-UTF-8 terminals; if anyone runs into
 * trouble due to this we can add options to disable unicode or color, but
 * should be sane on most modern terminals
 */
static size_t visible_strlen(const char *str) {
    size_t len = 0;
    while (*str) {
        // Skip escape codes
        if (*str == '\x1b' && str[1] == '[') {
            str += 2;

            while (*str && *str != 'm')
                str++;

            if (*str == 'm')
                str++;

            continue;
        }

        // Skip UTF-8 continuation bytes
        if ((*str & UTF8_LEADING_PREFIX) != UTF8_CONTINUATION_PREFIX) {
            len++; // Count leading byte as one column
        }

        str++;
    }

    return len;
}


static void print_field(FILE *out, field_align_t align, const char *fmt, ...) {
    va_list args;
    // Reserve a little extra width for unicode, escape sequences, etc.
    char field_buf[BOX_WIDTH * 2] = {0};
    va_start(args, fmt);
    // NOLINTNEXTLINE(clang-analyzer-valist.Uninitialized)
    vsnprintf(field_buf, sizeof(field_buf), fmt, args);
    va_end(args);
    fprintf(out, DIM("│"));
    int field_len = (int)visible_strlen(field_buf);

    switch (align) {
    case LEFT: {
        int pad = BOX_WIDTH - field_len - 3;
        fprintf(out, " %s%*s", field_buf, pad, "");
        break;
    }
    case CENTER: {
        int left_pad = ((BOX_WIDTH - field_len) / 2) - 1;
        int right_pad = BOX_WIDTH - field_len - left_pad - 2;
        fprintf(out, "%*s%s%*s", left_pad, "", field_buf, right_pad, "");
        break;
    }
    }
    fprintf(out, DIM("│") "\n");
}


static int print_block(FILE *out, asdf_file_t *file, size_t block_idx, bool verify) {
    asdf_block_t *block = asdf_block_open(file, block_idx);

    if (!block)
        return -1;

    asdf_block_header_t *header = &block->info.header;

    // Print the block header with the block number
    print_border(out, TOP);
    print_field(out, CENTER, "Block #%zu", block_idx);
    print_border(out, MIDDLE);
    print_field(out, LEFT, "flags: 0x%08x", header->flags);
    print_border(out, MIDDLE);
    print_field(
        out, LEFT, "compression: \"%.*s\"", sizeof(header->compression), header->compression);
    print_border(out, MIDDLE);
    print_field(out, LEFT, "allocated_size: %" PRIu64, header->allocated_size);
    print_border(out, MIDDLE);
    print_field(out, LEFT, "used_size: %" PRIu64, header->used_size);
    print_border(out, MIDDLE);
    print_field(out, LEFT, "data_size: %" PRIu64, header->data_size);
    print_border(out, MIDDLE);

    char checksum[(ASDF_BLOCK_CHECKSUM_FIELD_SIZE * 2) + 1] = {0};
    char *p = checksum;
    for (int idx = 0; idx < ASDF_BLOCK_CHECKSUM_FIELD_SIZE; idx++) {
        p += sprintf(p, "%02x", header->checksum[idx]);
    }

    const char *verified = "";

    if (verify) {
        if (asdf_block_checksum_verify(block, NULL))
            verified = " " COLOR(COLOR_GREEN, "✓");
        else
            verified = " " COLOR(COLOR_RED, "✗");
    }

    print_field(out, LEFT, "checksum: %s%s", checksum, verified);
    print_border(out, BOTTOM);
    asdf_block_close(block);
    return 0;
}


static const asdf_info_cfg_t asdf_info_default_cfg = {
    .filename = NULL, .print_tree = true, .print_blocks = false, .verify_checksums = false};


int asdf_info(asdf_file_t *file, FILE *out, const asdf_info_cfg_t *cfg) {

    if (!cfg)
        cfg = &asdf_info_default_cfg;

    if (UNLIKELY(!file))
        return -1;

    int ret = 0;

    if (cfg->print_tree) {
        asdf_value_t *root = asdf_get_value(file, "/");
        // Yes, could use a bitfield too but that's overkill
        bool *active_levels = calloc(INITIAL_MAX_DEPTH, sizeof(bool));

        if (UNLIKELY(!active_levels))
            return -1;

        node_state_t state = {
            .depth = 0,
            .max_depth = INITIAL_MAX_DEPTH,
            .active_levels = active_levels,
            .is_mapping = true,
            .is_leaf = true};
        node_index_t index = {.key = "root"};
        ret = print_node(out, root, &index, &state);
        asdf_value_destroy(root);
        free(active_levels);
    }

    if (ret == 0 && cfg->print_blocks) {
        for (size_t idx = 0; idx < asdf_block_count(file); idx++) {
            ret = print_block(out, file, idx, cfg->verify_checksums);

            if (ret != 0)
                break;
        }
    }

    const char *error = asdf_error(file);

    if (error) {
        ret = -1;
        fprintf(stderr, "error: %s\n", error);
    }

    return ret;
}
