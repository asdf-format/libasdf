#include "asdf/log.h"
#include <stdbool.h>
#include <stdlib.h>

#include <asdf/core/asdf.h>
#include <asdf/extension.h>
#define ASDF_GWCS_INTERNAL
#include <asdf/gwcs/frame.h>
#include <asdf/gwcs/frame2d.h>
#include <asdf/gwcs/gwcs.h>
#undef ASDF_GWCS_INTERNAL
#include <asdf/value.h>

#include "../extension_util.h"
#include "../log.h"
#include "../util.h"
#include "../value.h"
#include "step.h"


static asdf_value_err_t asdf_gwcs_step_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_gwcs_step_t *step = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;
    asdf_gwcs_frame_t *frame = NULL;

    if (!asdf_value_is_mapping(value))
        goto failure;

    // gwcs_step is usually not found on its own, but as a sequence of steps
    // in a gwcs, which is normally pre-allocated.
    // If *out is non-null then don't allocate
    if (*out) {
        step = (asdf_gwcs_step_t *)*out;
        step->free = false;
    } else {
        step = calloc(1, sizeof(asdf_gwcs_step_t));
        step->free = true;
    }

    if (!step) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    if ((prop = asdf_get_required_property(value, "frame", ASDF_VALUE_UNKNOWN, NULL))) {
        asdf_value_as_gwcs_frame(prop, &frame);
        asdf_value_destroy(prop);
    }

    if (!frame) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(value);
        ASDF_LOG(value->file, ASDF_LOG_WARN, "invalid frame value for step at %s", path);
#endif
        goto failure;
    }

    step->frame = frame;

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

    if (step->frame)
        asdf_gwcs_frame_destroy(step->frame);

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
