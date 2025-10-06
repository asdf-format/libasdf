#include <stdbool.h>
#include <stdlib.h>

#include <asdf/core/asdf.h>
#include <asdf/extension.h>
#define ASDF_GWCS_INTERNAL
#include <asdf/gwcs/gwcs.h>
#undef ASDF_GWCS_INTERNAL
#include <asdf/value.h>

#include "../util.h"
#include "../value.h"


typedef struct _asdf_gwcs_step {
    // TODO
    const void *frame;
    const void *transform;
    bool free;
} asdf_gwcs_step_t;


static asdf_value_err_t asdf_gwcs_step_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_gwcs_step_t *step = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;

    if (!asdf_value_is_mapping(value))
        goto failure;

    // gwcs_step is usually not found on its own, but as a sequence of steps
    // in a gwcs, which is normally pre-allocated.
    // If *out is non-null then don't allocate
    if (*out) {
        step = (asdf_gwcs_step_t *)*out;
    } else {
        step = calloc(1, sizeof(asdf_gwcs_step_t));
        step->free = true;
    }

    if (!step) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    if (!*out)
        *out = step;

    return ASDF_VALUE_OK;
failure:
    asdf_value_destroy(prop);
    return err;
}


static void asdf_gwcs_step_dealloc(void *value) {
    if (!value)
        return;

    asdf_gwcs_step_t *step = (asdf_gwcs_step_t *)value;

    if (step->free)
        free(step);
}


ASDF_REGISTER_EXTENSION(
    gwcs_step,
    ASDF_GWCS_TAG_PREFIX "step-1.3.0",
    asdf_gwcs_step_t,
    &libasdf_software,
    asdf_gwcs_step_deserialize,
    asdf_gwcs_step_dealloc,
    NULL);
