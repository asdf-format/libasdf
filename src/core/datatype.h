#pragma once

#define ASDF_CORE_NDARRAY_INTERNAL
#include "asdf/core/datatype.h" // IWYU pragma: export

#include "../util.h"
#include "../value.h"


/**
 * Internal structure for representing a shape and number of dimensions
 *
 * Returned by `asdf_datatype_parse_shape`
 */
typedef struct {
    uint32_t ndim;
    uint64_t *shape;
} asdf_datatype_shape_t;


/** datatype internal API shared with the ndarray implementation */
ASDF_LOCAL asdf_value_err_t
asdf_datatype_parse(asdf_value_t *value, asdf_byteorder_t byteorder, asdf_datatype_t *datatype);

ASDF_LOCAL void asdf_datatype_clean(asdf_datatype_t *datatype);

ASDF_LOCAL asdf_value_err_t
asdf_datatype_byteorder_parse(asdf_mapping_t *parent, const char *path, asdf_byteorder_t *out);

ASDF_LOCAL asdf_value_err_t
asdf_datatype_shape_parse(asdf_sequence_t *value, asdf_datatype_shape_t *out);
