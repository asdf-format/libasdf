/** Common internal frame routines */

#include <stdint.h>

#include <asdf/gwcs/frame.h>
#include <asdf/util.h>

#include "../value.h"


/**
 * Convenience struct for passing around common frame parameters for frame parsing
 */
typedef struct {
    uint32_t max_axes;
    uint32_t min_axes;
    char **axes_names;
    uint32_t *axes_order;
    char **unit;
    // TODO:
    // - reference_frame
    // - axes_physical_type
} asdf_gwcs_frame_common_params_t;



/**
 * Internal helper for parsing different frame types
 */
ASDF_LOCAL asdf_value_err_t asdf_gwcs_frame_parse(asdf_value_t *value, asdf_gwcs_frame_t *frame,
                                                  asdf_gwcs_frame_common_params_t *params);
