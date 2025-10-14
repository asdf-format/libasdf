#ifndef ASDF_CORE_ASDF_H
#define ASDF_CORE_ASDF_H

#include <asdf/core/extension_metadata.h>
#include <asdf/core/history_entry.h>
#include <asdf/core/software.h>
#include <asdf/extension.h>
#include <asdf/util.h>


ASDF_BEGIN_DECLS

ASDF_EXPORT extern asdf_software_t libasdf_software;


#define ASDF_STANDARD_TAG_PREFIX "tag:stsci.edu:asdf/"
#define ASDF_CORE_TAG_PREFIX ASDF_STANDARD_TAG_PREFIX "core/"


/*
 * The asdf_meta_history_t object representing the "history" property of the ``core/asdf-1.1.0``
 * schema
 *
 * The ``core/asdf-1.1.0`` schema has two internal definitions for ``"history"`` though they
 * aren't distinguished with explicit tags.  This captures the structure of the ``history-1.1.0``
 * format which also easily encapsulates the old format (which was just an sequence of
 * ``history_entry-1.0.0``)
 */
typedef struct {
    asdf_extension_metadata_t **extensions;
    asdf_history_entry_t **entries;
} asdf_meta_history_t;


typedef struct {
    asdf_software_t *asdf_library;
    asdf_meta_history_t history;
} asdf_meta_t;


ASDF_DECLARE_EXTENSION(meta, asdf_meta_t);

ASDF_END_DECLS

#endif /* ASDF_CORE_ASDF_H */
