/* Data type an extension for http://stsci.edu/schemas/asdf/core/history_entry-1.0.0 schema */
#ifndef ASDF_CORE_HISTORY_ENTRY_H
#define ASDF_CORE_HISTORY_ENTRY_H

#include <time.h>

#include <asdf/extension.h>
#include <asdf/core/software.h>


ASDF_BEGIN_DECLS

typedef struct {
    const char *description;
    struct timespec time;
    const asdf_software_t **software;
} asdf_history_entry_t;


ASDF_DECLARE_EXTENSION(history_entry, asdf_history_entry_t);

ASDF_END_DECLS

#endif /* ASDF_CORE_HISTORY_ENTRY_H */
