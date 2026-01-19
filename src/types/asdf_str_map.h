/**
 * Hashmap to use for any string -> string mappings
 */

#pragma once

#include <stc/cstr.h>

#define i_type asdf_str_map
#define i_keypro cstr
#define i_valpro cstr
#include <stc/hmap.h>

typedef asdf_str_map asdf_str_map_t;
typedef asdf_str_map_iter asdf_str_map_iter_t;
typedef asdf_str_map_result asdf_str_map_res_t;
