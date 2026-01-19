#include <assert.h>
#include <stdlib.h>

#include "../core/ndarray.h"
#include "../extension_util.h"
#include "../log.h"
#include "../util.h"

#include "gwcs.h"
#include "step.h"
#include "transform.h"


/** Helper to read crpix, crval, etc. */
static asdf_value_err_t get_coordinates_prop(asdf_mapping_t *value, const char *name, double *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_ndarray_t *ndarray = NULL;
    const void *data = NULL;
    size_t size = 0;

    assert(value);

    err = asdf_get_required_property(
        value, name, ASDF_VALUE_EXTENSION, ASDF_CORE_NDARRAY_TAG, (void *)&ndarray);

    if (ASDF_IS_ERR(err))
        goto failure;

    assert(ndarray);

    if (ndarray->ndim != 1 || ndarray->shape[0] != 2 ||
        ndarray->datatype.type != ASDF_DATATYPE_FLOAT64) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(&value->value);
        ASDF_LOG(value->value.file, ASDF_LOG_ERROR, "invalid %s array in %s", name, path);
#endif
        goto failure;
    }

    data = asdf_ndarray_data_raw(ndarray, &size);

    if (size != 2 * sizeof(double)) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(&value->value);
        ASDF_LOG(value->value.file, ASDF_LOG_ERROR, "invalid array data for %s in %s", name, path);
#endif
        goto failure;
    }

    memcpy(out, data, size);
    err = ASDF_VALUE_OK;
failure:
    asdf_ndarray_destroy(ndarray);
    return err;
}


/** Helper to read the pc matrix */
static asdf_value_err_t get_matrix_prop(asdf_mapping_t *value, const char *name, double *out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_ndarray_t *ndarray = NULL;
    const void *data = NULL;
    size_t size = 0;

    assert(value);

    err = asdf_get_required_property(
        value, name, ASDF_VALUE_EXTENSION, ASDF_CORE_NDARRAY_TAG, (void *)&ndarray);

    if (ASDF_IS_ERR(err))
        goto failure;

    if (ndarray->ndim != 2 || ndarray->shape[0] != 2 || ndarray->shape[1] != 2 ||
        ndarray->datatype.type != ASDF_DATATYPE_FLOAT64) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(&value->value);
        ASDF_LOG(value->value.file, ASDF_LOG_ERROR, "invalid %s array in %s", name, path);
#endif
        goto failure;
    }

    data = asdf_ndarray_data_raw(ndarray, &size);

    if (size != 2UL * 2UL * sizeof(double)) {
#ifdef ASDF_LOG_ENABLED
        const char *path = asdf_value_path(&value->value);
        ASDF_LOG(value->value.file, ASDF_LOG_ERROR, "invalid array data for %s in %s", name, path);
#endif
        goto failure;
    }

    memcpy(out, data, size);
    err = ASDF_VALUE_OK;
failure:
    asdf_ndarray_destroy(ndarray);
    return err;
}


static asdf_value_err_t asdf_gwcs_fits_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_gwcs_fits_t *fits = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_mapping_t *transform_map = NULL;
    asdf_value_t *projection = NULL;
    asdf_ndarray_t *crpix = NULL;
    asdf_ndarray_t *crval = NULL;

    if (asdf_value_as_mapping(value, &transform_map) != ASDF_VALUE_OK)
        goto cleanup;

    fits = calloc(1, sizeof(asdf_gwcs_fits_t));

    if (!fits) {
        err = ASDF_VALUE_ERR_OOM;
        goto cleanup;
    }

    err = asdf_gwcs_transform_parse(value, &fits->base);

    if (ASDF_IS_ERR(err))
        goto cleanup;

    err = get_coordinates_prop(transform_map, "crpix", (double *)fits->crpix);

    if (ASDF_IS_ERR(err))
        goto cleanup;

    err = get_coordinates_prop(transform_map, "crval", (double *)fits->crval);

    if (ASDF_IS_ERR(err))
        goto cleanup;

    err = get_coordinates_prop(transform_map, "cdelt", (double *)fits->cdelt);

    if (ASDF_IS_ERR(err))
        goto cleanup;

    err = get_matrix_prop(transform_map, "pc", (double *)fits->pc);

    if (ASDF_IS_ERR(err))
        goto cleanup;

    err = asdf_get_required_property(
        transform_map, "projection", ASDF_VALUE_UNKNOWN, NULL, &projection);
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
    if (ASDF_IS_ERR(err))
        goto cleanup;

    err = asdf_gwcs_transform_parse(projection, &fits->projection);

    if (ASDF_IS_ERR(err))
        goto cleanup;

    *out = fits;
    err = ASDF_VALUE_OK;
cleanup:
    asdf_ndarray_destroy(crpix);
    asdf_ndarray_destroy(crval);
    asdf_value_destroy(projection);

    if (err != ASDF_VALUE_OK)
        asdf_gwcs_fits_destroy(fits);

    return err;
}


