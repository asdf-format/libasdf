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
