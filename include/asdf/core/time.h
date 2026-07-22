/** Data type and extension for the stsci.edu/schemas/asdf/time/time schema */
#ifndef ASDF_CORE_TIME_H
#define ASDF_CORE_TIME_H

#include <sys/time.h>
#include <time.h>

#include <asdf/extension.h>


ASDF_BEGIN_DECLS

/* The default tag written for a time object; also the newest version read.
 * Older versions (ASDF_CORE_TIME_TAG_BASE "1.x.0") are recognized when reading;
 * see the ASDF_REGISTER_EXTENSION call in time.c. */
#define ASDF_CORE_TIME_TAG_BASE "tag:stsci.edu:asdf/time/time-"
#define ASDF_CORE_TIME_TAG ASDF_CORE_TIME_TAG_BASE "1.4.0"
#define ASDF_TIME_TIMESTR_MAXLEN 255

typedef enum {
    /** Unset/unspecified format; and the zero value */
    ASDF_TIME_FORMAT_NONE = 0,
    ASDF_TIME_FORMAT_ISO,
    ASDF_TIME_FORMAT_YDAY,
    ASDF_TIME_FORMAT_BYEAR,
    ASDF_TIME_FORMAT_JYEAR,
    ASDF_TIME_FORMAT_DECIMALYEAR,
    ASDF_TIME_FORMAT_JD,
    ASDF_TIME_FORMAT_MJD,
    ASDF_TIME_FORMAT_GPS,
    ASDF_TIME_FORMAT_UNIX,
    ASDF_TIME_FORMAT_UTIME,
    ASDF_TIME_FORMAT_TAI_SECONDS,
    ASDF_TIME_FORMAT_CXCSEC,
    ASDF_TIME_FORMAT_GALEXSEC,
    ASDF_TIME_FORMAT_UNIX_TAI,
    ASDF_TIME_FORMAT_RESERVED1,
    /* "other" format(s) below */
    ASDF_TIME_FORMAT_BYEAR_STR,
    ASDF_TIME_FORMAT_DATETIME,
    ASDF_TIME_FORMAT_FITS,
    ASDF_TIME_FORMAT_ISOT,
    ASDF_TIME_FORMAT_JYEAR_STR,
    ASDF_TIME_FORMAT_PLOT_DATE,
    ASDF_TIME_FORMAT_YMDHMS,
    ASDF_TIME_FORMAT_DATETIME64,
} asdf_time_format_t;


typedef enum {
    ASDF_TIME_SCALE_UTC = 0,
    ASDF_TIME_SCALE_TAI,
    ASDF_TIME_SCALE_TCB,
    ASDF_TIME_SCALE_TCG,
    ASDF_TIME_SCALE_TDB,
    ASDF_TIME_SCALE_TT,
    ASDF_TIME_SCALE_UT1,
} asdf_time_scale_t;

typedef struct {
    double longitude;
    double latitude;
    double height;
} asdf_time_location_t;

typedef struct {
    struct timespec ts;
    struct tm tm;
} asdf_time_info_t;

typedef struct {
    char *value;
    asdf_time_info_t info;
    asdf_time_format_t format;
    /**
     * The original format of the time object (the optional ``base_format``
     * field, added in time-1.2.0)
     *
     * Left as ASDF_TIME_FORMAT_NONE (the zero value) when unset, in which case
     * it is omitted on serialization; deserialization sets it only when the
     * ``base_format`` key is present.
     */
    asdf_time_format_t base_format;
    asdf_time_scale_t scale;
    asdf_time_location_t location;
} asdf_time_t;

ASDF_DECLARE_EXTENSION(time, asdf_time_t);

ASDF_EXPORT int asdf_time_parse(asdf_time_t *time);
ASDF_EXPORT const char *asdf_time_format_string(asdf_time_format_t format);

ASDF_END_DECLS

#endif /* ASDF_CORE_TIME_H */