static void asdf_gwcs_fits_dealloc(void *value) {
    if (!value)
        return;

    asdf_gwcs_fits_t *fits = (asdf_gwcs_fits_t *)value;
    asdf_gwcs_transform_clean(&fits->base);
    asdf_gwcs_transform_clean(&fits->projection);
    free((char *)fits->ctype[0]);
    free((char *)fits->ctype[1]);
    free(fits);
}


ASDF_REGISTER_EXTENSION(
    gwcs_fits,
    ASDF_GWCS_TAG_PREFIX "fitswcs_imaging-1.0.0",
    asdf_gwcs_fits_t,
    &libasdf_software,
    NULL,
    asdf_gwcs_fits_deserialize,
    asdf_gwcs_fits_dealloc,
    NULL);


/**
 * Maps a UCD1+ term to the corresponding coordinate type value in the FITS
 * WCS scheme.
 *
 * Returns `NULL` if no appropiate mapping is found.
 *
 * .. todo::
 *
 *   This is far from all-inclusive, and only includes some basic celestial
 *   coordinate types for now, no spectral, time, etc.; left as a later
 *   exercise.
 *
 * .. todo::
 *
 *   Maybe just make this an STC hash map...
 */

typedef struct {
    const char *ucd1;
    const char *ctype;
} ucd1_ctype_map_t;


static const ucd1_ctype_map_t ucd1_ctype_map[] = {
    {"pos.eq.ra", "RA"},
    {"pos.eq.dec", "DEC"},
    {"pos.galactic.lon", "GLON"},
    {"pos.galactic.lat", "GLAT"},
    {"pos.ecliptic.lon", "ELON"},
    {"pos.ecliptic.lat", "ELAT"},
    {"pos.bodyrc.lon", "TLON"},
    {"pos.bodyrc.lat", "TLAT"},
    {NULL, NULL}};


static const char *ucd1_to_ctype(const char *ucd1) {
    if (UNLIKELY(!ucd1))
        return NULL;

    for (int idx = 0; ucd1_ctype_map[idx].ucd1; idx++) {
        if (strcmp(ucd1_ctype_map[idx].ucd1, ucd1) == 0)
            return ucd1_ctype_map[idx].ctype;
    }
    return NULL;
}


static const char *transform_type_ctype_map[] = {
    [ASDF_GWCS_TRANSFORM_GENERIC] = "",
    [ASDF_GWCS_TRANSFORM_AIRY] = "AIR",
    [ASDF_GWCS_TRANSFORM_BONNE_EQUAL_AREA] = "BON",
    [ASDF_GWCS_TRANSFORM_COBE_QUAD_SPHERICAL_CUBE] = "CSC",
    [ASDF_GWCS_TRANSFORM_CONIC_EQUAL_AREA] = "COE",
    [ASDF_GWCS_TRANSFORM_CONIC_EQUIDISTANT] = "COD",
    [ASDF_GWCS_TRANSFORM_CONIC_ORTHOMORPHIC] = "COO",
    [ASDF_GWCS_TRANSFORM_CONIC_PERSPECTIVE] = "COP",
    [ASDF_GWCS_TRANSFORM_CYLINDRICAL_EQUAL_AREA] = "CEA",
    [ASDF_GWCS_TRANSFORM_CYLINDRICAL_PERSPECTIVE] = "CYP",
    [ASDF_GWCS_TRANSFORM_FITSWCS_IMAGING] = "",
    [ASDF_GWCS_TRANSFORM_GNOMONIC] = "TAN",
    [ASDF_GWCS_TRANSFORM_HAMMER_AITOFF] = "AIT",
    [ASDF_GWCS_TRANSFORM_HEALPIX_POLAR] = "XPH",
    [ASDF_GWCS_TRANSFORM_MOLLEWEIDE] = "MOL",
    [ASDF_GWCS_TRANSFORM_PARABOLIC] = "PAR",
    [ASDF_GWCS_TRANSFORM_PLATE_CARREE] = "CAR",
    [ASDF_GWCS_TRANSFORM_POLYCONIC] = "PCO",
    [ASDF_GWCS_TRANSFORM_SANSON_FLAMSTEED] = "SFL",
    [ASDF_GWCS_TRANSFORM_SLANT_ORTHOGRAPHIC] = "SIN",
    [ASDF_GWCS_TRANSFORM_STEREOGRAPHIC] = "STG",
    [ASDF_GWCS_TRANSFORM_QUAD_SPHERICAL_CUBE] = "QSC",
    [ASDF_GWCS_TRANSFORM_SLANT_ZENITHAL_PERSPECTIVE] = "SZP",
    [ASDF_GWCS_TRANSFORM_TANGENTIAL_SPHERICAL_CUBE] = "TSC",
    [ASDF_GWCS_TRANSFORM_ZENITHAL_EQUAL_AREA] = "ZEA",
    [ASDF_GWCS_TRANSFORM_ZENITHAL_EQUIDISTANT] = "ARC",
    [ASDF_GWCS_TRANSFORM_ZENITHAL_PERSPECTIVE] = "AZP"};


