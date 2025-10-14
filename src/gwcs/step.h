#pragma once

#include <stdbool.h>

#include "frame.h"


/** Internal definition of asdf_gwcs_step_t */
typedef struct _asdf_gwcs_step {
    asdf_gwcs_frame_t *frame;
    // TODO
    const void *transform;
    bool free;
} asdf_gwcs_step_t;
