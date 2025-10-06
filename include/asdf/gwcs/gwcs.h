/**
 * 
 */
#ifndef ASDF_GWCS_GWCS_H
#define ASDF_GWCS_GWCS_H

#include <asdf/gwcs/wcs.h>


/**
 * Prefix for all GWCS schema tags
 */
#define ASDF_GWCS_TAG_PREFIX "stsci.edu:gwcs/"

/* Error codes for reading gwcs data */
typedef enum {
    ASDF_GWCS_OK = 0,
    ASDF_GWCS_ERR_OOM,
    ASDF_GWCS_ERR_NOT_IMPLEMENTED,
} asdf_gwcs_err_t;


#endif /* ASDF_GWCS_GWCS_H */
