#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "../error.h"
#include "../log.h"
#include "../util.h"
#include "../value.h"

#include "asdf.h"
#include "extension_metadata.h"
#include "history_entry.h"
#include "software.h"


asdf_software_t libasdf_software = {
    .name = PACKAGE_NAME,
    .version = PACKAGE_VERSION,
    .homepage = PACKAGE_URL,
    .author = "The libasdf Developers"};


// TODO: Seems useful to package this as a macro; a common helper to build an array of some
// extension values...
static asdf_extension_metadata_t **asdf_meta_extensions_deserialize(asdf_value_t *value) {
    asdf_sequence_t *extensions_seq = NULL;

    if (UNLIKELY(asdf_value_as_sequence(value, &extensions_seq) != ASDF_VALUE_OK))
        goto failure;

    int extensions_size = asdf_sequence_size(extensions_seq);

    if (UNLIKELY(extensions_size < 0))
        goto failure;

    asdf_extension_metadata_t **extensions = (asdf_extension_metadata_t **)calloc(
        extensions_size + 1, sizeof(asdf_extension_metadata_t *));

    if (!extensions) {
        ASDF_ERROR_OOM(value->file);
        goto failure;
    }

    asdf_extension_metadata_t **extension_p = extensions;
    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *val = NULL;
    while ((val = asdf_sequence_iter(extensions_seq, &iter))) {
        if (ASDF_VALUE_OK == asdf_value_as_extension_metadata(val, extension_p))
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
    asdf_sequence_t *history_seq = NULL;

    if (UNLIKELY(asdf_value_as_sequence(value, &history_seq) != ASDF_VALUE_OK))
        goto failure;

    int history_size = asdf_sequence_size(history_seq);

    if (UNLIKELY(history_size < 0))
        goto failure;

    asdf_history_entry_t **entries = (asdf_history_entry_t **)calloc(
        history_size + 1, sizeof(asdf_history_entry_t *));

    if (!entries) {
        ASDF_ERROR_OOM(value->file);
        goto failure;
    }

    asdf_history_entry_t **entry_p = entries;
    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *val = NULL;
    while ((val = asdf_sequence_iter(history_seq, &iter))) {
        if (ASDF_VALUE_OK == asdf_value_as_history_entry(val, entry_p))
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
    asdf_value_t *history_seq = NULL;
    asdf_value_t *extension_seq = NULL;
    asdf_extension_metadata_t **extensions = NULL;
    asdf_history_entry_t **history_entries = NULL;

    if (asdf_value_is_sequence(value)) {
        // Old-style history
        history_seq = value;
    } else if (asdf_value_is_mapping(value)) {
        asdf_mapping_t *history_map = NULL;
        asdf_value_as_mapping(value, &history_map);
        extension_seq = asdf_mapping_get(history_map, "extensions");
        history_seq = asdf_mapping_get(history_map, "entries");
    } else {
        ASDF_LOG(value->file, ASDF_LOG_WARN, "ignoring invalid \"history\" property");
    }

    if (extension_seq)
        extensions = asdf_meta_extensions_deserialize(extension_seq);

    if (history_seq)
        history_entries = asdf_meta_history_entries_deserialize(history_seq);

    history.extensions = extensions;
    history.entries = history_entries;
    asdf_value_destroy(history_seq);
    asdf_value_destroy(extension_seq);
    return history;
}


static asdf_value_err_t asdf_meta_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;
    asdf_software_t *asdf_library = NULL;
    asdf_meta_history_t history = {0};
    asdf_mapping_t *meta_map = NULL;

    if (asdf_value_as_mapping(value, &meta_map) != ASDF_VALUE_OK)
        goto failure;

    if ((prop = asdf_mapping_get(meta_map, "asdf_library"))) {
        if (ASDF_VALUE_OK != asdf_value_as_software(prop, &asdf_library))
            ASDF_LOG(value->file, ASDF_LOG_WARN, "ignoring invalid asdf_library software");
    }

    asdf_value_destroy(prop);

    if ((prop = asdf_mapping_get(meta_map, "history"))) {
        history = asdf_meta_history_deserialize(prop);
    }


    asdf_value_destroy(prop);

    asdf_meta_t *meta = calloc(1, sizeof(asdf_meta_t));

    if (!meta) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

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
        free((void *)meta->history.extensions);
    }

    if (meta->history.entries) {
        for (asdf_history_entry_t **ep = meta->history.entries; *ep; ++ep) {
            asdf_history_entry_destroy(*ep);
        }
        free((void *)meta->history.entries);
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
