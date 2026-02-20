/* Data type an extension for http://stsci.edu/schemas/asdf/core/history_entry-1.0.0 schema */
#ifndef ASDF_CORE_HISTORY_ENTRY_H
#define ASDF_CORE_HISTORY_ENTRY_H

#include <time.h>

#include <asdf/core/software.h>
#include <asdf/extension.h>
#include <asdf/file.h>


ASDF_BEGIN_DECLS

typedef struct {
    const char *description;
    struct timespec time;
    const asdf_software_t **software;
} asdf_history_entry_t;


ASDF_DECLARE_EXTENSION(history_entry, asdf_history_entry_t);

/**
 * Add a history entry to the file
 *
 * .. todo::
 *
 *   Allow passing a timestamp for the history entry as well, or an extended
 *   version that accepts a timestamp.
 *
 * :params file: Open `asdf_file_t *` handle
 * :params description: The text to add to the history entry
 * :return: Non-zero if adding the history entry failed
 */
ASDF_EXPORT int asdf_history_entry_add(asdf_file_t *file, const char *description);

ASDF_END_DECLS

#endif /* ASDF_CORE_HISTORY_ENTRY_H */
