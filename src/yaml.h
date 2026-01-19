/**
 * Thin wrappers around libfyaml
 *
 * Idea is to allow users to stick to asdf_ APIs rather than having to learn both
 * and also expose only the bits of libfyaml most users will actually need.
 * Other idea is to enable the possibility for using other YAML parsers.
 */

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdlib.h>

#include <libfyaml.h>

#include "asdf/file.h"
#include "asdf/yaml.h" // IWYU pragma: export
#include "util.h"


#define ASDF_YAML_DEFAULT_TAG_HANDLE "!"

#define ASDF_YAML_DIRECTIVE_PREFIX "%YAML "
#define ASDF_YAML_DIRECTIVE ASDF_YAML_DIRECTIVE_PREFIX "1.1"
#define ASDF_YAML_DOCUMENT_BEGIN_MARKER "\n---"
#define ASDF_YAML_DOCUMENT_END_MARKER "\n..."


/**
 * The "%YAML 1.1\n" directive specifically expected for valid ASDF
 */
extern ASDF_LOCAL const char *asdf_yaml_directive;
#define ASDF_YAML_DIRECTIVE_SIZE 9


/**
 * The string "%YAML " indicating start of an arbitrary YAML directive
 */
extern ASDF_LOCAL const char *asdf_yaml_directive_prefix;
#define ASDF_YAML_DIRECTIVE_PREFIX_SIZE 6


/**
 * Token for a valid YAML document end marker, i.e. '\n...'
 * It should also end with a newline but that's excluded from this constant
 * since it could be either \r\n or just \n so we check for that explicitly once
 * this token is found
 */
extern ASDF_LOCAL const char *asdf_yaml_document_end_marker;
#define ASDF_YAML_DOCUMENT_END_MARKER_SIZE 4


extern ASDF_LOCAL const char *asdf_yaml_tag_prefix;
#define ASDF_YAML_TAG_PREFIX_SIZE 4


/**
 * A hard-coded string containg an empty YAML 1.1 document
 *
 * This is used with libfyaml to initialize an empty document.  This is a
 * workaround to a shortcoming that libfyaml does not allow setting a
 * YAML version on a document unless it's preserving the original version
 * of an existing document (otherwise it always outputs the latest version).
 *
 * That would be fine except for the fact that ASDF 1.x standardizes on
 * YAML 1.1 specifically
 */
extern ASDF_LOCAL const char *asdf_yaml_empty_document;


ASDF_LOCAL struct fy_document *asdf_yaml_create_empty_document(asdf_config_t *config);


/**
 * Utilities for parsing and iterating over YAML Pointer paths
 */
typedef enum {
    /**
     * The target can be a mapping or a sequence depending on context
     *
     * This is the one 'ambiguous' case when we have a positive integer which
     * could be a mapping key or a sequence index.  If the parent value does
     * not already exist then it is assumed to be a sequence index, but if the
     * parent does exist and is a mapping, then it can be taken as a mapping
     * key.
     */
    ASDF_YAML_PC_TARGET_ANY,
    /** A key in a mapping */
    ASDF_YAML_PC_TARGET_MAP,
    /** An index in a sequence */
    ASDF_YAML_PC_TARGET_SEQ,
} asdf_yaml_pc_target_t;


typedef struct {
    asdf_yaml_pc_target_t target;
    const char *key;
    ssize_t index;
} asdf_yaml_path_component;


typedef asdf_yaml_path_component asdf_yaml_path_component_t;


/* LCOV_EXCL_START */
static asdf_yaml_path_component_t asdf_yaml_path_component_clone(
    asdf_yaml_path_component_t component) {
    if (component.key)
        component.key = strdup(component.key);

    return component;
}
/* LCOV_EXCL_STOP */


static void asdf_yaml_path_component_drop(asdf_yaml_path_component_t *component) {
    free((char *)component->key);
}


// NOLINTNEXTLINE(readability-identifier-naming)
#define i_type asdf_yaml_path
#define i_keyclass asdf_yaml_path_component
#include "stc/vec.h"
typedef asdf_yaml_path asdf_yaml_path_t;
typedef asdf_yaml_path_iter asdf_yaml_path_iter_t;


ASDF_LOCAL char *asdf_yaml_tag_canonicalize(const char *tag);
ASDF_LOCAL char *asdf_yaml_tag_normalize(const char *tag, const asdf_yaml_tag_handle_t *handles);
ASDF_LOCAL bool asdf_yaml_path_parse(const char *path, asdf_yaml_path_t *out_path);
