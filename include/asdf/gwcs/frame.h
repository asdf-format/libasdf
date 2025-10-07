/**
 * Partial implementation of the gwcs/frame-1.2.0 schema
 */

#ifndef ASDF_GWCS_FRAME_H
#define ASDF_GWCS_FRAME_H

#include <stdint.h>

#include <asdf/extension.h>
#include <asdf/util.h>

ASDF_BEGIN_DECLS


/**
 * Enum for tagging which type of frame a give `asdf_frame_t *` contains
 */
typedef enum {
    ASDF_GWCS_FRAME_GENERIC,
    ASDF_GWCS_FRAME_2D
} asdf_gwcs_frame_type_t;

typedef struct {
    asdf_gwcs_frame_type_t type;
    const char *name;
    // TODO
} asdf_gwcs_frame_t;

ASDF_DECLARE_EXTENSION(gwcs_frame, asdf_gwcs_frame_t);

// TODO
ASDF_DECLARE_EXTENSION(gwcs_frame_celestial, asdf_gwcs_frame_t);

ASDF_END_DECLS

#endif /* ASDF_GWCS_FRAME_H */
