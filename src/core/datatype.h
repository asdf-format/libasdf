#pragma once

#include <string.h>

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


static inline asdf_byteorder_t asdf_byteorder_from_string(const char *str) {
    if (!str)
        return ASDF_BYTEORDER_INVALID;

    if (strcmp(str, "little") == 0)
        return ASDF_BYTEORDER_LITTLE;

    if (strcmp(str, "big") == 0)
        return ASDF_BYTEORDER_BIG;

    return ASDF_BYTEORDER_INVALID;
}


static inline const char *asdf_byteorder_to_string(asdf_byteorder_t byteorder) {
    switch (byteorder) {
    case ASDF_BYTEORDER_LITTLE:
        return "little";
    case ASDF_BYTEORDER_BIG:
        return "big";
    case ASDF_BYTEORDER_DEFAULT:
    case ASDF_BYTEORDER_INVALID:
    default:
        return NULL;
    }
}
