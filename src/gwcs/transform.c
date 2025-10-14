#include <stdatomic.h>

#define ASDF_GWCS_INTERNAL
#include <asdf/gwcs/transform.h>
#include <asdf/gwcs/transforms/property/bounding_box.h>
#undef ASDF_GWCS_INTERNAL
#include <asdf/util.h>
#include <asdf/value.h>

#include "transform.h"
#include "types/asdf_gwcs_transform_map.h"
#include "../extension_util.h"


static asdf_gwcs_transform_map_t transform_map = {0};
static atomic_bool transform_map_initialized = false;


static asdf_gwcs_transform_type_t asdf_gwcs_transform_type_get(const char *tagstr) {
    const asdf_gwcs_transform_map_value *item = asdf_gwcs_transform_map_get(&transform_map, tagstr);

    if (!item)
        return ASDF_GWCS_TRANSFORM_INVALID;

    return item->second;
}


asdf_value_err_t asdf_gwcs_transform_parse(
    asdf_value_t *value, asdf_gwcs_transform_t *transform) {

    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;

    if (!asdf_value_is_mapping(value))
        goto failure;

    const char *tag = asdf_value_tag(value);

    if (!tag) {
        // Not a transform as far as we know
        err = ASDF_VALUE_ERR_TYPE_MISMATCH;
        goto failure;
    }

    asdf_gwcs_transform_type_t type = asdf_gwcs_transform_type_get(tag);

    if (ASDF_GWCS_TRANSFORM_INVALID == type) {
        err = ASDF_VALUE_ERR_TYPE_MISMATCH;
        goto failure;
    }

    transform->type = type;

    // TODO: Still need a better way to handle optional properties, specifically the
    // case of if not found, do nothing, if any other err: failure
    err = asdf_get_optional_property(value, "name", ASDF_VALUE_STRING, NULL, &prop);

    if (ASDF_VALUE_OK == err) {
        err = asdf_value_as_string0(prop, &transform->name);

        if (ASDF_VALUE_OK != err)
            goto failure;
    } else if (ASDF_VALUE_ERR_NOT_FOUND != err) {
        goto failure;
    }

    asdf_value_destroy(prop);

    // TODO: Fix this, but asdf_get_optional_property doesn't work correctly yet with
    // extension types....
    // err = asdf_get_optional_property(value, "bounding_box", ASDF_VALUE_EXTENSION,
    //                                  ASDF_GWCS_BOUNDING_BOX_TAG, &prop);
    err = asdf_get_optional_property(value, "bounding_box", ASDF_VALUE_UNKNOWN, NULL, &prop);

    if (err != ASDF_VALUE_OK && err != ASDF_VALUE_ERR_NOT_FOUND)
        goto failure;

    if (ASDF_VALUE_OK == err) {
        err = asdf_value_as_gwcs_bounding_box(
            prop, (asdf_gwcs_bounding_box_t **)&transform->bounding_box);

        if (ASDF_VALUE_OK != err)
            goto failure;
    } else if (ASDF_VALUE_ERR_NOT_FOUND != err) {
        goto failure;
    }

    // TODO: Mostly not implemented yet.
    return ASDF_VALUE_OK;

failure:
    asdf_value_destroy(prop);
    return err;
}


/**
 * Hard-coded mapping between known transform tags and their associated
 * `asdf_gwcs_transform_type_t` value
 *
 * .. todo::
 *
 *   Later we will want to have routines for extending this map
 *   programmatically, e.g. by extensions for the GWCS plugin.  Also will still need to
 *   better support different schema versions.
 */
