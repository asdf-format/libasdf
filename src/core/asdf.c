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


static asdf_value_t *asdf_meta_history_extensions_serialize(
    asdf_file_t *file, const asdf_meta_t *meta) {
    assert(file);
    assert(meta);
    asdf_value_t *value = NULL;
    asdf_sequence_t *ext_seq = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_OOM;

    if (!meta->history.extensions)
        return value;

    ext_seq = asdf_sequence_create(file);

    if (!ext_seq)
        goto cleanup;

    const asdf_extension_metadata_t **extp = meta->history.extensions;

    while (*extp != NULL) {
        asdf_value_t *ext_val = asdf_value_of_extension_metadata(file, *extp);

        if (!ext_val)
            goto cleanup;

        err = asdf_sequence_append(ext_seq, ext_val);

        if (err != ASDF_VALUE_OK)
            goto cleanup;

        extp++;
    }

    value = asdf_value_of_sequence(ext_seq);
cleanup:
    if (err != ASDF_VALUE_OK)
        asdf_sequence_destroy(ext_seq);

    return value;
}


// TODO: It seems to be a common pattern to want to serialize some array of
// extension types; could be useful to have some macro for this
static asdf_value_t *asdf_meta_history_entries_serialize(
    asdf_file_t *file, const asdf_meta_t *meta) {
    assert(file);
    assert(meta);
    asdf_value_t *value = NULL;
    asdf_sequence_t *entry_seq = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_OOM;

    if (!meta->history.entries)
        return value;

    entry_seq = asdf_sequence_create(file);

    if (!entry_seq)
        goto cleanup;

    const asdf_history_entry_t **entryp = meta->history.entries;

    while (*entryp != NULL) {
        asdf_value_t *entry_val = asdf_value_of_history_entry(file, *entryp);

        if (!entry_val)
            goto cleanup;

        err = asdf_sequence_append(entry_seq, entry_val);

        if (err != ASDF_VALUE_OK)
            goto cleanup;

        entryp++;
    }

    value = asdf_value_of_sequence(entry_seq);
cleanup:
    if (err != ASDF_VALUE_OK)
        asdf_sequence_destroy(entry_seq);

    return value;
}


static asdf_value_t *asdf_meta_history_serialize(asdf_file_t *file, const asdf_meta_t *meta) {
    assert(file);
    assert(meta);
    asdf_value_t *value = NULL;
    asdf_mapping_t *history_map = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_OOM;

    if (meta->history.extensions && *meta->history.extensions) {
        asdf_value_t *extensions = asdf_meta_history_extensions_serialize(file, meta);

        if (extensions) {
            history_map = asdf_mapping_create(file);

            if (!history_map)
                goto cleanup;

            err = asdf_mapping_set(history_map, "extensions", extensions);

            if (err != ASDF_VALUE_OK)
                goto cleanup;
        }
    }

    if (meta->history.entries && *meta->history.entries) {
        asdf_value_t *entries = asdf_meta_history_entries_serialize(file, meta);

        if (entries) {
            if (!history_map)
                history_map = asdf_mapping_create(file);

            if (!history_map)
                goto cleanup;

            err = asdf_mapping_set(history_map, "entries", entries);

            if (err != ASDF_VALUE_OK)
                goto cleanup;
        }
    }

    value = asdf_value_of_mapping(history_map);
cleanup:
    if (err != ASDF_VALUE_OK)
        asdf_mapping_destroy(history_map);

    return value;
}


static asdf_value_t *asdf_meta_serialize(
    asdf_file_t *file,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    const void *obj,
    UNUSED(const void *userdata)) {

    if (UNLIKELY(!file || !obj))
        return NULL;

    const asdf_meta_t *meta = obj;
    asdf_mapping_t *meta_map = NULL;
    asdf_value_t *software_value = NULL;
    asdf_value_t *history_value = NULL;
    asdf_value_t *value = NULL;
    asdf_value_err_t err = ASDF_VALUE_OK;

    meta_map = asdf_mapping_create(file);
    software_value = asdf_value_of_software(
        file, meta->asdf_library ? meta->asdf_library : &libasdf_software);

    if (!software_value)
        goto cleanup;

    err = asdf_mapping_set(meta_map, "asdf_library", software_value);

    if (UNLIKELY(err != ASDF_VALUE_OK))
        goto cleanup;

    history_value = asdf_meta_history_serialize(file, meta);

    if (history_value)
        err = asdf_mapping_set(meta_map, "history", history_value);

    if (err != ASDF_VALUE_OK)
        goto cleanup;

    value = asdf_value_of_mapping(meta_map);
cleanup:
    if (err != ASDF_VALUE_OK) {
        asdf_value_destroy(software_value);
        asdf_value_destroy(history_value);
        asdf_mapping_destroy(meta_map);
    }

    return value;
}


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

    history.extensions = (const asdf_extension_metadata_t **)extensions;
    history.entries = (const asdf_history_entry_t **)history_entries;
    asdf_value_destroy(history_seq);
    asdf_value_destroy(extension_seq);
    return history;
}


static void asdf_meta_history_dealloc(asdf_meta_history_t *history) {
    if (!history)
        return;

    if (history->extensions) {
        for (const asdf_extension_metadata_t **ep = history->extensions; *ep; ++ep) {
            asdf_extension_metadata_destroy((asdf_extension_metadata_t *)*ep);
        }
        free((void *)history->extensions);
    }


    if (history->entries) {
        for (const asdf_history_entry_t **ep = history->entries; *ep; ++ep) {
            asdf_history_entry_destroy((asdf_history_entry_t *)*ep);
        }
        free((void *)history->entries);
    }
}


static void asdf_meta_dealloc(void *value) {
    if (!value)
        return;

    asdf_meta_t *meta = value;

    if (meta->asdf_library)
        asdf_software_destroy(meta->asdf_library);

    asdf_meta_history_dealloc(&meta->history);
    free(meta);
}


static void *asdf_meta_copy(const void *value) {
    if (!value)
        return NULL;

    const asdf_meta_t *meta = value;
    asdf_meta_t *copy = calloc(1, sizeof(asdf_meta_t));

    if (!copy) {
        ASDF_ERROR_OOM(NULL);
        return NULL;
    }

    if (meta->asdf_library) {
        copy->asdf_library = asdf_software_clone(meta->asdf_library);

        if (!copy->asdf_library)
            goto failure;
    }

    if (meta->history.extensions) {
        copy->history.extensions = (const asdf_extension_metadata_t **)
            asdf_extension_metadata_array_clone(meta->history.extensions);

        if (!copy->history.extensions)
            goto failure;
    }

    if (meta->history.entries) {
        copy->history.entries = (const asdf_history_entry_t **)asdf_history_entry_array_clone(
            meta->history.entries);

        if (!copy->history.entries)
            goto failure;
    }

    return copy;

failure:
    asdf_meta_dealloc(copy);
    return NULL;
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
    asdf_meta_history_dealloc(&history);
    return err;
}


/* Define the extension for the core/asdf schema
 *
 * The internal types and methods are named ``asdf_meta_*``, however, to avoid names like
 * ``asdf_asdf_t`` and ``asdf_get_asdf`` and so on.
 */
ASDF_REGISTER_EXTENSION(
    meta,
    ASDF_CORE_ASDF_TAG,
    asdf_meta_t,
    &libasdf_software,
    asdf_meta_serialize,
    asdf_meta_deserialize,
    asdf_meta_copy,
    asdf_meta_dealloc,
    NULL);
