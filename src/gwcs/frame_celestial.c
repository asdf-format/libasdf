#include <asdf/core/asdf.h>
#include <asdf/gwcs/gwcs.h>
#include <asdf/log.h>

#include "../util.h"
#include "../value.h"

#include "asdf/gwcs/frame.h"
#include "asdf/value.h"
#include "frame.h"


static asdf_value_err_t asdf_gwcs_frame_celestial_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_gwcs_frame_celestial_t *frame_celestial = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;

    frame_celestial = calloc(1, sizeof(asdf_gwcs_frame_celestial_t));

    if (!frame_celestial) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    asdf_gwcs_frame_common_params_t params = {
        .min_axes = 2,
        .max_axes = 3,
        .axes_names = (char **)frame_celestial->axes_names,
        .axes_order = frame_celestial->axes_order,
        .unit = (char **)frame_celestial->unit};

    if (ASDF_VALUE_OK !=
        asdf_gwcs_frame_parse(value, (asdf_gwcs_frame_t *)frame_celestial, &params))
        goto failure;

    frame_celestial->base.type = ASDF_GWCS_FRAME_CELESTIAL;
    *out = frame_celestial;
    return ASDF_VALUE_OK;
failure:
    asdf_gwcs_frame_celestial_destroy(frame_celestial);
    return err;
}


static void asdf_gwcs_frame_celestial_dealloc(void *value) {
    if (!value)
        return;

    asdf_gwcs_base_frame_t *frame = (asdf_gwcs_base_frame_t *)value;
    asdf_gwcs_base_frame_destroy(frame);
}


ASDF_REGISTER_EXTENSION(
    gwcs_frame_celestial,
    ASDF_GWCS_TAG_PREFIX "celestial_frame-1.2.0",
    asdf_gwcs_frame_celestial_t,
    &libasdf_software,
    asdf_gwcs_frame_celestial_deserialize,
    asdf_gwcs_frame_celestial_dealloc,
    NULL);
