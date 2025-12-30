#include <stdatomic.h>

#include "../extension_util.h"

#include "gwcs.h"
#include "types/asdf_gwcs_transform_map.h"


static asdf_gwcs_transform_map_t global_transform_map = {0};
static atomic_bool global_transform_map_initialized = false;


static asdf_gwcs_transform_type_t asdf_gwcs_transform_type_get(const char *tagstr) {
    const asdf_gwcs_transform_map_value *item = asdf_gwcs_transform_map_get(
        &global_transform_map, tagstr);

    if (!item)
        return ASDF_GWCS_TRANSFORM_INVALID;

    return item->second;
}


asdf_value_err_t asdf_gwcs_transform_parse(asdf_value_t *value, asdf_gwcs_transform_t *transform) {

    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_mapping_t *transform_map = NULL;
    asdf_tag_t *parsed_tag = NULL;

    if (asdf_value_as_mapping(value, &transform_map) != ASDF_VALUE_OK)
        goto failure;

    const char *tag = asdf_value_tag(value);

    if (!tag) {
        // Not a transform as far as we know
        err = ASDF_VALUE_ERR_TYPE_MISMATCH;
        goto failure;
    }

    parsed_tag = asdf_tag_parse(tag);

    if (!parsed_tag) {
        err = ASDF_VALUE_ERR_TYPE_MISMATCH;
        goto failure;
    }

    asdf_gwcs_transform_type_t type = asdf_gwcs_transform_type_get(parsed_tag->name);

    if (ASDF_GWCS_TRANSFORM_INVALID == type) {
        err = ASDF_VALUE_ERR_TYPE_MISMATCH;
        goto failure;
    }

    transform->type = type;

    err = asdf_get_optional_property(
        transform_map, "name", ASDF_VALUE_STRING, NULL, (void *)&transform->name);

    if (!ASDF_IS_OPTIONAL_OK(err))
        goto failure;

    err = asdf_get_optional_property(
        transform_map,
        "bounding_box",
        ASDF_VALUE_EXTENSION,
        ASDF_GWCS_BOUNDING_BOX_TAG,
        (void *)&transform->bounding_box);

    if (!ASDF_IS_OPTIONAL_OK(err))
        goto failure;

    // TODO: Mostly not implemented yet.
    err = ASDF_VALUE_OK;

failure:
    asdf_tag_free(parsed_tag);
    return err;
}


void asdf_gwcs_transform_clean(asdf_gwcs_transform_t *transform) {
    if (!transform)
        return;

    asdf_gwcs_bounding_box_destroy((asdf_gwcs_bounding_box_t *)transform->bounding_box);
    ZERO_MEMORY(transform, sizeof(asdf_gwcs_transform_t));
}


void asdf_gwcs_transform_destroy(asdf_gwcs_transform_t *transform) {
    if (!transform)
        return;

    switch (transform->type) {
    case ASDF_GWCS_TRANSFORM_FITSWCS_IMAGING:
        asdf_gwcs_fits_destroy((asdf_gwcs_fits_t *)transform);
        return;
    default:
        break;
    }

    asdf_gwcs_transform_clean(transform);
    free(transform);
}


