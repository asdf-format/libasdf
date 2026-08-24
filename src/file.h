#pragma once

#include <stdbool.h>

#include <libfyaml.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "asdf/file.h" // IWYU pragma: export

#include "context.h"
#include "core/history_entry.h"
#include "emitter.h"
#include "parser.h"
#include "types/asdf_block_info_vec.h"
#include "types/asdf_str_map.h"


#ifdef HAVE_USERFAULTFD
/**
 * A macro which can be used at compile-time to check if lazy-mode
 * decompression is available.
 *
 * Currently only works when built on new-enough Linux versions that have
 * userfaultfd support, though can provide other implementations later.
 */
#define ASDF_BLOCK_DECOMP_LAZY_AVAILABLE HAVE_USERFAULTFD
#endif


/**
 * Open modes for files
 */
typedef enum {
    ASDF_FILE_MODE_INVALID = -1,
    ASDF_FILE_MODE_READ_ONLY,
    ASDF_FILE_MODE_WRITE_ONLY,
    ASDF_FILE_MODE_READ_WRITE
} asdf_file_mode_t;


#define ASDF_FILE_TAG_MAP_DEFAULT_SIZE 20


typedef struct asdf_write_cleanup {
    void (*callback)(void *);
    void *data;
    struct asdf_write_cleanup *next;
} asdf_write_cleanup_t;


typedef struct asdf_file {
    asdf_base_t base;
    asdf_config_t *config;
    asdf_file_mode_t mode;
    asdf_stream_t *stream;
    asdf_parser_t *parser;
    asdf_emitter_t *emitter;
    struct fy_document *tree;
    asdf_block_info_vec_t blocks;
    /**
     * Map of full canonical tag names to shortned ("normalized") tags using
     * document's defined tag handles
     *
     * This serves two purposes:
     *
     * - Because of how libfyaml handles tags, all tags attached to nodes
     *   have to be kept alive somewhere in memory.  When we shorten full
     *   tags to their normalized form these are allocated on the heap, so
     *   we have to track them sometime for the lifetime of the file so that
     *   any new tagged nodes created can continue to reference those tags.
     *
     * - As a side benefit we cache the normalized tags and don't have to
     *   rebuild them.  This has little benefit for tags that are only used
     *   once in the file, but may have some benefit for frequently used tags.
     */
    asdf_str_map_t tag_map;
    /**
     * Optional override of the asdf_library software to set in the file
     * metadata on output
     */
    asdf_software_t *asdf_library;
    /**
     * Optional array of user-added history entries to append to the file's
     * metadata on output
     */
    asdf_history_entry_t **history_entries;
    /** Linked list of cleanup callbacks to run after each write */
    asdf_write_cleanup_t *write_cleanups;
} asdf_file_t;


/** Internal helper to get the `struct fy_document` for the tree, if any */
ASDF_LOCAL struct fy_document *asdf_file_tree_document(asdf_file_t *file);

/** Internal helper to lazily create/return the file's parser (NULL if none) */
ASDF_LOCAL asdf_parser_t *asdf_file_parser(asdf_file_t *file);

/** Internal helper to register a cleanup callback to run after each write */
ASDF_LOCAL void asdf_file_write_cleanup_add(
    asdf_file_t *file, void (*callback)(void *), void *data);

/** Internal helper to run and free all registered write cleanup callbacks */
ASDF_LOCAL void asdf_file_run_write_cleanups(asdf_file_t *file);

/** Internal helper to set and/or retrieve a normalized tag */
ASDF_LOCAL const char *asdf_file_tag_normalize(asdf_file_t *file, const char *tag);

// Forward-declarations
typedef struct asdf_block_comp_state asdf_block_comp_state_t;

/**
 * User-level object for inspecting ASDF block metadata and data
 */
typedef struct asdf_block {
    asdf_file_t *file;
    asdf_block_info_t info;
    void *data;
    bool should_close;
    // Should be the same as used_size in the header but may be truncated in exceptional
    // cases (we should probably log a warning when it is)
    size_t avail_size;

    const char *compression;
    asdf_block_comp_state_t *comp_state;
    /**
     * True for a handle created by `asdf_block_create` that has not yet been
     * appended to a file: such a handle owns its `info` (including `info.data`
     * when `info.data_owned`) and is torn down by `asdf_block_destroy`.  A
     * handle from `asdf_block_open` (a view onto a block the file owns) or one
     * that has been appended has this cleared and is torn down by
     * `asdf_block_close`.
     */
    bool detached;
} asdf_block_t;


/** Internal block methods */
ASDF_LOCAL const char *asdf_block_compression_orig(asdf_block_t *block);
