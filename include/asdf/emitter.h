#ifndef ASDF_EMITTER_H
#define ASDF_EMITTER_H

#include <stdbool.h>
#include <stdint.h>

#include <asdf/util.h>
#include <asdf/yaml.h>


ASDF_BEGIN_DECLS

// NOLINTNEXTLINE(readability-identifier-naming)
#define ASDF__EMITTER_OPTS(X) \
    X(ASDF_EMITTER_OPT_DEFAULT, 0) \
    X(ASDF_EMITTER_OPT_EMIT_EMPTY, 1) \
    X(ASDF_EMITTER_OPT_NO_BLOCK_CHECKSUM, 2) \
    X(ASDF_EMITTER_OPT_NO_BLOCK_INDEX, 3) \
    X(ASDF_EMITTER_OPT_EMIT_EMPTY_TREE, 4) \
    X(ASDF_EMITTER_OPT_NO_EMIT_EMPTY_TREE, 5) \
    X(ASDF_EMITTER_OPT_NO_EMIT_ASDF_LIBRARY, 6) \
    X(ASDF_EMITTER_OPT_LAST, 62) // Always keep this option reserved as last


typedef enum {
// clang-format off
#define X(flag, bit) flag = (1ULL << (bit)),
    ASDF__EMITTER_OPTS(X)
#undef X
    // clang-format on
} asdf_emitter_opt_t;


// NOLINTNEXTLINE(readability-magic-numbers)
ASDF_STATIC_ASSERT(ASDF_EMITTER_OPT_LAST < (1ULL << 63), "too many flags for 64-bit int");


typedef uint64_t asdf_emitter_optflags_t;


/**
 * Controls where an ndarray's data is stored when written.
 *
 * Used both as a per-array setting (see `asdf_ndarray_storage_set`) and as a
 * file-level override in `asdf_emitter_cfg_t`.
 */
typedef enum {
    /** Use the file-level ``array_storage`` emitter setting (default). */
    ASDF_ARRAY_STORAGE_DEFAULT = 0,
    /** Store the array as inline YAML data under the ``data`` key. */
    ASDF_ARRAY_STORAGE_INLINE = 1,
    /** Store the array in an internal binary block. */
    ASDF_ARRAY_STORAGE_INTERNAL = 2,
    /** Store the array in an external file (reserved; not yet supported). */
    ASDF_ARRAY_STORAGE_EXTERNAL = 3,
} asdf_array_storage_t;


/**
 * Low-level emitter configuration
 *
 * Currently just consists of a bitset of flags that are used internally by
 * the library; these flags are not currently documented.
 */
typedef struct asdf_emitter_cfg {
    asdf_emitter_optflags_t flags;
    asdf_yaml_tag_handle_t *tag_handles;
    /**
     * Number of elements above which a warning is logged when writing an
     * ndarray with inline data.  Set to 0 to use the library default (1024).
     * Set to SIZE_MAX to suppress the warning entirely.
     */
    size_t inline_ndarray_warning_thresh;
    /**
     * Override where all ndarray data is written; see `asdf_array_storage_t`.
     * Defaults to ``ASDF_ARRAY_STORAGE_DEFAULT`` (0), which respects the
     * per-array storage mode set via `asdf_ndarray_storage_set`.
     */
    asdf_array_storage_t array_storage;
} asdf_emitter_cfg_t;

ASDF_END_DECLS

#endif /* ASDF_EMITTER_H */