asdf_value_err_t asdf_value_as_gwcs_transform(asdf_value_t *value, asdf_gwcs_transform_t **out) {
    // TODO: This has the same problem as asdf_value_as_gwcs_frame; currently
    // we only support fitswcs_imaging transform so there is no problem, but
    // it will not scale so this needs to be totally rewritten later
    const char *tag_str = asdf_value_tag(value);
    asdf_tag_t *tag = asdf_tag_parse(tag_str);
    asdf_gwcs_transform_type_t type = asdf_gwcs_transform_type_get(tag->name);
    asdf_tag_free(tag);

    switch (type) {
    case ASDF_GWCS_TRANSFORM_INVALID:
        return ASDF_VALUE_ERR_TYPE_MISMATCH;
    case ASDF_GWCS_TRANSFORM_FITSWCS_IMAGING:
        return asdf_value_as_gwcs_fits(value, (asdf_gwcs_fits_t **)out);
    default:
        break;
    }

    // Generic case
    asdf_gwcs_transform_t *transform = calloc(1, sizeof(asdf_gwcs_transform_t));

    if (!transform)
        return ASDF_VALUE_ERR_OOM;

    asdf_value_err_t err = asdf_gwcs_transform_parse(value, transform);

    if (ASDF_IS_OK(err))
        *out = transform;
    else
        asdf_gwcs_transform_destroy(transform);

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
    if (atomic_load_explicit(&global_transform_map_initialized, memory_order_acquire))
        return;

    global_transform_map = c_make(
        asdf_gwcs_transform_map,
        {{ASDF_GWCS_TRANSFORM_TAG_PREFIX "transform", ASDF_GWCS_TRANSFORM_GENERIC},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "airy", ASDF_GWCS_TRANSFORM_AIRY},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "bonne_equal_area", ASDF_GWCS_TRANSFORM_BONNE_EQUAL_AREA},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "cobe_quad_spherical_cube",
          ASDF_GWCS_TRANSFORM_COBE_QUAD_SPHERICAL_CUBE},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "conic_equal_area", ASDF_GWCS_TRANSFORM_CONIC_EQUAL_AREA},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "conic_equidistant",
          ASDF_GWCS_TRANSFORM_CONIC_EQUIDISTANT},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "conic_orthomorphic",
          ASDF_GWCS_TRANSFORM_CONIC_ORTHOMORPHIC},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "conic_perspective",
          ASDF_GWCS_TRANSFORM_CONIC_PERSPECTIVE},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "cylindrical_equal_area",
          ASDF_GWCS_TRANSFORM_CYLINDRICAL_EQUAL_AREA},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "cylindrical_perspective",
          ASDF_GWCS_TRANSFORM_CYLINDRICAL_PERSPECTIVE},
         {ASDF_GWCS_TAG_PREFIX "fitswcs_imaging", ASDF_GWCS_TRANSFORM_FITSWCS_IMAGING},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "gnomonic", ASDF_GWCS_TRANSFORM_GNOMONIC},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "hammer_aitoff", ASDF_GWCS_TRANSFORM_HAMMER_AITOFF},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "healpix_polar", ASDF_GWCS_TRANSFORM_HEALPIX_POLAR},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "molleweide", ASDF_GWCS_TRANSFORM_MOLLEWEIDE},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "parabolic", ASDF_GWCS_TRANSFORM_PARABOLIC},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "plate_carree", ASDF_GWCS_TRANSFORM_PLATE_CARREE},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "polyconic", ASDF_GWCS_TRANSFORM_POLYCONIC},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "sanson_flamsteed", ASDF_GWCS_TRANSFORM_SANSON_FLAMSTEED},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "slant_orthographic",
          ASDF_GWCS_TRANSFORM_SLANT_ORTHOGRAPHIC},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "stereographic", ASDF_GWCS_TRANSFORM_STEREOGRAPHIC},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "quad_spherical_cube",
          ASDF_GWCS_TRANSFORM_QUAD_SPHERICAL_CUBE},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "slant_zenithal_perspective",
          ASDF_GWCS_TRANSFORM_SLANT_ZENITHAL_PERSPECTIVE},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "tangential_spherical_cube",
          ASDF_GWCS_TRANSFORM_TANGENTIAL_SPHERICAL_CUBE},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "zenithal_equal_area",
          ASDF_GWCS_TRANSFORM_ZENITHAL_EQUAL_AREA},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "zenithal_equidistant",
          ASDF_GWCS_TRANSFORM_ZENITHAL_EQUIDISTANT},
         {ASDF_GWCS_TRANSFORM_TAG_PREFIX "zenithal_perspective",
          ASDF_GWCS_TRANSFORM_ZENITHAL_PERSPECTIVE}});

    atomic_store_explicit(&global_transform_map_initialized, true, memory_order_release);
}


ASDF_DESTRUCTOR static void asdf_gwcs_transform_map_destroy(void) {
    if (atomic_load_explicit(&global_transform_map_initialized, memory_order_acquire)) {
        asdf_gwcs_transform_map_drop(&global_transform_map);
        atomic_store_explicit(&global_transform_map_initialized, false, memory_order_release);
    }
}
