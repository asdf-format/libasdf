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


/*
 * Parse a YAML-serialized timestamp
 *
 * Generally in ISO8601 but can be "relaxed" having a space between the date and the time (the
 * Python asdf actually appears to output in this format though maybe it depends on the Python
 * yaml version--we should specify this more strictly maybe...
 */
#ifdef HAVE_STRPTIME
static int asdf_parse_datetime(const char *s, struct timespec *out) {
    if (!s || !out)
        return -1;

    struct tm tm = {0};
    char tz_sign = 0;
    int tz_hour = 0;
    int tz_min = 0;
    long nsec = 0;
    bool has_time = false;
    char *rest = NULL;
    char *buf = strdup(s);

    if (!buf)
        return -1;

    // Normalize separators (replace 'T' or 't' with space)
    for (char *c = buf; *c; ++c)
        if (*c == 'T' || *c == 't')
            *c = ' ';

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
            nsec = (long)((frac - (int)frac) * 1e9);
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
    time_t t = timegm(&tm);
    if (t == (time_t)-1) {
        free(buf);
        return -1;
    }

    t -= tz_sign * (tz_hour * 3600 + tz_min * 60);
    out->tv_sec = t;
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
    struct timespec time = {0};
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
    prop = asdf_mapping_get(value, "software");

    if (prop) {
        software = asdf_history_entry_deserialize_software(prop);
    }

    asdf_value_destroy(prop);

    asdf_history_entry_t *entry = calloc(1, sizeof(asdf_history_entry_t));

    if (!entry) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    entry->description = description;
    entry->time = time;
    entry->software = (const asdf_software_t **)software;
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

    if (entry->software) {
        asdf_software_t **sp = (asdf_software_t **)entry->software;
        while (*sp++) {
            asdf_software_destroy(*sp);
        }
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
