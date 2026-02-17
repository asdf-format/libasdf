#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "../extension_util.h"
#include "../util.h"
#include "../value.h"

#include "frame.h"
#include "gwcs.h"

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
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    UNUSED(asdf_value_type_t expected_type),
    UNUSED(uint32_t min_axes),
    UNUSED(uint32_t max_axes)) {
}
#endif


static asdf_value_err_t get_frame_axes_string_param(
    asdf_mapping_t *value,
    const char *propname,
    char **strings,
    uint32_t min_axes,
    uint32_t max_axes) {
    asdf_sequence_t *frames_seq = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;

    err = asdf_get_optional_property(
        value, propname, ASDF_VALUE_SEQUENCE, NULL, (void *)&frames_seq);

    if (!ASDF_IS_OPTIONAL_OK(err))
        goto cleanup;

    uint32_t size = (uint32_t)asdf_sequence_size(frames_seq);

    if (size < min_axes || size > max_axes) {
        warn_invalid_frame_axes_param(
            &value->value, propname, ASDF_VALUE_STRING, min_axes, max_axes);
        goto cleanup;
    }

    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *item = NULL;
    char **str_tmp = strings;
    while ((item = asdf_sequence_iter(frames_seq, &iter)) != NULL) {
        if (!ASDF_IS_OK(asdf_value_as_string0(item, (const char **)str_tmp))) {
            warn_invalid_frame_axes_param(
                &value->value, propname, ASDF_VALUE_STRING, min_axes, max_axes);
            goto cleanup;
        }
        str_tmp++;
    }

    err = ASDF_VALUE_OK;
cleanup:
    asdf_sequence_destroy(frames_seq);
    return err;
}


static asdf_value_err_t get_frame_axes_order_param(
    asdf_mapping_t *value, uint32_t *ints, uint32_t min_axes, uint32_t max_axes) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_sequence_t *axes_seq = NULL;
    assert(ints);

    err = asdf_get_optional_property(
        value, "axes_order", ASDF_VALUE_SEQUENCE, NULL, (void *)&axes_seq);

    if (!ASDF_IS_OPTIONAL_OK(err))
        goto cleanup;

    uint32_t size = (uint32_t)asdf_sequence_size(axes_seq);

    if (size < min_axes || size > max_axes) {
        warn_invalid_frame_axes_param(
            &value->value, "axes_order", ASDF_VALUE_UINT32, min_axes, max_axes);
        goto cleanup;
    }

    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *item = NULL;
    uint32_t *int_tmp = ints;
    while ((item = asdf_sequence_iter(axes_seq, &iter)) != NULL) {
        if (ASDF_VALUE_OK != asdf_value_as_uint32(item, int_tmp)) {
            warn_invalid_frame_axes_param(
                &value->value, "axes_order", ASDF_VALUE_UINT32, min_axes, max_axes);
            goto cleanup;
        }

        if (*int_tmp >= max_axes) {
            warn_invalid_frame_axes_param(
                &value->value, "axes_order", ASDF_VALUE_UINT32, min_axes, max_axes);
            goto cleanup;
        }

        int_tmp++;
    }

    err = ASDF_VALUE_OK;
cleanup:
    asdf_sequence_destroy(axes_seq);
    return err;
}


asdf_value_err_t asdf_gwcs_frame_parse(
    asdf_value_t *value, asdf_gwcs_frame_t *frame, asdf_gwcs_frame_common_params_t *params) {
    assert(value);
    assert(frame);
    assert(params);
    asdf_mapping_t *frame_map = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;

    if (asdf_value_as_mapping(value, &frame_map) != ASDF_VALUE_OK)
        goto failure;

    err = asdf_get_required_property(
        frame_map, "name", ASDF_VALUE_STRING, NULL, (void *)&frame->name);

    if (ASDF_IS_ERR(err))
        goto failure;

    if (!ASDF_IS_OPTIONAL_OK(get_frame_axes_string_param(
            frame_map, "axes_names", params->axes_names, params->min_axes, params->max_axes)))
        goto failure;

    if (!ASDF_IS_OPTIONAL_OK(get_frame_axes_string_param(
            frame_map, "unit", params->unit, params->min_axes, params->max_axes)))
        goto failure;

    if (!ASDF_IS_OPTIONAL_OK(get_frame_axes_string_param(
            frame_map,
            "axis_physical_types",
            params->axis_physical_types,
            params->min_axes,
            params->max_axes)))
        goto failure;

    if (!(ASDF_IS_OPTIONAL_OK(get_frame_axes_order_param(
            frame_map, params->axes_order, params->min_axes, params->max_axes))))
        goto failure;

    return ASDF_VALUE_OK;
failure:
    return err;
}


static asdf_value_err_t asdf_gwcs_base_frame_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_gwcs_frame_t *frame = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;

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
    NULL,
    asdf_gwcs_base_frame_deserialize,
    NULL, /* TODO: copy */
    asdf_gwcs_base_frame_dealloc,
    NULL);
