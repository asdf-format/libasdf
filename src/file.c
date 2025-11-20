#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <libfyaml.h>

#include "compression.h"
#include "context.h"
#include "error.h"
#include "event.h"
#include "file.h"
#include "log.h"
#include "parse.h"
#include "util.h"
#include "value.h"


static const asdf_config_t default_asdf_config = {
    /* Basic parser settings for high-level file interface: ignore individual YAML events and
     * just store the tree in memory to parse into a fy_document later */
    .parser = {.flags = ASDF_PARSER_OPT_BUFFER_TREE}};


/**
 * Override the default config value (which should always be some form of 0) if the
 * user-provided value is non-zero.
 *
 * This might not be sustainable if any config options ever have 0 as a valid value
 * that the user might want to override though, in which case we'll have to probably
 * change the configuration API, but this is OK for now.
 */
#define ASDF_CONFIG_OVERRIDE(config, user_config, option, default) \
    do { \
        if (user_config->option != default) \
            config->option = user_config->option; \
    } while (0)


static asdf_config_t *asdf_config_build(asdf_config_t *user_config) {
    asdf_config_t *config = malloc(sizeof(asdf_config_t));

    if (!config) {
        // TODO: Should have more convenient macros for setting/logging global errors
        asdf_global_context_t *ctx = asdf_global_context_get();
        ASDF_LOG(ctx, ASDF_LOG_FATAL, "failed to allocate memory for libasdf config");
        asdf_context_error_set_oom(ctx->base.ctx);
        return NULL;
    }

    memcpy(config, &default_asdf_config, sizeof(asdf_config_t));

    if (user_config) {
        ASDF_CONFIG_OVERRIDE(config, user_config, parser.flags, 0);
        ASDF_CONFIG_OVERRIDE(config, user_config, decomp.mode, ASDF_BLOCK_DECOMP_MODE_AUTO);
        ASDF_CONFIG_OVERRIDE(config, user_config, decomp.max_memory_bytes, 0);
        ASDF_CONFIG_OVERRIDE(config, user_config, decomp.max_memory_threshold, 0.0);
        ASDF_CONFIG_OVERRIDE(config, user_config, decomp.chunk_size, 0);
        ASDF_CONFIG_OVERRIDE(config, user_config, decomp.tmp_dir, NULL);
    }

    return config;
}


static void asdf_config_validate(asdf_file_t *file) {
    double max_memory_threshold = file->config->decomp.max_memory_threshold;
    if (max_memory_threshold < 0.0 || max_memory_threshold > 1.0 || isnan(max_memory_threshold)) {
        ASDF_LOG(
            file,
            ASDF_LOG_WARN,
            "invalid config value for decomp.max_memory_threshold; the setting will be disabled "
            "(expected >=0.0 and <= 1.0, got %g)",
            max_memory_threshold);
        file->config->decomp.max_memory_threshold = 0.0;
    }
#ifndef HAVE_STATGRAB
    if (max_memory_threshold > 0.0) {
        ASDF_LOG(
            file,
            ASDF_LOG_WARN,
            "decomp.max_memory_threshold set to %g, but libasdf was compiled without libstatgrab "
            "support to detect available memory; the setting will be disabled",
            max_memory_threshold);
        file->config->decomp.max_memory_threshold = 0.0;
    }
#endif
#ifdef ASDF_BLOCK_DECOMP_LAZY_AVAILABLE
    asdf_block_decomp_mode_t mode = file->config->decomp.mode;
    switch (mode) {
    case ASDF_BLOCK_DECOMP_MODE_AUTO:
        // Lazy mode not a available, just set to eager
        file->config->decomp.mode = ASDF_BLOCK_DECOMP_MODE_EAGER;
        break;
    case ASDF_BLOCK_DECOMP_MODE_EAGER:
        break;
    case ASDF_BLOCK_DECOMP_MODE_LAZY:
        ASDF_LOG(
            file,
            ASDF_LOG_WARN,
            "decomp.mode is set to lazy but this is only available currently on Linux; eager "
            "decompression will be used instead");
        file->config->decomp.mode = ASDF_BLOCK_DECOMP_MODE_EAGER;
    }
#endif
}


/* Internal helper to allocate and set up a new asdf_file_t */
static asdf_file_t *asdf_file_create(asdf_config_t *user_config) {
    /* Try to allocate asdf_file_t object, returns NULL on memory allocation failure*/
    asdf_file_t *file = calloc(1, sizeof(asdf_file_t));
    asdf_config_t *config = asdf_config_build(user_config);

    if (UNLIKELY(!file))
        return NULL;

    if (UNLIKELY(!config))
        return NULL;

    asdf_parser_t *parser = asdf_parser_create(&config->parser);

    if (!parser)
        return NULL;

    file->base.ctx = parser->base.ctx;
    asdf_context_retain(file->base.ctx);
    file->parser = parser;
    file->config = config;
    asdf_config_validate(file);
    /* Now we can start cooking */
    return file;
}


