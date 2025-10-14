/**
 * Partial implementation of the gwcs/frame_celestial-1.2.0 schema
 */

#ifndef ASDF_GWCS_FRAME_CELESTIAL_H
#define ASDF_GWCS_FRAME_CELESTIAL_H

#include <stdint.h>

#include <asdf/extension.h>
#include <asdf/gwcs/frame.h>
#include <asdf/util.h>

ASDF_BEGIN_DECLS

typedef struct {
    asdf_gwcs_frame_t base;
    const char *axes_names[3];
    uint32_t axes_order[3];
    const char *unit[3];
} asdf_gwcs_frame_celestial_t;

ASDF_DECLARE_EXTENSION(gwcs_frame_celestial, asdf_gwcs_frame_celestial_t);

ASDF_END_DECLS

#endif /* ASDF_GWCS_FRAME_CELESTIAL_H */

