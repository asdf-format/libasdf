/**
 * Partial implementation of the gwcs/frame2d-1.2.0 schema
 */

#ifndef ASDF_GWCS_FRAME2D_H
#define ASDF_GWCS_FRAME2D_H

#include <stdint.h>

#include <asdf/extension.h>
#include <asdf/gwcs/frame.h>
#include <asdf/util.h>

ASDF_BEGIN_DECLS

typedef struct {
    asdf_gwcs_frame_t base;
    const char *axes_names[2];
    uint8_t axes_order[2];
    // TODO: Should be an asdf_unit_t but right now that is just a string
    const char *unit[2];
} asdf_gwcs_frame2d_t;

ASDF_DECLARE_EXTENSION(gwcs_frame2d, asdf_gwcs_frame2d_t);

ASDF_END_DECLS

#endif /* ASDF_GWCS_FRAME2D_H */

