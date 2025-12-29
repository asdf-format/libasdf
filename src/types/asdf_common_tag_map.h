/**
 * STC hash map mapping YAML Common Schema tags to their associated type enums
 */
#pragma once

#include "stc/cstr.h"

#include "../value_util.h"

#define i_type asdf_common_tag_map
#define i_keypro cstr
#define i_val asdf_yaml_common_tag_t
#include "stc/hmap.h"

typedef asdf_common_tag_map asdf_common_tag_map_t;
