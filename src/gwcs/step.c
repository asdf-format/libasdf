#include <stdbool.h>
#include <stdlib.h>

#include "gwcs.h"
#include "step.h"
#include "transform.h"

#include "../extension_util.h"
#include "../log.h"
#include "../util.h"
#include "../value.h"


static asdf_value_err_t asdf_gwcs_step_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_gwcs_step_t *step = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;
    asdf_mapping_t *step_map = NULL;
    asdf_gwcs_frame_t *frame = NULL;
    asdf_gwcs_transform_t *transform = NULL;

    if (asdf_value_as_mapping(value, &step_map) != ASDF_VALUE_OK)
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

    err = asdf_get_required_property(step_map, "frame", ASDF_VALUE_UNKNOWN, NULL, (void *)&prop);

    if (ASDF_IS_OK(err)) {
        err = asdf_value_as_gwcs_frame(prop, &frame);
        asdf_value_destroy(prop);
    }

    if (ASDF_IS_ERR(err) || !frame) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(value);
        ASDF_LOG(value->file, ASDF_LOG_WARN, "invalid frame value for step at %s", path);
#endif
        goto failure;
    }

    step->frame = frame;

    err = asdf_get_optional_property(
        step_map, "transform", ASDF_VALUE_UNKNOWN, NULL, (void *)&prop);

    if (ASDF_IS_OK(err)) {
        // transform may be null
        if (!asdf_value_is_null(prop))
            err = asdf_value_as_gwcs_transform(prop, &transform);

        asdf_value_destroy(prop);
    }

    if (ASDF_IS_ERR(err)) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(value);
        ASDF_LOG(value->file, ASDF_LOG_WARN, "invalid transform value for step at %s", path);
#endif
        goto failure;
    }

    step->transform = transform;

    if (!*out)
        *out = step;

    return ASDF_VALUE_OK;
failure:
    asdf_gwcs_transform_destroy(transform);
    if (step && step->free)
        free(step);

    return err;
}


static void asdf_gwcs_step_dealloc(void *value) {
    if (!value)
        return;

    asdf_gwcs_step_t *step = (asdf_gwcs_step_t *)value;

    if (step->frame)
        asdf_gwcs_frame_destroy(step->frame);

    if (step->transform)
        asdf_gwcs_transform_destroy((asdf_gwcs_transform_t *)step->transform);

    if (step->free)
        free(step);
}


ASDF_REGISTER_EXTENSION(
    gwcs_step,
    ASDF_GWCS_TAG_PREFIX "step-1.3.0",
    asdf_gwcs_step_t,
    &libasdf_software,
    NULL,
    asdf_gwcs_step_deserialize,
    asdf_gwcs_step_dealloc,
    NULL);
