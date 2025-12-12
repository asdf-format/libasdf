/**
 * Represents the block index
 *
 * The block index is just a vector of isize, with negative values indicating
 * an invalid index entry.
 */
#pragma once

#define i_type asdf_block_index, off_t
#include <stc/vec.h>

typedef asdf_block_index asdf_block_index_t;
typedef asdf_block_index_iter asdf_block_index_iter_t;
