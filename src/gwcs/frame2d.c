#include <asdf/core/asdf.h>
#include <asdf/gwcs/gwcs.h>
#include <asdf/log.h>

#include "../extension_util.h"
#include "../util.h"
#include "../value.h"

#include "asdf/gwcs/frame.h"
#include "asdf/value.h"
#include "frame.h"


#ifdef ASDF_LOGGING_ENABLED
static inline void warn_invalid_frame2d_param(
    asdf_value_t *value, const char *propname, asdf_value_type_t expected_type) {
    const char *path = asdf_value_path(value);
    ASDF_LOG(
        value->file,
        ASDF_LOG_WARN,
        "property %s in %s must be a two element sequence of %s",
        propname,
        path,
        asdf_value_type_string(expected_type));
}
#else
static inline void warn_invalid_frame2d_param(
    UNUSED(asdf_value_t *value),
    UNUSED(const char *propname),
    UNUSED(asdf_value_type_t expected_type)) {
}
#endif


static asdf_value_err_t get_frame2d_string_param(
    asdf_value_t *value, const char *propname, char **strings) {
    asdf_value_t *prop = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    prop = asdf_get_optional_property(value, propname, ASDF_VALUE_SEQUENCE, NULL);

    if (!prop)
        return err;

    int size = asdf_sequence_size(prop);

    if (size != 2) {
        warn_invalid_frame2d_param(value, propname, ASDF_VALUE_STRING);
        goto failure;
    }

    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *item = NULL;
    char **str_tmp = strings;
    while ((item = asdf_sequence_iter(prop, &iter)) != NULL) {
        if (ASDF_VALUE_OK != asdf_value_as_string0(item, (const char **)str_tmp)) {
            warn_invalid_frame2d_param(value, propname, ASDF_VALUE_STRING);
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


static asdf_value_err_t get_frame2d_uint8_param(
    asdf_value_t *value, const char *propname, uint8_t *ints) {
    asdf_value_t *prop = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    prop = asdf_get_optional_property(value, propname, ASDF_VALUE_SEQUENCE, NULL);

    if (!prop)
        return err;

    int size = asdf_sequence_size(prop);

    if (size != 2) {
        warn_invalid_frame2d_param(value, propname, ASDF_VALUE_UINT8);
        goto failure;
    }

    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *item = NULL;
    uint8_t *int_tmp = ints;
    while ((item = asdf_sequence_iter(prop, &iter)) != NULL) {
        if (ASDF_VALUE_OK != asdf_value_as_uint8(item, int_tmp)) {
            warn_invalid_frame2d_param(value, propname, ASDF_VALUE_UINT8);
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


static asdf_value_err_t asdf_gwcs_frame2d_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_gwcs_frame2d_t *frame2d = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;

    frame2d = calloc(1, sizeof(asdf_gwcs_frame2d_t));

    if (!frame2d) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    if (ASDF_VALUE_OK != asdf_gwcs_frame_parse(value, (asdf_gwcs_frame_t *)frame2d))
        goto failure;

    if (ASDF_VALUE_OK !=
        get_frame2d_string_param(value, "axes_names", (char **)frame2d->axes_names))
        goto failure;

    if (ASDF_VALUE_OK != get_frame2d_string_param(value, "unit", (char **)frame2d->unit))
        goto failure;

    if (ASDF_VALUE_OK !=
        get_frame2d_uint8_param(value, "axes_order", (uint8_t *)frame2d->axes_order))
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
