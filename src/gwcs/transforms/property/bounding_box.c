#include <stdlib.h>

#include <asdf/core/asdf.h>
#define ASDF_GWCS_INTERNAL
#include <asdf/gwcs/transform.h>
#include <asdf/gwcs/transforms/property/bounding_box.h>
#undef ASDF_GWCS_INTERNAL
#include <asdf/value.h>

#include "../../../extension_util.h"
#include "../../../log.h"


static asdf_value_err_t asdf_gwcs_bounding_box_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_gwcs_bounding_box_t *bounding_box = NULL;
    asdf_gwcs_interval_t *intervals = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;

    if (!asdf_value_is_mapping(value))
        goto failure;

    bounding_box = calloc(1, sizeof(asdf_gwcs_bounding_box_t));

    if (!bounding_box) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    err = asdf_get_required_property(value, "intervals", ASDF_VALUE_MAPPING, NULL, &prop);

    if (ASDF_VALUE_OK != err)
        goto failure;

    int n_intervals = asdf_mapping_size(prop);

    if (n_intervals < 1) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(value);
        ASDF_LOG(value->file, ASDF_LOG_WARN, "insufficient intervals in bounding_box at %s", path);
#endif

        goto failure;
    }

    intervals = calloc(n_intervals, sizeof(asdf_gwcs_interval_t));

    if (!intervals) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    asdf_mapping_iter_t iter = asdf_mapping_iter_init();
    asdf_mapping_item_t *item = NULL;
    asdf_gwcs_interval_t *interval_tmp = intervals;

    while ((item = asdf_mapping_iter(prop, &iter))) {
        interval_tmp->input_name = asdf_mapping_item_key(item);
        asdf_value_t *bounds = asdf_mapping_item_value(item);
        if (!asdf_value_is_sequence(bounds) || asdf_sequence_size(bounds) != 2)
            goto failure;

        for (int idx = 0; idx < 2; idx++) {
            double bound = 0.0;
            asdf_value_t *boundval = asdf_sequence_get(bounds, idx);

            if (!boundval)
                goto failure;

            if (asdf_value_as_double(boundval, &bound))
                goto failure;

            interval_tmp->bounds[idx] = bound;
        }
        interval_tmp++;
    }

    bounding_box->n_intervals = n_intervals;
    bounding_box->intervals = intervals;

    // TODO: Parse order and ignore

    if (!*out)
        *out = bounding_box;

    return ASDF_VALUE_OK;
failure:
    free(intervals);
    free(bounding_box);
    asdf_value_destroy(prop);
    return err;
}


static void asdf_gwcs_bounding_box_dealloc(void *value) {
    if (!value)
        return;

    asdf_gwcs_bounding_box_t *bounding_box = (asdf_gwcs_bounding_box_t *)value;

    if (bounding_box->intervals)
        free((void *)bounding_box->intervals);

    free(bounding_box);
}


ASDF_REGISTER_EXTENSION(
    gwcs_bounding_box,
    ASDF_GWCS_BOUNDING_BOX_TAG,
    asdf_gwcs_bounding_box_t,
    &libasdf_software,
    asdf_gwcs_bounding_box_deserialize,
    asdf_gwcs_bounding_box_dealloc,
    NULL);
