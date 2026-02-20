/* Data type an extension for http://stsci.edu/schemas/asdf/core/software-1.0.0 schema */
#ifndef ASDF_CORE_SOFTWARE_H
#define ASDF_CORE_SOFTWARE_H

#include <asdf/extension.h>
#include <asdf/file.h>


ASDF_BEGIN_DECLS

/* NOTE: asdf_software_t is defined in asdf/extension.h due to the circularity between them */
ASDF_DECLARE_EXTENSION(software, asdf_software_t);


/** Additional software-related methods */

/**
 * Overrite the ``asdf_library`` in the ASDF file metadata when writing the file
 *
 * Used primarily for testing/debugging, or lying ;)
 *
 * :param file: The `asdf_file_t *` handle
 * :param software: The `asdf_software_t *` to set as ``asdf_library``; this
 *   is *copied* and stored on the `asdf_file_t *` and the copy is deallocated
 *   the file is closed
 */
ASDF_EXPORT void asdf_library_set(asdf_file_t *file, const asdf_software_t *software);


/**
 * Like `asdf_library_set` but only overrides the software version
 *
 * :param file: The `asdf_file_t *` handle
 * :param software: Version string to set on the ``asdf_library`` metadata
 */
ASDF_EXPORT void asdf_library_set_version(asdf_file_t *file, const char *version);


ASDF_END_DECLS

#endif /* ASDF_CORE_SOFTWARE_H */
