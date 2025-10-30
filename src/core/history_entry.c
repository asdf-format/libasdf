#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <string.h>
#include <time.h>

#include <asdf/core/asdf.h>
#include <asdf/core/history_entry.h>
#include <asdf/core/software.h>
#include <asdf/extension.h>

#include "../error.h"
#include "../log.h"
#include "../util.h"
#include "../value.h"
#include "asdf/core/time.h"


static asdf_software_t **asdf_history_entry_deserialize_software(asdf_value_t *value) {
    if (!asdf_value_is_sequence(value)) {
        // Optimistically allocate enough for one entry and the NULL terminator, though if the
        // software entry is invalid the first entry will also be NULL
        asdf_software_t **software = calloc(2, sizeof(asdf_software_t *));

        if (!software) {
            ASDF_ERROR_OOM(value->file);
            return NULL;
        }

        if (ASDF_VALUE_OK != asdf_value_as_software(value, &software[0]))
            ASDF_LOG(
                value->file, ASDF_LOG_WARN, "ignoring invalid software entry in history_entry");
        return software;
    }

    // Case where it's an array
    int n_entries = asdf_sequence_size(value);

    if (n_entries < 0)
        return NULL;

    asdf_software_t **software = calloc(n_entries + 1, sizeof(asdf_software_t *));

    if (!software) {
        ASDF_ERROR_OOM(value->file);
        return NULL;
    }

    asdf_software_t **software_p = software;
    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *it = NULL;
    while (NULL != (it = asdf_sequence_iter(value, &iter))) {
        if (ASDF_VALUE_OK == asdf_value_as_software(it, software_p))
            software_p++;
        else
            ASDF_LOG(
                value->file, ASDF_LOG_WARN, "ignoring invalid software entry in history_entry");
    }

    return software;
}

static asdf_value_err_t asdf_history_entry_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;
    asdf_value_t *prop = NULL;
    const char *description = NULL;
    const char *time_str = NULL;
    asdf_time_t *time = NULL;
    asdf_software_t **software = NULL;

    if (!asdf_value_is_mapping(value))
        goto failure;

    /* The description field is the only required */
    if (!(prop = asdf_mapping_get(value, "description")))
        goto failure;


    if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &description))
        goto failure;

    asdf_value_destroy(prop);

    prop = asdf_mapping_get(value, "time");
    if (prop) {

        // cast the value of "time" to an asdf_time_t
        const asdf_extension_t *time_ext = asdf_extension_get(value->file, "tag:stsci.edu:asdf/time/time-1.4.0");
        if (time_ext) {
            bool valid_time = false;
            time_ext->deserialize(prop, NULL, (void *) &time);

            if (time) {
                valid_time = true;
            }

            #ifdef ASDF_LOG_ENABLED
            if (!valid_time) {
                if (ASDF_VALUE_OK != asdf_value_as_scalar0(prop, &time_str)) {
                    time_str = "<unreadable>";
                }
                ASDF_LOG(
                    value->file, ASDF_LOG_WARN, "ignoring invalid time %s in history_entry", time_str);
            }
            #endif
        }
        asdf_value_destroy(prop);
    }


    /* Software can be either an array of software or a single entry, but here it is always
     * returned as a NULL-terminated array of asdf_software_t *
     */
    prop = asdf_mapping_get(value, "software");

    if (prop) {
        software = asdf_history_entry_deserialize_software(prop);
    }

    asdf_value_destroy(prop);

    asdf_history_entry_t *entry = calloc(1, sizeof(asdf_history_entry_t));

    if (!entry)
        return ASDF_VALUE_ERR_OOM;

    entry->description = description;
    entry->software = (const asdf_software_t **)software;
    if (time) {
        entry->time = time;
    }
    *out = entry;

    return ASDF_VALUE_OK;
failure:
    asdf_value_destroy(prop);
    return err;
}


static void asdf_history_entry_dealloc(void *value) {
    if (!value)
        return;

    asdf_history_entry_t *entry = value;

    if (entry->time) {
        asdf_time_destroy((asdf_time_t *) entry->time);
    }

    if (entry->software) {
        for (asdf_software_t **sp = (asdf_software_t **)entry->software; *sp; ++sp) {
            asdf_software_destroy(*sp);
        }
        free(entry->software);
    }

    free(entry);
}


/* Define the extension for the core/history_entry-1.0.0 schema
 *
 */
ASDF_REGISTER_EXTENSION(
    history_entry,
    ASDF_CORE_TAG_PREFIX "history_entry-1.0.0",
    asdf_history_entry_t,
    &libasdf_software,
    asdf_history_entry_deserialize,
    asdf_history_entry_dealloc,
    NULL);
