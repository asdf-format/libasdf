#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "../util.h"

#include "ndarray.h"


typedef int (*asdf_ndarray_convert_fn_t)(
    void *restrict dst, const void *restrict src, size_t bytes, size_t elsize);


ASDF_LOCAL asdf_ndarray_convert_fn_t asdf_ndarray_get_convert_fn(
    asdf_scalar_datatype_t src_t, asdf_scalar_datatype_t dst_t, bool byteswap);
