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
    ASDF_GWCS_FRAME_2D,
    ASDF_GWCS_FRAME_CELESTIAL,
} asdf_gwcs_frame_type_t;

typedef struct {
    asdf_gwcs_frame_type_t type;
    const char *name;
} asdf_gwcs_base_frame_t;

typedef asdf_gwcs_base_frame_t asdf_gwcs_frame_t;

ASDF_DECLARE_EXTENSION(gwcs_base_frame, asdf_gwcs_base_frame_t);

ASDF_EXPORT asdf_value_err_t asdf_value_as_gwcs_frame(asdf_value_t *value, asdf_gwcs_frame_t **out);
ASDF_EXPORT void asdf_gwcs_frame_destroy(asdf_gwcs_frame_t *frame);

ASDF_END_DECLS

#endif /* ASDF_GWCS_FRAME_H */
