#ifndef ASDF_EMITTER_H
#define ASDF_EMITTER_H

#include <stdbool.h>
#include <stdint.h>

#include <asdf/util.h>
#include <asdf/yaml.h>


ASDF_BEGIN_DECLS

// NOLINTNEXTLINE(readability-identifier-naming)
#define _ASDF_EMITTER_OPTS(X) \
    X(ASDF_EMITTER_OPT_DEFAULT, 0) \
    X(ASDF_EMITTER_OPT_EMIT_EMPTY, 1) \
    X(ASDF_EMITTER_OPT_NO_BLOCK_INDEX, 2) \
    X(ASDF_EMITTER_OPT_EMIT_EMPTY_TREE, 3) \
    X(ASDF_EMITTER_OPT_NO_EMIT_EMPTY_TREE, 4)


typedef enum {
// clang-format off
#define X(flag, bit) flag = (1UL << (bit)),
    _ASDF_EMITTER_OPTS(X)
#undef X
    // clang-format on
} asdf_emitter_opt_t;


// NOLINTNEXTLINE(readability-magic-numbers)
ASDF_STATIC_ASSERT(
    ASDF_EMITTER_OPT_NO_EMIT_EMPTY_TREE < (1UL << 63), "too many flags for 64-bit int");


typedef uint64_t asdf_emitter_optflags_t;


/**
 * Low-level emitter configuration
 *
 * Currently just consists of a bitset of flags that are used internally by
 * the library; these flags are not currently documented.
 */
typedef struct asdf_emitter_cfg {
    asdf_emitter_optflags_t flags;
    asdf_yaml_tag_handle_t *tag_handles;
} asdf_emitter_cfg_t;

ASDF_END_DECLS

#endif /* ASDF_EMITTER_H */