bool asdf_gwcs_is_fits(asdf_file_t *file, asdf_gwcs_t *gwcs) {
    if (gwcs->n_steps != 2) {
        ASDF_LOG(
            file,
            ASDF_LOG_DEBUG,
            "GWCS object for FITS WCS must have only two steps, got WCS with %d steps",
            gwcs->n_steps);
        return false;
    }

    const asdf_gwcs_step_t *step0 = &gwcs->steps[0];
    if (!step0->transform || step0->transform->type != ASDF_GWCS_TRANSFORM_FITSWCS_IMAGING) {
        ASDF_LOG(
            file,
            ASDF_LOG_DEBUG,
            "GWCS object for FITS WCS must have a fitswcs_imaging transform for its first step");
        return false;
    }

    const asdf_gwcs_step_t *step1 = &gwcs->steps[1];
    if (step1->frame->type != ASDF_GWCS_FRAME_CELESTIAL) {
        ASDF_LOG(
            file,
            ASDF_LOG_DEBUG,
            "currently only celestial coordinates in a ``gwcs/celestial_frame-*`` are supported");
        return false;
    }

    const asdf_gwcs_frame_celestial_t *celestial = (asdf_gwcs_frame_celestial_t *)step1->frame;

    for (int idx = 0; idx < 2; idx++) {
        if (!ucd1_to_ctype(celestial->axis_physical_types[idx])) {
            ASDF_LOG(
                file,
                ASDF_LOG_DEBUG,
                "missing or unsupported axis_physical_types in celestial frame: %s",
                celestial->axis_physical_types[idx]);
            return false;
        }
    }
    return true;
}


/**
 * Length of a CTYPEn FITS header
 */
#define CTYPE_SIZE 8


/**
 * Get the FITS WCS CTYPEn parameters from an `asdf_gwcs_t` object
 *
 * Doing this requires the full `asdf_gwcs_t` since for this to make sense
 * we need:
 *
 * - A GWCS with two steps, with a fitswcs_imaging transform between the two
 * - The projection type of the fitswcs_imaging transform
 * - The physical axis types of the output frame
 *
 * This does not handle every conceivable case yet, with focus just on
 * celestial coordinate frames for now.
 *
 * It outputs two values ``ctype1`` and ``ctype2`` since ``fitswcs_imaging``
 * currently only supports two-dimensional transforms.
 */
asdf_gwcs_err_t asdf_gwcs_fits_get_ctype(
    asdf_file_t *file, asdf_gwcs_t *gwcs, const char **ctype1, const char **ctype2) {

    assert(ctype1);
    assert(ctype2);

    if (!asdf_gwcs_is_fits(file, gwcs))
        return ASDF_GWCS_ERR_NOT_IMPLEMENTED;

    asdf_gwcs_err_t err = ASDF_GWCS_ERR_NOT_IMPLEMENTED;
    char *ctypes[2] = {0};

    // These should already be effectively asserted by asdf_gwcs_is_fits
    assert(gwcs->n_steps == 2);
    const asdf_gwcs_step_t *step0 = &gwcs->steps[0];
    const asdf_gwcs_step_t *step1 = &gwcs->steps[1];
    assert(step0->transform && step0->transform->type == ASDF_GWCS_TRANSFORM_FITSWCS_IMAGING);
    assert(step1->frame->type == ASDF_GWCS_FRAME_CELESTIAL);
    asdf_gwcs_fits_t *fits = (asdf_gwcs_fits_t *)step0->transform;
    asdf_gwcs_frame_celestial_t *celestial = (asdf_gwcs_frame_celestial_t *)step1->frame;

    for (int idx = 0; idx < 2; idx++) {
        char *ctype = malloc(CTYPE_SIZE + 1); // -------- with null

        if (!ctype) {
            err = ASDF_GWCS_ERR_OOM;
            goto failure;
        }

        memset(ctype, '-', CTYPE_SIZE);
        ctype[CTYPE_SIZE] = '\0';
        ctypes[idx] = ctype;

        const char *axis_type = ucd1_to_ctype(celestial->axis_physical_types[idx]);

        if (!axis_type)
            goto failure;

        size_t axis_type_len = strlen(axis_type);
        assert(axis_type_len <= CTYPE_SIZE);
        memcpy(ctype, axis_type, axis_type_len);

        asdf_gwcs_transform_type_t projection_type = fits->projection.type;
        assert(projection_type >= 0);
        assert(
            (unsigned int)projection_type <
            (sizeof(transform_type_ctype_map) / sizeof(const char *)));
        const char *proj_str = transform_type_ctype_map[projection_type];

        if (!proj_str)
            goto failure;

        size_t proj_str_len = strlen(proj_str);
        assert((axis_type_len + proj_str_len) <= CTYPE_SIZE);
        memcpy(ctype + (CTYPE_SIZE - proj_str_len), proj_str, proj_str_len);
    }

    *ctype1 = ctypes[0];
    *ctype2 = ctypes[1];
    return ASDF_GWCS_OK;
failure:
    free(ctypes[0]);
    free(ctypes[1]);
    return err;
}
