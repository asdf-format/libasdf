#include <stdbool.h>
#include <stdlib.h>

#include <asdf/core/asdf.h>
#include <asdf/extension.h>
#include <asdf/gwcs/frame.h>
#include <asdf/gwcs/frame2d.h>
#include <asdf/gwcs/gwcs.h>
#include <asdf/value.h>

#include "../extension_util.h"
#include "../util.h"
#include "../value.h"

#include "frame.h"


asdf_value_err_t asdf_gwcs_frame_parse(asdf_value_t *value, asdf_gwcs_frame_t *frame) {
    assert(value);
    assert(frame);
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;

    if (!asdf_value_is_mapping(value))
        goto failure;

    if (!(prop = asdf_get_required_property(value, "name", ASDF_VALUE_STRING, NULL)))
        goto failure;

    if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &frame->name))
        goto failure;

    asdf_value_destroy(prop);
    return ASDF_VALUE_OK;
failure:
    asdf_value_destroy(prop);
    return err;
}


static asdf_value_err_t asdf_gwcs_base_frame_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_gwcs_frame_t *frame = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;

    frame = calloc(1, sizeof(asdf_gwcs_frame_t));

    if (!frame) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    if (ASDF_VALUE_OK != asdf_gwcs_frame_parse(value, frame))
        goto failure;

    *out = frame;
    return ASDF_VALUE_OK;
failure:
    asdf_value_destroy(prop);
    asdf_gwcs_base_frame_destroy(frame);
    return err;
}


static void asdf_gwcs_base_frame_dealloc(void *value) {
    if (!value)
        return;

    asdf_gwcs_base_frame_t *frame = (asdf_gwcs_base_frame_t *)value;
    free(frame);
}


// Generic constructor for frames of different types from a value, depending on the tag
asdf_value_err_t asdf_value_as_gwcs_frame(asdf_value_t *value, asdf_gwcs_frame_t **out) {
    // TODO: It will be useful in the future to have some registery of known frame
    // extension types.  Because there are only two currently it's hard-coded for now,
    // but this is a bit ugly...
    asdf_gwcs_frame_t *frame = NULL;
    asdf_gwcs_frame2d_t *frame2d = NULL;

    if (ASDF_VALUE_OK == asdf_value_as_gwcs_frame2d(value, &frame2d)) {
        frame = (asdf_gwcs_frame_t *)frame2d;
        assert(frame);
    } else if (ASDF_VALUE_OK == asdf_value_as_gwcs_frame_celestial(value, &frame)) {
        assert(frame);
    } else if (ASDF_VALUE_OK != asdf_value_as_gwcs_base_frame(value, &frame)) {
        frame = NULL;
    }

    if (frame) {
        *out = frame;
        return ASDF_VALUE_OK;
    }

    return ASDF_VALUE_ERR_TYPE_MISMATCH;
}


// Generic destructor for frames of different types from a value, depending on the tag
void asdf_gwcs_frame_destroy(asdf_gwcs_frame_t *frame) {
    if (!frame)
        return;

    switch (frame->type) {
    case ASDF_GWCS_FRAME_2D:
        asdf_gwcs_frame2d_destroy((asdf_gwcs_frame2d_t *)frame);
        break;
    case ASDF_GWCS_FRAME_CELESTIAL:
        asdf_gwcs_frame_celestial_destroy(frame);
        break;
    case ASDF_GWCS_FRAME_GENERIC:
        asdf_gwcs_base_frame_destroy(frame);
        break;
    }
}


ASDF_REGISTER_EXTENSION(
    gwcs_base_frame,
    ASDF_GWCS_TAG_PREFIX "frame-1.2.0",
    asdf_gwcs_base_frame_t,
    &libasdf_software,
    asdf_gwcs_base_frame_deserialize,
    asdf_gwcs_base_frame_dealloc,
    NULL);


// TODO: Create proper extension for this frame type
ASDF_REGISTER_EXTENSION(
    gwcs_frame_celestial,
    ASDF_GWCS_TAG_PREFIX "celestial_frame-1.2.0",
    asdf_gwcs_frame_t,
    &libasdf_software,
    asdf_gwcs_base_frame_deserialize,
    asdf_gwcs_base_frame_dealloc,
    NULL);
