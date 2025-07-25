#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libfyaml.h>

#include "context.h"
#include "error.h"
#include "event.h"
#include "file.h"
#include "log.h"
#include "parse.h"
#include "util.h"
#include "value.h"


/* Internal helper to allocate and set up a new asdf_file_t */
static asdf_file_t *asdf_file_create() {
    /* Try to allocate asdf_file_t object, returns NULL on memory allocation failure*/
    asdf_file_t *file = calloc(1, sizeof(asdf_file_t));

    if (UNLIKELY(!file))
        return NULL;

    /* Basic parser settings for high-level file interface: ignore individual YAML events and
     * just store the tree in memory to parse into a fy_document later */
    asdf_parser_cfg_t parser_cfg = {.flags = ASDF_PARSER_OPT_BUFFER_TREE};
    asdf_parser_t *parser = asdf_parser_create(&parser_cfg);

    if (!parser)
        return NULL;

    file->base.ctx = parser->base.ctx;
    asdf_context_retain(file->base.ctx);
    file->parser = parser;
    /* Now we can start cooking */
    return file;
}


asdf_file_t *asdf_open_file(const char *filename, const char *mode) {
    asdf_file_t *file = asdf_file_create();

    if (!file)
        return NULL;

    /* Currently only the mode string "r" is supported */
    if ((0 != strcasecmp(mode, "r"))) {
        ASDF_ERROR(file, "invalid asdf file mode: %s", mode);
        return file;
    }

    asdf_parser_set_input_file(file->parser, filename);
    return file;
}


asdf_file_t *asdf_open_fp(FILE *fp, const char *filename) {
    asdf_file_t *file = asdf_file_create();

    if (!file)
        return NULL;

    asdf_parser_set_input_fp(file->parser, fp, filename);
    return file;
}


asdf_file_t *asdf_open_mem(const void *buf, size_t size) {
    asdf_file_t *file = asdf_file_create();

    if (!file)
        return NULL;

    asdf_parser_set_input_mem(file->parser, buf, size);
    return file;
}


void asdf_close(asdf_file_t *file) {
    if (!file)
        return;

    asdf_context_release(file->base.ctx);
    asdf_parser_destroy(file->parser);
    fy_document_destroy(file->tree);
    /* Clean up */
    ZERO_MEMORY(file, sizeof(asdf_file_t));
    free(file);
}


ASDF_LOCAL struct fy_document *asdf_file_get_tree_document(asdf_file_t *file) {
    if (!file)
        return NULL;

    if (file->tree)
        /* Already exists and ready to go */
        return file->tree;

    asdf_parser_t *parser = file->parser;

    if (!parser)
        return NULL;

    if (UNLIKELY(0 == parser->tree.has_tree))
        return NULL;

    asdf_event_t *event = NULL;

    if (parser->tree.has_tree < 0) {
        /* We have to run the parser until the tree is found or we hit a block or eof (no tree) */
        while ((event = asdf_event_iterate(parser))) {
            asdf_event_type_t event_type = asdf_event_type(event);
            switch (event_type) {
            case ASDF_TREE_END_EVENT:
                goto has_tree;
            case ASDF_BLOCK_EVENT:
            case ASDF_END_EVENT:
                asdf_event_free(parser, event);
                return NULL;
            default:
                break;
            }
        }

        return NULL;
    }
has_tree:
    asdf_event_free(parser, event);

    if (parser->tree.has_tree < 1 || parser->tree.buf == NULL) {
        ASDF_LOG(
            file,
            ASDF_LOG_WARN,
            "logic error: there should be a YAML tree in the file at "
            "this point but it was not found (tree.has_tree = %d; tree.buf = 0x%zu)",
            parser->tree.has_tree,
            parser->tree.buf);
        return NULL;
    }

    size_t size = parser->tree.size;
    const char *buf = (const char *)parser->tree.buf;
    file->tree = fy_document_build_from_string(NULL, buf, size);
    return file->tree;
}


