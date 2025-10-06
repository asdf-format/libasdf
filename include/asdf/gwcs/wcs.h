/**
 * Partial implementation of the gwcs/wcs-1.4.0 schema
 */
#ifndef ASDF_GWCS_WCS_H
#define ASDF_GWCS_WCS_H

#include <stdint.h>

#include <asdf/gwcs/step.h>


/**
 * Represents an instance of the ``gwcs/wcs-1.4.0`` schema
 *
 * .. note::
 *
 *   Most GWCS objects have corresponding structures starting with
 *   ``asdf_gwcs_``; e.g. `asdf_gwcs_step_t` to avoid ambiguity.  However this
 *   top-level object is named just ``asdf_gwcs_t`` as there is little
 *   ambiguity and avoids the repetitious "asdf_gwcs_wcs_t".
 */
typedef struct {
    const char *name;
    uint32_t pixel_ndim;
    const uint64_t *pixel_shape;
    uint32_t n_steps;
    const asdf_gwcs_step_t *steps;
} asdf_gwcs_t;


#endif /* ASDF_GWCS_WCS_H */
