/**
 * Representation of the http://stsci.edu/schemas/gwcs/fitswcs_imaging-1.0.0
 * schema--a GWCS transform encapuslating a FITS WCS
 *
 * The C type representing the FITS WCS data is called just `asdf_gwcs_fits_t`
 * for short.
 *
 * If libasdf is configured with the ``--with-fits-wcs`` flag (which requires
 * WCSLIB) then it is also possible to convert this to the associated
 * `wcsprm`.  But even without WCSLIB it is possible to read these objects.
 */
#ifndef ASDF_GWCS_FITSWCS_IMAGING_H
#define ASDF_GWCS_FITSWCS_IMAGING_H

#include <asdf/gwcs/gwcs.h>
#include <asdf/gwcs/transform.h>

ASDF_BEGIN_DECLS

/**
 * Contains properties from an ``gwcs/fitswcs_imaging-1.0.0`` object
 */
typedef struct {
    asdf_gwcs_transform_t base;

    /** The FITS CRPIXn headers (0-indexed) */
    const double crpix[2];

    /** The FITS CRVALn headers (0-indexed) */
    const double crval[2];

    /** The FITS CDELTn headers (0-indexed) */
    const double cdelt[2];

    /** The fits PCij headers (0-index) */
    const double pc[2][2];

    asdf_gwcs_transform_t projection;
} asdf_gwcs_fits_t;

ASDF_DECLARE_EXTENSION(gwcs_fits, asdf_gwcs_fits_t);

ASDF_END_DECLS
#endif /* ASDF_GWCS_FITSWCS_IMAGING_H */