ASDF_CONSTRUCTOR static void asdf_gwcs_transform_map_create() {
    if (atomic_load_explicit(&transform_map_initialized, memory_order_acquire))
        return;

    transform_map = c_make(
        asdf_gwcs_transform_map, {
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "transform-1.4.0",               ASDF_GWCS_TRANSFORM_GENERIC},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "airy-1.4.0",                    ASDF_GWCS_TRANSFORM_AIRY},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "bonne_equal_area-1.5.0",        ASDF_GWCS_TRANSFORM_BONNE_EQUAL_AREA},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "cobe_quad_spherical_cube-1.4.0",ASDF_GWCS_TRANSFORM_COBE_QUAD_SPHERICAL_CUBE},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "conic_equal_area-1.4.0",        ASDF_GWCS_TRANSFORM_CONIC_EQUAL_AREA},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "conic_equidistant-1.4.0",       ASDF_GWCS_TRANSFORM_CONIC_EQUIDISTANT},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "conic_orthomorphic-1.5.0",      ASDF_GWCS_TRANSFORM_CONIC_ORTHOMORPHIC},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "conic_perspective-1.5.0",       ASDF_GWCS_TRANSFORM_CONIC_PERSPECTIVE},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "cylindrical_equal_area-1.5.0",  ASDF_GWCS_TRANSFORM_CYLINDRICAL_EQUAL_AREA},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "cylindrical_perspective-1.5.0", ASDF_GWCS_TRANSFORM_CYLINDRICAL_PERSPECTIVE},
        {ASDF_GWCS_TAG_PREFIX "fitswcs_imaging-1.0.0",         ASDF_GWCS_TRANSFORM_FITSWCS_IMAGING},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "gnomonic-1.3.0",                ASDF_GWCS_TRANSFORM_GNOMONIC},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "hammer_aitoff-1.4.0",           ASDF_GWCS_TRANSFORM_HAMMER_AITOFF},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "healpix_polar-1.4.0",           ASDF_GWCS_TRANSFORM_HEALPIX_POLAR},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "molleweide-1.4.0",              ASDF_GWCS_TRANSFORM_MOLLEWEIDE},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "parabolic-1.4.0",               ASDF_GWCS_TRANSFORM_PARABOLIC},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "plate_carree-1.4.0",            ASDF_GWCS_TRANSFORM_PLATE_CARREE},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "polyconic-1.4.0",               ASDF_GWCS_TRANSFORM_POLYCONIC},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "sanson_flamsteed-1.4.0",        ASDF_GWCS_TRANSFORM_SANSON_FLAMSTEED},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "slant_orthographic-1.4.0",      ASDF_GWCS_TRANSFORM_SLANT_ORTHOGRAPHIC},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "stereographic-1.4.0",           ASDF_GWCS_TRANSFORM_STEREOGRAPHIC},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "quad_spherical_cube-1.4.0",     ASDF_GWCS_TRANSFORM_QUAD_SPHERICAL_CUBE},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "slant_zenithal_perspective-1.4.0", ASDF_GWCS_TRANSFORM_SLANT_ZENITHAL_PERSPECTIVE},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "tangential_spherical_cube-1.4.0",  ASDF_GWCS_TRANSFORM_TANGENTIAL_SPHERICAL_CUBE},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "zenithal_equal_area-1.4.0",     ASDF_GWCS_TRANSFORM_ZENITHAL_EQUAL_AREA},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "zenithal_equidistant-1.4.0",    ASDF_GWCS_TRANSFORM_ZENITHAL_EQUIDISTANT},
        {ASDF_GWCS_TRANSFORM_TAG_PREFIX "zenithal_perspective-1.5.0",    ASDF_GWCS_TRANSFORM_ZENITHAL_PERSPECTIVE}
    });

    atomic_store_explicit(&transform_map_initialized, true, memory_order_release);
}


ASDF_DESTRUCTOR static void asdf_gwcs_transform_map_destroy(void) {
    if (atomic_load_explicit(&transform_map_initialized, memory_order_acquire)) {
        asdf_gwcs_transform_map_drop(&transform_map);
        atomic_store_explicit(&transform_map_initialized, false, memory_order_release);
    }
}
