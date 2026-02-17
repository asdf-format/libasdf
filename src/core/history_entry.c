#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../error.h"
#include "../extension_util.h"
#include "../file.h"
#include "../log.h"
#include "../util.h"
#include "../value.h"

#include "asdf.h"
#include "history_entry.h"
#include "software.h"


#define ASDF_CORE_HISTORY_ENTRY_TAG ASDF_CORE_TAG_PREFIX "history_entry-1.0.0"

/*
 * Parse a YAML-serialized timestamp
 *
 * Generally in ISO8601 but can be "relaxed" having a space between the date and the time (the
 * Python asdf actually appears to output in this format though maybe it depends on the Python
 * yaml version--we should specify this more strictly maybe...
 */
#ifdef HAVE_STRPTIME
#define NSEC_PER_SEC 1e9 // In case this ever changes
#define SEC_PER_HOUR 3600
#define SEC_PER_MIN 60

static int asdf_parse_datetime(const char *scalar, struct timespec *out) {
    if (!scalar || !out)
        return -1;

    struct tm tm = {0};
    char tz_sign = 0;
    int tz_hour = 0;
    int tz_min = 0;
    long nsec = 0;
    bool has_time = false;
    char *rest = NULL;
    char *buf = strdup(scalar);

    if (!buf)
        return -1;

    // Normalize separators (replace 'T' or 't' with space)
    for (char *chr = buf; *chr; ++chr)
        if (*chr == 'T' || *chr == 't')
            *chr = ' ';

    // Try to parse date and time (without optional fractional seconds and timezone)
    rest = strptime(buf, "%Y-%m-%d %H:%M:%S", &tm);

    if (!rest)
        rest = strptime(buf, "%Y-%m-%d", &tm);
    else
        has_time = true;

    if (!rest) {
        free(buf);
        return -1;
    }

    // Handle optional fractional seconds
    if (has_time) {
        const char *dot = strchr(rest, '.');
        if (dot) {
            double frac = 0;
            sscanf(dot, "%lf", &frac);
            nsec = (long)((frac - (int)frac) * NSEC_PER_SEC);
        }

        // Handle timezone offsets (Z/z = Zulu is ignored, just don't add any offset)
        const char *tz = strpbrk(rest, "+-");
        if (tz && (*tz == '+' || *tz == '-')) {
            tz_sign = (*tz == '-') ? -1 : 1;
            if (sscanf(tz + 1, "%2d:%2d", &tz_hour, &tz_min) < 1)
                sscanf(tz + 1, "%2d", &tz_hour);
        }
    }

    // Convert to time_t and adjust for time zone
    time_t time = timegm(&tm);
    if (time == (time_t)-1) {
        free(buf);
        return -1;
    }

    time -= (long)tz_sign * (tz_hour * SEC_PER_HOUR + tz_min * SEC_PER_MIN);
    out->tv_sec = time;
    out->tv_nsec = nsec;
    free(buf);
    return 0;
}
#else
#warning "strptime() not available, times will not be parsed"
static int asdf_parse_datetime(UNUSED(const char *s), struct timespec *out) {
    if (out) {
        out->tv_sec = 0;
        out->tv_nsec = 0;
    }
    return 0;
}
#endif


static asdf_value_t *asdf_history_entry_serialize(
    asdf_file_t *file,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    const void *obj,
    UNUSED(const void *userdata)) {

    if (UNLIKELY(!file || !obj))
        return NULL;

    const asdf_history_entry_t *entry = obj;
    asdf_mapping_t *entry_map = NULL;
    asdf_value_t *value = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_EMIT_FAILURE;
    asdf_value_t *software_val = NULL;
    asdf_sequence_t *software_seq = NULL;
    size_t software_count = 0;
    const asdf_software_t **softwarep = entry->software;

    if (softwarep) {
        while (*(softwarep++) != NULL)
            software_count++;
    }

    if (software_count == 1) {
        software_val = asdf_value_of_software(file, *entry->software);
    } else if (software_count > 1) {
        software_seq = asdf_sequence_create(file);

        if (!software_seq)
            goto cleanup;

        softwarep = entry->software;
        while (*softwarep != NULL) {
            software_val = asdf_value_of_software(file, *softwarep);

            if (software_val != NULL)
                err = asdf_sequence_append(software_seq, software_val);

            softwarep++;
        }

        software_val = asdf_value_of_sequence(software_seq);
    }

    if (!software_val)
        ASDF_LOG(
            file,
            ASDF_LOG_WARN,
            ASDF_CORE_HISTORY_ENTRY_TAG " should have at least one software entry");

    entry_map = asdf_mapping_create(file);

    if (UNLIKELY(!entry_map))
        goto cleanup;

    err = asdf_mapping_set_string0(
        entry_map, "description", entry->description ? entry->description : "");

    if (UNLIKELY(err != ASDF_VALUE_OK))
        goto cleanup;

    if (software_val)
        err = asdf_mapping_set(entry_map, "software", software_val);

    if (UNLIKELY(err != ASDF_VALUE_OK))
        goto cleanup;

    // TODO: Serialize the .time field if set; wait on #91 for that since it
    // refactors a lot of timestamp handling

    value = asdf_value_of_mapping(entry_map);
cleanup:
    if (UNLIKELY(err != ASDF_VALUE_OK)) {
        asdf_value_destroy(software_val);
        asdf_mapping_destroy(entry_map);
    }
    return value;
}


