/**
 * Hashmap from transform tag names to `asdf_gwcs_transform_type_t`
 */

#pragma once

#include <stc/cstr.h>

#include <asdf/gwcs/transform.h>

#define i_type asdf_gwcs_transform_map
#define i_keypro cstr
#define i_val asdf_gwcs_transform_type_t
#include <stc/hmap.h>

typedef asdf_gwcs_transform_map asdf_gwcs_transform_map_t;
