/** Internal utilities for parsing generic transforms */
#pragma once

#define ASDF_GWCS_INTERNAL
#include <asdf/gwcs/transform.h>
#undef ASDF_GWCS_INTERNAL
#include <asdf/util.h>
#include <asdf/value.h>

ASDF_LOCAL asdf_value_err_t asdf_gwcs_transform_parse(
    asdf_value_t *value, asdf_gwcs_transform_t *transform);
