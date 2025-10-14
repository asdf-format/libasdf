/**
 * 
 */
#ifndef ASDF_GWCS_GWCS_H
#define ASDF_GWCS_GWCS_H

#include <asdf/gwcs/frame_celestial.h>
#include <asdf/gwcs/frame2d.h>
#include <asdf/gwcs/step.h>
#include <asdf/gwcs/wcs.h>
#include <asdf/util.h>

ASDF_BEGIN_DECLS

/**
 * Prefix for all GWCS schema tags
 */
#define ASDF_GWCS_TAG_PREFIX "tag:stsci.edu:gwcs/"

/* Error codes for reading gwcs data */
typedef enum {
    ASDF_GWCS_OK = 0,
    ASDF_GWCS_ERR_OOM,
    ASDF_GWCS_ERR_NOT_IMPLEMENTED,
} asdf_gwcs_err_t;


ASDF_END_DECLS
#endif /* ASDF_GWCS_GWCS_H */
