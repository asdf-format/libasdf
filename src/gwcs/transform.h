/** Internal utilities for parsing generic transforms */
#pragma once

#define ASDF_GWCS_INTERNAL
#include "asdf/gwcs/transform.h" // IWYU pragma: export
#undef ASDF_GWCS_INTERNAL

#include "../util.h"
#include "../value.h"


ASDF_LOCAL asdf_value_err_t
asdf_gwcs_transform_parse(asdf_value_t *value, asdf_gwcs_transform_t *transform);

/**
 * Release memory held by fields in an `asdf_gwcs_transform_t` and clear it in preparation
 * for releasing the transform's memory
 *
 * Does not free the transform struct's own memory.
 */
ASDF_LOCAL void asdf_gwcs_transform_clean(asdf_gwcs_transform_t *transform);

/**
 * Read an `asdf_value_t *` as any type of GWCS transform
 */
ASDF_EXPORT asdf_value_err_t
asdf_value_as_gwcs_transform(asdf_value_t *value, asdf_gwcs_transform_t **out);
ASDF_EXPORT void asdf_gwcs_transform_destroy(asdf_gwcs_transform_t *transform);