asdf_value_t *asdf_get_value(asdf_file_t *file, const char *path) {
    struct fy_document *tree = asdf_file_get_tree_document(file);

    if (UNLIKELY(!tree))
        return NULL;

    struct fy_node *root = fy_document_root(tree);

    if (UNLIKELY(!root))
        return NULL;

    struct fy_node *node = fy_node_by_path(root, path, -1, FYNWF_PTR_DEFAULT);

    if (!node)
        return NULL;

    asdf_value_t *value = asdf_value_create(file, node);

    if (UNLIKELY(!value)) {
        ASDF_ERROR_OOM(file);
        return NULL;
    }

    return value;
}


/* asdf_is_(type), asdf_get_(type) shortcuts */
#define __ASDF_IS_TYPE(type) \
    bool asdf_is_##type(asdf_file_t *file, const char *path) { \
        asdf_value_t *value = asdf_get_value(file, path); \
        if (!value) \
            return false; \
        bool ret = asdf_value_is_##type(value); \
        asdf_value_destroy(value); \
        return ret; \
    }


__ASDF_IS_TYPE(mapping)
__ASDF_IS_TYPE(sequence)
__ASDF_IS_TYPE(string)
__ASDF_IS_TYPE(scalar)
__ASDF_IS_TYPE(bool)
__ASDF_IS_TYPE(null)
__ASDF_IS_TYPE(int)
__ASDF_IS_TYPE(int8)
__ASDF_IS_TYPE(int16)
__ASDF_IS_TYPE(int32)
__ASDF_IS_TYPE(int64)
__ASDF_IS_TYPE(uint8)
__ASDF_IS_TYPE(uint16)
__ASDF_IS_TYPE(uint32)
__ASDF_IS_TYPE(uint64)
__ASDF_IS_TYPE(float)
__ASDF_IS_TYPE(double)


asdf_value_err_t asdf_get_mapping(asdf_file_t *file, const char *path, asdf_value_t **out) {
    asdf_value_t *value = asdf_get_value(file, path);

    if (!value)
        return ASDF_VALUE_ERR_NOT_FOUND;

    if (value->type != ASDF_VALUE_MAPPING) {
        asdf_value_destroy(value);
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }

    *out = value;
    return ASDF_VALUE_OK;
}


asdf_value_err_t asdf_get_sequence(asdf_file_t *file, const char *path, asdf_value_t **out) {
    asdf_value_t *value = asdf_get_value(file, path);

    if (!value)
        return ASDF_VALUE_ERR_NOT_FOUND;

    if (value->type != ASDF_VALUE_SEQUENCE) {
        asdf_value_destroy(value);
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    }

    *out = value;
    return ASDF_VALUE_OK;
}


asdf_value_err_t asdf_get_string(
    asdf_file_t *file, const char *path, const char **out, size_t *out_len) {
    asdf_value_t *value = asdf_get_value(file, path);

    if (!value)
        return ASDF_VALUE_ERR_NOT_FOUND;

    asdf_value_err_t err = asdf_value_as_string(value, out, out_len);
    asdf_value_destroy(value);
    return err;
}


asdf_value_err_t asdf_get_string0(asdf_file_t *file, const char *path, const char **out) {
    asdf_value_t *value = asdf_get_value(file, path);

    if (!value)
        return ASDF_VALUE_ERR_NOT_FOUND;

    asdf_value_err_t err = asdf_value_as_string0(value, out);
    asdf_value_destroy(value);
    return err;
}


asdf_value_err_t asdf_get_scalar(
    asdf_file_t *file, const char *path, const char **out, size_t *out_len) {
    asdf_value_t *value = asdf_get_value(file, path);

    if (!value)
        return ASDF_VALUE_ERR_NOT_FOUND;

    asdf_value_err_t err = asdf_value_as_scalar(value, out, out_len);
    asdf_value_destroy(value);
    return err;
}


asdf_value_err_t asdf_get_scalar0(asdf_file_t *file, const char *path, const char **out) {
    asdf_value_t *value = asdf_get_value(file, path);

    if (!value)
        return ASDF_VALUE_ERR_NOT_FOUND;

    asdf_value_err_t err = asdf_value_as_scalar0(value, out);
    asdf_value_destroy(value);
    return err;
}


