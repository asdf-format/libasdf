#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <asdf/core/asdf.h>
#include <asdf/extension.h>
#include <asdf/gwcs/frame.h>
#include <asdf/gwcs/frame2d.h>
#include <asdf/gwcs/gwcs.h>
#include <asdf/log.h>
#include <asdf/value.h>

#include "../extension_util.h"
#include "../util.h"
#include "../value.h"

#include "frame.h"

#ifdef ASDF_LOGGING_ENABLED
static inline void warn_invalid_frame_axes_param(
    asdf_value_t *value,
    const char *propname,
    asdf_value_type_t expected_type,
    uint32_t min_axes,
    uint32_t max_axes) {
    const char *path = asdf_value_path(value);

    if (min_axes == max_axes)
        ASDF_LOG(
            value->file,
            ASDF_LOG_WARN,
            "property %s in %s must be a %d element sequence of %s",
            propname,
            path,
            max_axes,
            asdf_value_type_string(expected_type));
    else
        ASDF_LOG(
            value->file,
            ASDF_LOG_WARN,
            "property %s in %s must be a %d to %d element sequence of %s",
            propname,
            path,
            min_axes max_axes,
            asdf_value_type_string(expected_type));
}
#else
static inline void warn_invalid_frame_axes_param(
    UNUSED(asdf_value_t *value),
    UNUSED(const char *propname),
    UNUSED(asdf_value_type_t expected_type),
    UNUSED(uint32_t min_axes),
    UNUSED(uint32_t max_axes)) {
}
#endif


static asdf_value_err_t get_frame_axes_string_param(
    asdf_value_t *value,
    const char *propname,
    char **strings,
    uint32_t min_axes,
    uint32_t max_axes) {
    asdf_value_err_t prop_err = ASDF_VALUE_OK;
    asdf_value_t *prop = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;

    prop_err = asdf_get_optional_property(value, propname, ASDF_VALUE_SEQUENCE, NULL, &prop);

    if (prop_err != ASDF_VALUE_OK && prop_err != ASDF_VALUE_ERR_NOT_FOUND)
        goto failure;

    uint32_t size = (uint32_t)asdf_sequence_size(prop);

    if (size < min_axes || size > max_axes) {
        warn_invalid_frame_axes_param(value, propname, ASDF_VALUE_STRING, min_axes, max_axes);
        goto failure;
    }

    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *item = NULL;
    char **str_tmp = strings;
    while ((item = asdf_sequence_iter(prop, &iter)) != NULL) {
        if (ASDF_VALUE_OK != asdf_value_as_string0(item, (const char **)str_tmp)) {
            warn_invalid_frame_axes_param(value, propname, ASDF_VALUE_STRING, min_axes, max_axes);
            goto failure;
        }
        str_tmp++;
    }

    asdf_value_destroy(prop);
    return ASDF_VALUE_OK;
failure:
    asdf_value_destroy(prop);
    return err;
}


static asdf_value_err_t get_frame_axes_order_param(
    asdf_value_t *value, uint32_t *ints, uint32_t min_axes, uint32_t max_axes) {
    asdf_value_err_t prop_err = ASDF_VALUE_OK;
    asdf_value_t *prop = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    prop_err = asdf_get_optional_property(value, "axes_order", ASDF_VALUE_SEQUENCE, NULL, &prop);

    if (prop_err != ASDF_VALUE_OK && prop_err != ASDF_VALUE_ERR_NOT_FOUND)
        return err;

    uint32_t size = (uint32_t)asdf_sequence_size(prop);

    if (size < min_axes || size > max_axes) {
        warn_invalid_frame_axes_param(value, "axes_order", ASDF_VALUE_UINT32, min_axes, max_axes);
        goto failure;
    }

    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *item = NULL;
    uint32_t *int_tmp = ints;
    while ((item = asdf_sequence_iter(prop, &iter)) != NULL) {
        if (ASDF_VALUE_OK != asdf_value_as_uint32(item, int_tmp)) {
            warn_invalid_frame_axes_param(
                value, "axes_order", ASDF_VALUE_UINT32, min_axes, max_axes);
            goto failure;
        }

        if (*int_tmp >= max_axes) {
            warn_invalid_frame_axes_param(
                value, "axes_order", ASDF_VALUE_UINT32, min_axes, max_axes);
            goto failure;
        }

        int_tmp++;
    }

    asdf_value_destroy(prop);
    return ASDF_VALUE_OK;
failure:
    asdf_value_destroy(prop);
    return err;
}


asdf_value_err_t asdf_gwcs_frame_parse(
    asdf_value_t *value, asdf_gwcs_frame_t *frame, asdf_gwcs_frame_common_params_t *params) {
    assert(value);
    assert(frame);
    assert(params);
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_err_t prop_err = ASDF_VALUE_OK;
    asdf_value_t *prop = NULL;

    if (!asdf_value_is_mapping(value))
        goto failure;

    prop_err = asdf_get_required_property(value, "name", ASDF_VALUE_STRING, NULL, &prop);

    if (prop_err != ASDF_VALUE_OK)
        goto failure;

    if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &frame->name))
        goto failure;


    if (ASDF_VALUE_OK !=
        get_frame_axes_string_param(
            value, "axes_names", params->axes_names, params->min_axes, params->max_axes))
        goto failure;

    if (ASDF_VALUE_OK != get_frame_axes_string_param(
                             value, "unit", params->unit, params->min_axes, params->max_axes))
        goto failure;


    if (ASDF_VALUE_OK !=
        get_frame_axes_order_param(value, params->axes_order, params->min_axes, params->max_axes))
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

    asdf_gwcs_frame_common_params_t params = {0};

    if (ASDF_VALUE_OK != asdf_gwcs_frame_parse(value, frame, &params))
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
    asdf_gwcs_frame_celestial_t *frame_celestial = NULL;

    if (ASDF_VALUE_OK == asdf_value_as_gwcs_frame2d(value, &frame2d)) {
        frame = (asdf_gwcs_frame_t *)frame2d;
        assert(frame);
    } else if (ASDF_VALUE_OK == asdf_value_as_gwcs_frame_celestial(value, &frame_celestial)) {
        frame = (asdf_gwcs_frame_t *)frame_celestial;
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
        asdf_gwcs_frame_celestial_destroy((asdf_gwcs_frame_celestial_t *)frame);
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
