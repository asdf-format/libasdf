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


#define ASDF_YAML_DEFAULT_TAG_HANDLE "!"


#include "asdf/yaml.h" // IWYU pragma: export