#define __ASDF_GET_TYPE(type) \
    asdf_value_err_t asdf_get_##type(asdf_file_t *file, const char *path, type *out) { \
        asdf_value_t *value = asdf_get_value(file, path); \
        if (!value) \
            return ASDF_VALUE_ERR_NOT_FOUND; \
        asdf_value_err_t err = asdf_value_as_##type(value, out); \
        asdf_value_destroy(value); \
        return err; \
    }


#define __ASDF_GET_INT_TYPE(type) \
    asdf_value_err_t asdf_get_##type(asdf_file_t *file, const char *path, type##_t *out) { \
        asdf_value_t *value = asdf_get_value(file, path); \
        if (!value) \
            return ASDF_VALUE_ERR_NOT_FOUND; \
        asdf_value_err_t err = asdf_value_as_##type(value, out); \
        asdf_value_destroy(value); \
        return err; \
    }


__ASDF_GET_TYPE(bool)
__ASDF_GET_INT_TYPE(int8)
__ASDF_GET_INT_TYPE(int16)
__ASDF_GET_INT_TYPE(int32)
__ASDF_GET_INT_TYPE(int64)
__ASDF_GET_INT_TYPE(uint8)
__ASDF_GET_INT_TYPE(uint16)
__ASDF_GET_INT_TYPE(uint32)
__ASDF_GET_INT_TYPE(uint64)
__ASDF_GET_TYPE(float)
__ASDF_GET_TYPE(double)


/* User-facing block-related methods */
size_t asdf_block_count(asdf_file_t *file) {
    if (!file)
        return 0;

    /* Because blocks are the last things we expect to find in a file (modulo the optional block
     * index) we cannot return the block count accurately without parsing the full file.  Relying
     * on the block index alone for the count is also not guaranteed to be accurate since it is
     * only a hint (a hint that nonetheless allows the parser to complete much faster when
     * possible).  So here we ensure the file is parsed to completion then return the block count.
     */
    asdf_parser_t *parser = file->parser;

    while (!parser->done) {
        asdf_event_iterate(parser);
    }

    return parser->blocks.n_blocks;
}

asdf_block_t *asdf_block_open(asdf_file_t *file, size_t index) {
    if (!file)
        return NULL;

    size_t n_blocks = asdf_block_count(file);

    if (index >= n_blocks) {
        ASDF_LOG(
            file,
            ASDF_LOG_WARN,
            "block index %zu does not exist (the file contains %zu blocks)",
            index,
            n_blocks);
        return NULL;
    }

    asdf_block_t *block = calloc(1, sizeof(asdf_block_t));

    if (!block) {
        ASDF_ERROR_OOM(file);
        return NULL;
    }

    asdf_parser_t *parser = file->parser;
    asdf_block_info_t *info = parser->blocks.block_infos[index];
    block->file = file;
    block->info = *info;
    return block;
}


void asdf_block_close(asdf_block_t *block) {
    if (!block)
        return;

    // If the block has an open data handle, close it
    if (block->data) {
        asdf_stream_t *stream = block->file->parser->stream;
        stream->close_mem(stream, block->data);
    }

    ZERO_MEMORY(block, sizeof(asdf_block_t));
    free(block);
}


size_t asdf_block_data_size(asdf_block_t *block) {
    return block->info.header.data_size;
}


void *asdf_block_data(asdf_block_t *block, size_t *size) {
    if (!block)
        return NULL;

    if (block->data) {
        if (size)
            *size = block->data_size;

        return block->data;
    }

    asdf_parser_t *parser = block->file->parser;
    asdf_stream_t *stream = parser->stream;
    size_t avail = 0;
    void *data =
        stream->open_mem(stream, block->info.data_pos, block->info.header.used_size, &avail);
    block->data = data;
    block->data_size = avail;

    if (size)
        *size = avail;

    return data;
}
