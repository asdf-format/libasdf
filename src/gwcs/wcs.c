#include <stdlib.h>

#include <asdf/core/asdf.h>
#include <asdf/extension.h>
#include <asdf/gwcs/gwcs.h>
#include <asdf/value.h>

#include "../util.h"


static asdf_value_err_t asdf_gwcs_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    assert(value);
    *out = NULL;
    return ASDF_VALUE_OK;
}


static void asdf_gwcs_dealloc(void *value) {
    if (!value)
        return;

    free(value);
}


ASDF_REGISTER_EXTENSION(
    gwcs,
    ASDF_GWCS_TAG_PREFIX "wcs-1.4.0",
    asdf_gwcs_t,
    &libasdf_software,
    asdf_gwcs_deserialize,
    asdf_gwcs_dealloc,
    NULL);
