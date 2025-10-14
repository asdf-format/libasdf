/**
 * Representation of the http://stsci.edu/schemas/asdf/transform/transform-1.4.0
 * base schema for all GWCS transforms.
 *
 * .. todo::
 *
 *   This is not fully implemented yet and is included right now with future
 *   ABI compatibility in mind.
 *
 *   Specific transforms should use this as a base member.
 */

#ifndef ASDF_GWCS_TRANSFORM_H
#define ASDF_GWCS_TRANSFORM_H

#include <asdf/core/asdf.h>
#include <asdf/gwcs/gwcs.h>
#include <asdf/gwcs/transforms/property/bounding_box.h>

ASDF_BEGIN_DECLS


#define ASDF_GWCS_TRANSFORM_TAG_PREFIX ASDF_STANDARD_TAG_PREFIX "transform/"

// Forward-declaration
typedef struct _asdf_gwcs_transform asdf_gwcs_transform_t;


/** Enum used to tag the transform type represented by a generic
 * `asdf_gwcs_transform_t`
 *
 * .. todo::
 *
 *   Does not yet enumerate all the known transform types, only those
 *   allowed for `asdf_gwcs_fits_t`. Will eventally have others, and
 *   maybe custom types, but a full implementation of GWCS is well
 *   out-of-scope for now.
 */
typedef enum {
    ASDF_GWCS_TRANSFORM_INVALID = -1,
    ASDF_GWCS_TRANSFORM_GENERIC,
    ASDF_GWCS_TRANSFORM_AIRY,
    ASDF_GWCS_TRANSFORM_BONNE_EQUAL_AREA,
    ASDF_GWCS_TRANSFORM_COBE_QUAD_SPHERICAL_CUBE,
    ASDF_GWCS_TRANSFORM_CONIC_EQUAL_AREA,
    ASDF_GWCS_TRANSFORM_CONIC_EQUIDISTANT,
    ASDF_GWCS_TRANSFORM_CONIC_ORTHOMORPHIC,
    ASDF_GWCS_TRANSFORM_CONIC_PERSPECTIVE,
    ASDF_GWCS_TRANSFORM_CYLINDRICAL_EQUAL_AREA,
    ASDF_GWCS_TRANSFORM_CYLINDRICAL_PERSPECTIVE,
    ASDF_GWCS_TRANSFORM_FITSWCS_IMAGING,
    ASDF_GWCS_TRANSFORM_GNOMONIC,
    ASDF_GWCS_TRANSFORM_HAMMER_AITOFF,
    ASDF_GWCS_TRANSFORM_HEALPIX_POLAR,
    ASDF_GWCS_TRANSFORM_MOLLEWEIDE,
    ASDF_GWCS_TRANSFORM_PARABOLIC,
    ASDF_GWCS_TRANSFORM_PLATE_CARREE,
    ASDF_GWCS_TRANSFORM_POLYCONIC,
    ASDF_GWCS_TRANSFORM_SANSON_FLAMSTEED,
    ASDF_GWCS_TRANSFORM_SLANT_ORTHOGRAPHIC,
    ASDF_GWCS_TRANSFORM_STEREOGRAPHIC,
    ASDF_GWCS_TRANSFORM_QUAD_SPHERICAL_CUBE,
    ASDF_GWCS_TRANSFORM_SLANT_ZENITHAL_PERSPECTIVE,
    ASDF_GWCS_TRANSFORM_TANGENTIAL_SPHERICAL_CUBE,
    ASDF_GWCS_TRANSFORM_ZENITHAL_EQUAL_AREA,
    ASDF_GWCS_TRANSFORM_ZENITHAL_EQUIDISTANT,
    ASDF_GWCS_TRANSFORM_ZENITHAL_PERSPECTIVE,
} asdf_gwcs_transform_type_t;


typedef struct _asdf_gwcs_transform {
    /**
     * Enum value specifying the type of transform
     */
    asdf_gwcs_transform_type_t type;

    /**
     * A human-readable name for the transform (may be `NULL`)
     *
     * This an other fields up through `input_units_equivalencies` are from
     * the base `stsci.edu/transform/transform-1.4.0 schema` but are not
     * fully implemented (many of them are not used or needed for FITS WCS
     * and will likely be moved later into a base transform field, but they
     * are laid out here with future ABI compatibility in mind...)
     * */
    const char *name;

    /**
     * Not yet implemented, but would be an inverse transform
     * (currently always `NULL`)
     * */
    const asdf_gwcs_transform_t *inverse;

    /** NULL-terminated array of names of input variables */
    const char **inputs;

    /** NULL-terminated array of names of output variables */
    const char **outputs;

    /** Bounding box of the model as `asdf_gwcs_bounding_box_t` */
    const asdf_gwcs_bounding_box_t *bounding_box;

    /** Fixed properties (not yet implemented) */
    const void *fixed;

    /** Bounded parameters (not yet implemented) */
    const void *bounds;

    /** Input unit equivalences (not yet implemented) */
    const void *input_units_equivalencies;
} asdf_gwcs_transform_t;


ASDF_END_DECLS

#endif /* ASDF_GWCS_TRANSFORM_H */
