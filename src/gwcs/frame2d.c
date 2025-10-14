#include <asdf/core/asdf.h>
#include <asdf/gwcs/gwcs.h>
#include <asdf/log.h>

#include "../util.h"
#include "../value.h"

#include "asdf/gwcs/frame.h"
#include "asdf/value.h"
#include "frame.h"


static asdf_value_err_t asdf_gwcs_frame2d_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_gwcs_frame2d_t *frame2d = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;

    frame2d = calloc(1, sizeof(asdf_gwcs_frame2d_t));

    if (!frame2d) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    asdf_gwcs_frame_common_params_t params = {
        .min_axes = 2,
        .max_axes = 2,
        .axes_names = (char **)frame2d->axes_names,
        .axes_order = frame2d->axes_order,
        .unit = (char **)frame2d->unit};

    if (ASDF_VALUE_OK != asdf_gwcs_frame_parse(value, (asdf_gwcs_frame_t *)frame2d, &params))
        goto failure;

    frame2d->base.type = ASDF_GWCS_FRAME_2D;
    *out = frame2d;
    return ASDF_VALUE_OK;
failure:
    asdf_gwcs_frame2d_destroy(frame2d);
    return err;
}


static void asdf_gwcs_frame2d_dealloc(void *value) {
    if (!value)
        return;

    asdf_gwcs_base_frame_t *frame = (asdf_gwcs_base_frame_t *)value;
    asdf_gwcs_base_frame_destroy(frame);
}


ASDF_REGISTER_EXTENSION(
    gwcs_frame2d,
    ASDF_GWCS_TAG_PREFIX "frame2d-1.2.0",
    asdf_gwcs_frame2d_t,
    &libasdf_software,
    asdf_gwcs_frame2d_deserialize,
    asdf_gwcs_frame2d_dealloc,
    NULL);
