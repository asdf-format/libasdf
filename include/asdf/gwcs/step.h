/**
 * Partial implementation of the gwcs/step-1.3.0 schema
 */

#ifndef ASDF_GWCS_STEP_H
#define ASDF_GWCS_STEP_H

#include <stdint.h>

#include <asdf/extension.h>
#include <asdf/util.h>

ASDF_BEGIN_DECLS

#ifndef ASDF_GWCS_INTERNAL
typedef struct _asdf_gwcs_step {
    // TODO
    const void *frame;
    const void *transform;
} asdf_gwcs_step_t;
#else
typedef struct _asdf_gwcs_step asdf_gwcs_step_t;
#endif

ASDF_DECLARE_EXTENSION(gwcs_step, asdf_gwcs_step_t);

ASDF_END_DECLS

#endif /* ASDF_GWCS_STEP_H */
