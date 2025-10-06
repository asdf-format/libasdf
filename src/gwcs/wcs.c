#include <stdlib.h>

#include <asdf/core/asdf.h>
#include <asdf/extension.h>
#include <asdf/gwcs/gwcs.h>
#include <asdf/value.h>

#include "../extension_util.h"
#include "../value.h"


static asdf_value_err_t asdf_gwcs_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_gwcs_t *gwcs = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;

    if (!asdf_value_is_mapping(value))
        goto failure;

    gwcs = calloc(1, sizeof(asdf_gwcs_t));

    if (!gwcs) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    // The name property is required
    if (!(prop = asdf_get_required_property(value, "name", ASDF_VALUE_STRING, NULL)))
        goto failure;

    if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &gwcs->name))
        goto failure;

    asdf_value_destroy(prop);

    // TODO: Implement pixel_shape parsing

    // Parse steps
    if (!(prop = asdf_get_required_property(value, "steps", ASDF_VALUE_SEQUENCE, NULL)))
        goto failure;

    int n_steps = asdf_sequence_size(prop);

    if (n_steps < 0)
        goto failure;

    gwcs->n_steps = (uint32_t)n_steps;

    asdf_gwcs_step_t *steps = calloc(n_steps, sizeof(asdf_gwcs_step_t));

    if (!steps) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    gwcs->steps = steps;

    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *item = NULL;
    asdf_gwcs_step_t *step_tmp = steps;
    while ((item = asdf_sequence_iter(prop, &iter)) != NULL) {
        if (ASDF_VALUE_OK != asdf_value_as_gwcs_step(item, &step_tmp))
            goto failure;

        step_tmp++;
    }

    asdf_value_destroy(prop);

    *out = gwcs;
    return ASDF_VALUE_OK;
failure:
    asdf_value_destroy(prop);
    asdf_gwcs_destroy(gwcs);
    return err;
}


static void asdf_gwcs_dealloc(void *value) {
    if (!value)
        return;

    asdf_gwcs_t *gwcs = (asdf_gwcs_t *)value;

    if (gwcs->steps) {
        for (uint32_t idx = 0; idx < gwcs->n_steps; idx++) {
            asdf_gwcs_step_t *step = (asdf_gwcs_step_t *)&gwcs->steps[idx];
            asdf_gwcs_step_destroy(step);
        }
    }

    free(gwcs);
}


ASDF_REGISTER_EXTENSION(
    gwcs,
    ASDF_GWCS_TAG_PREFIX "wcs-1.4.0",
    asdf_gwcs_t,
    &libasdf_software,
    asdf_gwcs_deserialize,
    asdf_gwcs_dealloc,
    NULL);
