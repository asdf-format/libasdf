#include <stdlib.h>

#include "../../../extension_util.h"
#include "../../../log.h"
#include "../../../value.h"

#include "../../gwcs.h"


/** Helper to parse bounding box intervals from mapping items */
static asdf_value_err_t asdf_gwcs_interval_parse(
    asdf_mapping_item_t *item, asdf_gwcs_interval_t *out) {
    asdf_value_t *bounds = NULL;
    asdf_sequence_t *bounds_seq = NULL;
    asdf_value_t *bound_val = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;

    out->input_name = asdf_mapping_item_key(item);
    bounds = asdf_mapping_item_value(item);

    if (asdf_value_as_sequence(bounds, &bounds_seq) != ASDF_VALUE_OK)
        goto cleanup;


    if (asdf_sequence_size(bounds_seq) != 2)
        goto cleanup;

    for (int idx = 0; idx < 2; idx++) {
        double bound = 0.0;
        bound_val = asdf_sequence_get(bounds_seq, idx);

        if (!bound_val)
            goto cleanup;

        if (asdf_value_as_double(bound_val, &bound))
            goto cleanup;

        asdf_value_destroy(bound_val);
        bound_val = NULL;
        out->bounds[idx] = bound;
    }

    err = ASDF_VALUE_OK;
cleanup:
    asdf_value_destroy(bound_val);
    return err;
}


static asdf_value_err_t asdf_gwcs_bounding_box_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_gwcs_bounding_box_t *bounding_box = NULL;
    asdf_mapping_t *intervals_map = NULL;
    asdf_gwcs_interval_t *intervals = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_mapping_t *bbox_map = NULL;
    asdf_sequence_t *bounds_seq = NULL;

    if (asdf_value_as_mapping(value, &bbox_map) != ASDF_VALUE_OK)
        goto cleanup;

    bounding_box = calloc(1, sizeof(asdf_gwcs_bounding_box_t));

    if (!bounding_box) {
        err = ASDF_VALUE_ERR_OOM;
        goto cleanup;
    }

    err = asdf_get_required_property(
        bbox_map, "intervals", ASDF_VALUE_MAPPING, NULL, &intervals_map);

    if (ASDF_VALUE_OK != err)
        goto cleanup;

    int n_intervals = asdf_mapping_size(intervals_map);

    if (n_intervals < 1) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(value);
        ASDF_LOG(value->file, ASDF_LOG_WARN, "insufficient intervals in bounding_box at %s", path);
#endif

        goto cleanup;
    }

    intervals = calloc(n_intervals, sizeof(asdf_gwcs_interval_t));

    if (!intervals) {
        err = ASDF_VALUE_ERR_OOM;
        goto cleanup;
    }

    asdf_mapping_iter_t iter = asdf_mapping_iter_init();
    asdf_mapping_item_t *item = NULL;
    asdf_gwcs_interval_t *interval_tmp = intervals;

    while ((item = asdf_mapping_iter(intervals_map, &iter))) {
        if (asdf_gwcs_interval_parse(item, interval_tmp) != ASDF_VALUE_OK)
            goto cleanup;

        interval_tmp++;
    }

    bounding_box->n_intervals = n_intervals;
    bounding_box->intervals = intervals;

    // TODO: Parse order and ignore

    if (!*out)
        *out = bounding_box;

    err = ASDF_VALUE_OK;
cleanup:
    asdf_sequence_destroy(bounds_seq);
    asdf_mapping_destroy(intervals_map);
    if (err != ASDF_VALUE_OK) {
        free(intervals);
        free(bounding_box);
    }
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