asdf_file_t *asdf_open_file_ex(const char *filename, const char *mode, asdf_config_t *config) {
    asdf_file_t *file = asdf_file_create(config);

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


asdf_file_t *asdf_open_fp_ex(FILE *fp, const char *filename, asdf_config_t *config) {
    asdf_file_t *file = asdf_file_create(config);

    if (!file)
        return NULL;

    asdf_parser_set_input_fp(file->parser, fp, filename);
    return file;
}


asdf_file_t *asdf_open_mem_ex(const void *buf, size_t size, asdf_config_t *config) {
    asdf_file_t *file = asdf_file_create(config);

    if (!file)
        return NULL;

    asdf_parser_set_input_mem(file->parser, buf, size);
    return file;
}


void asdf_close(asdf_file_t *file) {
    if (!file)
        return;

    asdf_context_release(file->base.ctx);
    fy_document_destroy(file->tree);
    asdf_parser_destroy(file->parser);
    free(file->config);
    /* Clean up */
    ZERO_MEMORY(file, sizeof(asdf_file_t));
    free(file);
}


const char *asdf_error(asdf_file_t *file) {
    asdf_base_t *base = (asdf_base_t *)file;

    if (!base) {
        // Return errors from the global context
        base = (asdf_base_t *)asdf_global_context_get();

        if (!base) {
            asdf_log_fallback(
                ASDF_LOG_FATAL,
                __FILE__,
                __LINE__,
                "libasdf global context not initialized; the library is in an "
                "undefined state");
            return NULL;
        }
    }
    return ASDF_ERROR_GET(base);
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

    if (value->raw_type != ASDF_VALUE_MAPPING) {
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

    if (value->raw_type != ASDF_VALUE_SEQUENCE) {
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


__ASDF_GET_TYPE(bool);
__ASDF_GET_INT_TYPE(int8);
__ASDF_GET_INT_TYPE(int16);
__ASDF_GET_INT_TYPE(int32);
__ASDF_GET_INT_TYPE(int64);
__ASDF_GET_INT_TYPE(uint8);
__ASDF_GET_INT_TYPE(uint16);
__ASDF_GET_INT_TYPE(uint32);
__ASDF_GET_INT_TYPE(uint64);
__ASDF_GET_TYPE(float);
__ASDF_GET_TYPE(double);


bool asdf_is_extension_type(asdf_file_t *file, const char *path, asdf_extension_t *ext) {
    asdf_value_t *value = asdf_get_value(file, path);
    if (!value)
        return false;

    bool ret = asdf_value_is_extension_type(value, ext);
    asdf_value_destroy(value);
    return ret;
}


asdf_value_err_t asdf_get_extension_type(
    asdf_file_t *file, const char *path, asdf_extension_t *ext, void **out) {
    asdf_value_t *value = asdf_get_value(file, path);
    if (!value)
        return ASDF_VALUE_ERR_NOT_FOUND;
    asdf_value_err_t err = asdf_value_as_extension_type(value, ext, out);
    asdf_value_destroy(value);
    return err;
}


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
    block->comp = asdf_block_comp_parse(file, info->header.compression);
    block->comp_state = NULL;
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

    if (block->comp_state)
        asdf_block_comp_close(block);

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
            *size = block->avail_size;

        return block->data;
    }

    asdf_parser_t *parser = block->file->parser;
    asdf_stream_t *stream = parser->stream;
    size_t avail = 0;
    void *data = stream->open_mem(
        stream, block->info.data_pos, block->info.header.used_size, &avail);
    block->data = data;
    block->avail_size = avail;

    // Open compressed data if applicable
    if (asdf_block_comp_open(block) != 0) {
        ASDF_LOG(block->file, ASDF_LOG_ERROR, "failed to open compressed block data");
        return NULL;
    }

    if (block->comp_state) {
        // Return the destination of the compressed data
        if (size)
            *size = block->comp_state->dest_size;

        return block->comp_state->dest;
    }

    if (size)
        *size = avail;

    // Just the raw data
    return block->data;
}


asdf_block_comp_t asdf_block_compression(asdf_block_t *block) {
    if (!block)
        return ASDF_BLOCK_COMP_UNKNOWN;

    return block->comp;
}
