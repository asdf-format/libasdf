/* Data type an extension for http://stsci.edu/schemas/asdf/core/software-1.0.0 schema */
#ifndef ASDF_CORE_SOFTWARE_H
#define ASDF_CORE_SOFTWARE_H

#include <asdf/extension.h>


ASDF_BEGIN_DECLS

/* NOTE: asdf_software_t is defined in asdf/extension.h due to the circularity between them */
ASDF_DECLARE_EXTENSION(software, asdf_software_t);

ASDF_END_DECLS

#endif /* ASDF_CORE_SOFTWARE_H */
