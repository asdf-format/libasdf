#include <asdf/core/asdf.h>
#include <asdf/core/extension_metadata.h>
#include <asdf/core/history_entry.h>
#include <asdf/core/software.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../error.h"
#include "../log.h"
#include "../util.h"
#include "../value.h"


asdf_software_t libasdf_software = {
    .name = PACKAGE_NAME,
    .version = PACKAGE_VERSION,
    .homepage = PACKAGE_URL,
    .author = "The libasdf Developers"};


// TODO: Seems useful to package this as a macro; a common helper to build an array of some
// extension values...
static asdf_extension_metadata_t **asdf_meta_extensions_deserialize(asdf_value_t *value) {
    if (UNLIKELY(!asdf_value_is_sequence(value)))
        goto failure;


    int extensions_size = asdf_sequence_size(value);

    if (UNLIKELY(extensions_size < 0))
        goto failure;

    asdf_extension_metadata_t **extensions = calloc(
        extensions_size + 1, sizeof(asdf_extension_metadata_t *));

    if (!extensions) {
        ASDF_ERROR_OOM(value->file);
        goto failure;
    }

    asdf_extension_metadata_t **extension_p = extensions;
    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *it = NULL;
    while ((it = asdf_sequence_iter(value, &iter))) {
        if (ASDF_VALUE_OK == asdf_value_as_extension_metadata(it, extension_p))
            extension_p++;
        else
            ASDF_LOG(value->file, ASDF_LOG_WARN, "ignoring invalid extension_metadata");
    }

    return extensions;

failure:
    ASDF_LOG(value->file, ASDF_LOG_WARN, "ignoring invalid extensions metadata");
    return NULL;
}


static asdf_history_entry_t **asdf_meta_history_entries_deserialize(asdf_value_t *value) {
    if (UNLIKELY(!asdf_value_is_sequence(value)))
        goto failure;


    int history_size = asdf_sequence_size(value);

    if (UNLIKELY(history_size < 0))
        goto failure;

    asdf_history_entry_t **entries = calloc(history_size + 1, sizeof(asdf_history_entry_t *));

    if (!entries) {
        ASDF_ERROR_OOM(value->file);
        goto failure;
    }

    asdf_history_entry_t **entry_p = entries;
    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *it = NULL;
    while ((it = asdf_sequence_iter(value, &iter))) {
        if (ASDF_VALUE_OK == asdf_value_as_history_entry(it, entry_p))
            entry_p++;
        else
            ASDF_LOG(value->file, ASDF_LOG_WARN, "ignoring invalid history_entry");
    }

    return entries;

failure:
    ASDF_LOG(value->file, ASDF_LOG_WARN, "ignoring invalid history entries");
    return NULL;
}


static asdf_meta_history_t asdf_meta_history_deserialize(asdf_value_t *value) {
    asdf_meta_history_t history = {0};
    asdf_value_t *history_sequence = NULL;
    asdf_value_t *extension_sequence = NULL;
    asdf_extension_metadata_t **extensions = NULL;
    asdf_history_entry_t **history_entries = NULL;

    if (asdf_value_is_sequence(value)) {
        // Old-style history
        history_sequence = value;
    } else if (asdf_value_is_mapping(value)) {
        extension_sequence = asdf_mapping_get(value, "extensions");
        history_sequence = asdf_mapping_get(value, "entries");
    } else {
        ASDF_LOG(value->file, ASDF_LOG_WARN, "ignoring invalid \"history\" property");
    }

    if (extension_sequence)
        extensions = asdf_meta_extensions_deserialize(extension_sequence);

    if (history_sequence)
        history_entries = asdf_meta_history_entries_deserialize(history_sequence);

    history.extensions = extensions;
    history.entries = history_entries;
    asdf_value_destroy(history_sequence);
    asdf_value_destroy(extension_sequence);
    return history;
}


static asdf_value_err_t asdf_meta_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;
    asdf_software_t *asdf_library = NULL;
    asdf_meta_history_t history = {0};

    if (!asdf_value_is_mapping(value))
        goto failure;

    if ((prop = asdf_mapping_get(value, "asdf_library"))) {
        if (ASDF_VALUE_OK != asdf_value_as_software(prop, &asdf_library))
            ASDF_LOG(value->file, ASDF_LOG_WARN, "ignoring invalid asdf_library software");
    }

    asdf_value_destroy(prop);

    if ((prop = asdf_mapping_get(value, "history"))) {
        history = asdf_meta_history_deserialize(prop);
    }


    asdf_value_destroy(prop);

    asdf_meta_t *meta = calloc(1, sizeof(asdf_meta_t));

    if (!meta)
        return ASDF_VALUE_ERR_OOM;

    meta->asdf_library = asdf_library;
    meta->history = history;
    *out = meta;
    return ASDF_VALUE_OK;
failure:
    asdf_value_destroy(prop);
    return err;
}


static void asdf_meta_dealloc(void *value) {
    if (!value)
        return;

    asdf_meta_t *meta = value;

    if (meta->asdf_library)
        asdf_software_destroy(meta->asdf_library);

    if (meta->history.extensions) {
        for (asdf_extension_metadata_t **ep = meta->history.extensions; *ep; ++ep) {
            asdf_extension_metadata_destroy(*ep);
        }
        free(meta->history.extensions);
    }

    if (meta->history.entries) {
        for (asdf_history_entry_t **ep = meta->history.entries; *ep; ++ep) {
            asdf_history_entry_destroy(*ep);
        }
        free(meta->history.entries);
    }

    free(meta);
}


/* Define the extension for the core/asdf schema
 *
 * The internal types and methods are named ``asdf_meta_*``, however, to avoid names like
 * ``asdf_asdf_t`` and ``asdf_get_asdf`` and so on.
 */
ASDF_REGISTER_EXTENSION(
    meta,
    ASDF_CORE_TAG_PREFIX "asdf-1.1.0",
    asdf_meta_t,
    &libasdf_software,
    asdf_meta_deserialize,
    asdf_meta_dealloc,
    NULL);
