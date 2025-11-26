/**
 * STC hash map mapping block compression types to associated ASDF compressor extension
 */
#pragma once

#include <stc/cstr.h>

#include "compression.h"

#define i_type asdf_compressor_map
#define i_keypro cstr
#define i_val asdf_compressor_t *
#include <stc/hmap.h>

typedef asdf_compressor_map asdf_compressor_map_t;