static asdf_software_t **asdf_history_entry_deserialize_software(asdf_value_t *value) {
    asdf_sequence_t *software_seq = NULL;

    if (asdf_value_as_sequence(value, &software_seq) != ASDF_VALUE_OK) {
        // Optimistically allocate enough for one entry and the NULL terminator, though if the
        // software entry is invalid the first entry will also be NULL
        asdf_software_t **software = (asdf_software_t **)calloc(2, sizeof(asdf_software_t *));

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
    int n_entries = asdf_sequence_size(software_seq);

    if (n_entries < 0)
        return NULL;

    asdf_software_t **software = (asdf_software_t **)calloc(
        n_entries + 1, sizeof(asdf_software_t *));

    if (!software) {
        ASDF_ERROR_OOM(value->file);
        return NULL;
    }

    asdf_software_t **software_p = software;
    asdf_sequence_iter_t iter = asdf_sequence_iter_init();
    asdf_value_t *val = NULL;
    while (NULL != (val = asdf_sequence_iter(software_seq, &iter))) {
        if (ASDF_VALUE_OK == asdf_value_as_software(val, software_p))
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
    struct timespec time = {0};
    asdf_software_t **software = NULL;
    asdf_mapping_t *entry_map = NULL;

    if (asdf_value_as_mapping(value, &entry_map) != ASDF_VALUE_OK)
        goto failure;

    /* The description field is the only required */
    if (!(prop = asdf_mapping_get(entry_map, "description")))
        goto failure;


    if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &description))
        goto failure;

    asdf_value_destroy(prop);

    prop = asdf_mapping_get(entry_map, "time");

    if (prop) {
        bool valid_time = false;
        if (ASDF_VALUE_OK == asdf_value_as_string0(prop, &time_str)) {
            if (0 == asdf_parse_datetime(time_str, &time))
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

    /* Software can be either an array of software or a single entry, but here it is always
     * returned as a NULL-terminated array of asdf_software_t *
     */
    prop = asdf_mapping_get(entry_map, "software");

    if (prop) {
        software = asdf_history_entry_deserialize_software(prop);
    }

    asdf_value_destroy(prop);

    asdf_history_entry_t *entry = calloc(1, sizeof(asdf_history_entry_t));

    if (!entry) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    entry->description = strdup(description);
    entry->time = time;
    entry->software = (const asdf_software_t **)software;
    *out = entry;
    return ASDF_VALUE_OK;
failure:
    asdf_value_destroy(prop);
    free((void *)software);
    return err;
}


static void asdf_history_entry_dealloc(void *value) {
    if (!value)
        return;

    asdf_history_entry_t *entry = value;

    free((void *)entry->description);

    if (entry->software) {
        for (asdf_software_t **sp = (asdf_software_t **)entry->software; *sp; ++sp) {
            asdf_software_destroy(*sp);
        }
        free((void *)entry->software);
    }

    free(entry);
}


static void *asdf_history_entry_copy(const void *value) {
    if (!value)
        return NULL;

    const asdf_history_entry_t *entry = value;

    asdf_history_entry_t *copy = calloc(1, sizeof(asdf_history_entry_t));

    if (!copy)
        goto failure;

    if (entry->description) {
        copy->description = strdup(entry->description);

        if (!copy->description)
            goto failure;
    }

    copy->time = entry->time;

    if (entry->software) {
        copy->software = (const asdf_software_t **)asdf_software_array_clone(entry->software);

        if (!copy->software)
            goto failure;
    }

    return copy;
failure:
    asdf_history_entry_destroy(copy);
    ASDF_ERROR_OOM(NULL);
    return NULL;
}


/* Define the extension for the core/history_entry-1.0.0 schema
 *
 */
ASDF_REGISTER_EXTENSION(
    history_entry,
    ASDF_CORE_HISTORY_ENTRY_TAG,
    asdf_history_entry_t,
    &libasdf_software,
    asdf_history_entry_serialize,
    asdf_history_entry_deserialize,
    asdf_history_entry_copy,
    asdf_history_entry_dealloc,
    NULL);


/** Additional history entry methods */

int asdf_history_entry_add(asdf_file_t *file, const char *description) {
    // Just use asdf_array_concat to allocate/extend file->history_entries
    // In practice it will be rare to add more than one entry per opening of
    // the file (existing entries are prepended to this list when outputting)
    asdf_history_entry_t *entry = calloc(1, sizeof(asdf_history_entry_t));

    if (!entry)
        goto failure;

    entry->description = strdup(description);
    entry->software = (const asdf_software_t **)calloc(2, sizeof(asdf_software_t *));

    if (!entry->software)
        goto failure;

    entry->software[0] = asdf_software_clone(&libasdf_software);

    if (!entry->software[0])
        goto failure;

    asdf_history_entry_t *entries[] = {entry, NULL};
    file->history_entries = (asdf_history_entry_t **)asdf_array_concat(
        (void **)file->history_entries, (const void **)entries);

    if (!file->history_entries)
        goto failure;

    return 0;
failure:
    ASDF_ERROR_OOM(file);
    asdf_history_entry_destroy(entry);
    return -1;
}
