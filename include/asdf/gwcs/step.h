/**
 * Partial implementation of the gwcs/step-1.3.0 schema
 */

#ifndef ASDF_GWCS_STEP_H
#define ASDF_GWCS_STEP_H

#include <stdint.h>

typedef struct {
    // TODO
    const void *frame;
    const void *transform;
} asdf_gwcs_step_t;


#endif /* ASDF_GWCS_STEP_H */
