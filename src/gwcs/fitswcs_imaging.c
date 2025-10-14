#include <assert.h>
#include <stdlib.h>

#include <asdf/core/asdf.h>
#include <asdf/core/ndarray.h>
#include <string.h>
#define ASDF_GWCS_INTERNAL
#include <asdf/gwcs/fitswcs_imaging.h>
#include <asdf/gwcs/transforms/property/bounding_box.h>
#undef ASDF_GWCS_INTERNAL
#include <asdf/value.h>

#include "../extension_util.h"
#include "../log.h"

#include "transform.h"


/** Helper to read crpix, crval, etc. */
static asdf_value_err_t get_coordinates_prop(asdf_value_t *value, const char *name, double *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;
    asdf_ndarray_t *ndarray = NULL;
    void *data = NULL;
    size_t size = 0;

    assert(value);

    // TODO: Fix this, but asdf_get_optional_property doesn't work correctly yet with
    // extension types....
    // err = asdf_get_required_property(
    //     value, name, ASDF_VALUE_EXTENSION, ASDF_CORE_NDARRAY_TAG, &prop);
    err = asdf_get_required_property(value, name, ASDF_VALUE_UNKNOWN, NULL, &prop);

    if (ASDF_VALUE_OK != err)
        goto failure;

    err = asdf_value_as_ndarray(prop, &ndarray);

    if (ASDF_VALUE_OK != err || UNLIKELY(!ndarray))
        goto failure;

    if (ndarray->ndim != 1 || ndarray->shape[0] != 2 ||
        ndarray->datatype.type != ASDF_DATATYPE_FLOAT64) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(value);
        ASDF_LOG(value->file, ASDF_LOG_ERROR, "invalid %s array in %s", name, path);
#endif
        goto failure;
    }

    data = asdf_ndarray_data_raw(ndarray, &size);

    if (size != 2 * sizeof(double)) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(value);
        ASDF_LOG(value->file, ASDF_LOG_ERROR, "invalid array data for %s in %s", name, path);
#endif
        goto failure;
    }

    memcpy(out, data, size);
    return ASDF_VALUE_OK;
failure:
    asdf_ndarray_destroy(ndarray);
    asdf_value_destroy(prop);
    return err;
}


/** Helper to read the pc matrix */
static asdf_value_err_t get_matrix_prop(asdf_value_t *value, const char *name, double *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;
    asdf_ndarray_t *ndarray = NULL;
    void *data = NULL;
    size_t size = 0;

    assert(value);

    // TODO: Fix this, but asdf_get_optional_property doesn't work correctly yet with
    // extension types....
    // err = asdf_get_required_property(
    //     value, name, ASDF_VALUE_EXTENSION, ASDF_CORE_NDARRAY_TAG, &prop);
    err = asdf_get_required_property(value, name, ASDF_VALUE_UNKNOWN, NULL, &prop);

    if (ASDF_VALUE_OK != err)
        goto failure;

    err = asdf_value_as_ndarray(prop, &ndarray);

    if (ASDF_VALUE_OK != err || UNLIKELY(!ndarray))
        goto failure;

    if (ndarray->ndim != 2 || ndarray->shape[0] != 2 || ndarray->shape[1] != 2 ||
        ndarray->datatype.type != ASDF_DATATYPE_FLOAT64) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(value);
        ASDF_LOG(value->file, ASDF_LOG_ERROR, "invalid %s array in %s", name, path);
#endif
        goto failure;
    }

    data = asdf_ndarray_data_raw(ndarray, &size);

    if (size != 2 * 2 * sizeof(double)) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(value);
        ASDF_LOG(value->file, ASDF_LOG_ERROR, "invalid array data for %s in %s", name, path);
#endif
        goto failure;
    }

    memcpy(out, data, size);
    err = ASDF_VALUE_OK;
failure:
    asdf_ndarray_destroy(ndarray);
    asdf_value_destroy(prop);
    return err;
}


static asdf_value_err_t asdf_gwcs_fits_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_gwcs_fits_t *fits = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;
    asdf_ndarray_t *crpix = NULL;
    asdf_ndarray_t *crval = NULL;

    if (!asdf_value_is_mapping(value))
        goto failure;

    fits = calloc(1, sizeof(asdf_gwcs_fits_t));

    if (!fits) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    err = asdf_gwcs_transform_parse(value, &fits->base);

    if (ASDF_VALUE_OK != err)
        goto failure;

    err = get_coordinates_prop(value, "crpix", (double *)fits->crpix);

    if (ASDF_VALUE_OK != err)
        goto failure;

    err = get_coordinates_prop(value, "crval", (double *)fits->crval);

    if (ASDF_VALUE_OK != err)
        goto failure;

    err = get_coordinates_prop(value, "cdelt", (double *)fits->cdelt);

    if (ASDF_VALUE_OK != err)
        goto failure;

    err = get_matrix_prop(value, "pc", (double *)fits->pc);

    if (ASDF_VALUE_OK != err)
        goto failure;

    err = asdf_get_required_property(value, "projection", ASDF_VALUE_UNKNOWN, NULL, &prop);
    // projection must be a transform; in fact it can't even be just any arbitrary
    // transform according to the schema, but one matching any of the base transform
    // schemas
    //
    // - zenithal
    // - conic
    // - quadcube
    // - pseudoconic
    // - pseudocylindrical
    // - cylindrical
    //
    // Here we have the interesting question of how to manage tagged type "hierarchies"
    // If we fully implement reading data from schemas (which we might want to longer
    // term) then that might be interesting, but it's not always obvious since it is
    // not even strictly a tree structure.
    //
    // For present purposes just allow any of the currently hard-coded transforms (most
    // of which currently *are* projection types associated with FITS WCS)
    if (ASDF_VALUE_OK != err)
        goto failure;

    err = asdf_gwcs_transform_parse(prop, &fits->projection);

    if (ASDF_VALUE_OK != err)
        goto failure;

    *out = fits;
    return ASDF_VALUE_OK;
failure:
    asdf_ndarray_destroy(crpix);
    asdf_ndarray_destroy(crval);
    asdf_value_destroy(prop);
    asdf_gwcs_fits_destroy(fits);
    return err;
}


static void asdf_gwcs_fits_dealloc(void *value) {
    if (!value)
        return;

    asdf_gwcs_fits_t *fits = (asdf_gwcs_fits_t *)value;
    free(fits);
}


ASDF_REGISTER_EXTENSION(
    gwcs_fits,
    ASDF_GWCS_TAG_PREFIX "fitswcs_imaging-1.0.0",
    asdf_gwcs_fits_t,
    &libasdf_software,
    asdf_gwcs_fits_deserialize,
    asdf_gwcs_fits_dealloc,
    NULL);

