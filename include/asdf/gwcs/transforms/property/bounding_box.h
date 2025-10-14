/**
 * Partial implementation of the transforms/property/bounding_box-1.2.0 schema
 */
#ifndef ASDF_GWCS_TRANSFORMS_PROPERTY_BOUNDING_BOX_H
#define ASDF_GWCS_TRANSFORMS_PROPERTY_BOUNDING_BOX_H

#include <stdint.h>

#include <asdf/extension.h>
#include <asdf/util.h>

ASDF_BEGIN_DECLS

// TODO: The latest version is actually 1.3.0 but the example file I have uses bounding_box-1.1.0
// Need to implement tag version management :(
#define ASDF_GWCS_BOUNDING_BOX_TAG ASDF_GWCS_TRANSFORM_TAG_PREFIX "property/bounding_box-1.1.0"
/**
 * Enum values for array storage order
 *
 * This probably belongs in a different header but the bounding_box is the first schema
 * where I've seen it used so it is declared here for now.
 */
typedef enum {
    ASDF_ARRAY_STORAGE_ORDER_C = 'C',
    ASDF_ARRAY_STORAGE_ORDER_F = 'F'
} asdf_array_storage_order_t;


/** Pairs an axis name with an interval
 *
 * .. todo::
 *
 *   Currently only numeric intervals are supported, not quantities.
 */
typedef struct {
    const char *input_name;
    double bounds[2];
} asdf_gwcs_interval_t;


/* Error codes for reading gwcs data */
typedef struct {
    uint32_t n_intervals;
    const asdf_gwcs_interval_t *intervals;
    /** Null-terminated (or NULL pointer when missing) array of ignored inputs */
    const char **ignore;
    /** Input ordering (C or FORTRAN) */
    asdf_array_storage_order_t order;
} asdf_gwcs_bounding_box_t;


ASDF_DECLARE_EXTENSION(gwcs_bounding_box, asdf_gwcs_bounding_box_t);


ASDF_END_DECLS
#endif /* ASDF_GWCS_TRANSFORMS_PROPERTY_BOUNDING_BOX_H */
