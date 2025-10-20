/**
 * Useful utilities for extension / schema parsing
 */

#pragma once

#include <asdf/util.h>

#include "value.h"


#define ASDF_IS_OK(err) (ASDF_VALUE_OK == (err))

#define ASDF_IS_ERR(err) (UNLIKELY(!ASDF_IS_OK((err))))

#define ASDF_IS_OPTIONAL_OK(err) (ASDF_VALUE_OK == (err) || ASDF_VALUE_ERR_NOT_FOUND == (err))


/**
 * Helper to look up required properties within a mapping and log a warning if
 * missing
 *
 * .. todo::
 *
 *   Might be more useful to require specific extension types by a constant,
 *   but we don't currently export extension constants--will probably want to
 *   for external plugin support though.
 */
ASDF_LOCAL asdf_value_err_t asdf_get_required_property(
    asdf_value_t *mapping, const char *name, asdf_value_type_t type, const char *tag, void *out);

/**
 * Like `asdf_get_required_property` but allows the property to be missing
 *
 * However, if the property is present, still performs type checking.
 */
ASDF_LOCAL asdf_value_err_t asdf_get_optional_property(
    asdf_value_t *mapping, const char *name, asdf_value_type_t type, const char *tag, void *out);
