/**
 * Internal routines for fitswcs_imaging
 */

#include "../file.h"
#include "../util.h"

#include "gwcs.h"


ASDF_LOCAL asdf_gwcs_err_t asdf_gwcs_fits_get_ctype(
    asdf_file_t *file, asdf_gwcs_t *gwcs, const char **ctype1, const char **ctype2);
