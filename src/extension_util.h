/**
 * Useful utilities for extension / schema parsing
 */

#pragma once

#include <asdf/util.h>

#include "value.h"


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
ASDF_LOCAL asdf_value_t *asdf_get_required_property(
    asdf_value_t *mapping, const char *name, asdf_value_type_t type, const char *tag);
