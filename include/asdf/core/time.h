/* Data type an extension for http://stsci.edu/schemas/asdf/core/time-1.0.0 schema */
#ifndef ASDF_CORE_TIME_H
#define ASDF_CORE_TIME_H

#include <asdf/extension.h>
#include <sys/time.h>


ASDF_BEGIN_DECLS

#define ASDF_TIME_TIMESTR_MAXLEN    255

#define ASDF_TIME_EPOCH_JD_B1900 2415020.31352
#define ASDF_TIME_EPOCH_JD_J1900 2415020.0
#define ASDF_TIME_EPOCH_JD_J2000 2451545.0
#define ASDF_TIME_EPOCH_JD_MJD   2400000.5
#define ASDF_TIME_EPOCH_JD_UNIX  2440587.5

#define ASDF_TIME_AVG_MONTH_LENGTH   30.6001
#define ASDF_TIME_AVG_YEAR_LENGTH    365.242198781
#define ASDF_TIME_DAYS_IN_CENTURY    36524.2198781
#define ASDF_TIME_HOURS_PER_DAY      24
#define ASDF_TIME_SECONDS_PER_DAY    86400
#define ASDF_TIME_SECONDS_PER_HOUR   3600
#define ASDF_TIME_SECONDS_PER_MINUTE 60

typedef enum {
    ASDF_TIME_ERROR_SUCCESS=0,
    ASDF_TIME_ERROR_FAILED=-1,
    ASDF_TIME_ERROR_CONVERSION, // Failed to convert from one time format to another
    ASDF_TIME_ERROR_NOT_IMPLEMENTED, // Feature should exist but isn't supported yet.
    ASDF_TIME_ERROR_INVALID_FORMAT, // Invalid time format
    ASDF_TIME_ERROR_INVALID_SCALE, // Invalid time scale
} asdf_time_error;

typedef enum {
    ASDF_TIME_FORMAT_ISO_TIME=0,
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
    // "other" format(s) below
    ASDF_TIME_FORMAT_BYEAR_STR,
    ASDF_TIME_FORMAT_DATETIME,
    ASDF_TIME_FORMAT_FITS,
    ASDF_TIME_FORMAT_ISOT,
    ASDF_TIME_FORMAT_JYEAR_STR,
    ASDF_TIME_FORMAT_PLOT_DATE,
    ASDF_TIME_FORMAT_YMDHMS,
    ASDF_TIME_FORMAT_datetime64,
} asdf_time_base_format;


typedef enum {
    ASDF_TIME_SCALE_UTC=0,
    ASDF_TIME_SCALE_TAI,
    ASDF_TIME_SCALE_TCB,
    ASDF_TIME_SCALE_TCG,
    ASDF_TIME_SCALE_TDB,
    ASDF_TIME_SCALE_TT,
    ASDF_TIME_SCALE_UT1,
} asdf_time_scale;

typedef struct {
    double longitude;
    double latitude;
    double height;
} asdf_time_location_t;

typedef struct {
    bool is_base_format;
    asdf_time_base_format type;
} asdf_time_format_t;

struct asdf_time_info {
    struct timespec ts;
    struct tm tm;
};

typedef struct {
    char *value;
    struct asdf_time_info info;
    asdf_time_format_t format;
    asdf_time_scale scale;
    asdf_time_location_t location;
} asdf_time_t;

ASDF_DECLARE_EXTENSION(time, asdf_time_t);

ASDF_LOCAL int asdf_time_parse_std(const char *s, const asdf_time_format_t *format, struct asdf_time_info *out);
ASDF_LOCAL int asdf_time_parse_byear(const char *s, struct asdf_time_info *out);
ASDF_LOCAL int asdf_time_parse_yday(const char *s, struct asdf_time_info *out);
ASDF_LOCAL double asdf_time_convert_tm_to_julian(const struct tm *t);
ASDF_LOCAL void asdf_time_convert_julian_to_tm(double jd, struct tm *t, time_t *nanoseconds);
ASDF_LOCAL double asdf_time_convert_julian_to_mjd(double jd);
ASDF_LOCAL void asdf_time_convert_mjd_to_tm(double mjd, struct tm *t, time_t *nsec);
ASDF_LOCAL double asdf_time_convert_tm_to_besselian(const struct tm *t);
ASDF_LOCAL double asdf_time_convert_julian_to_besselian(double jd);
ASDF_LOCAL double asdf_time_convert_besselian_to_julian(double b);
ASDF_LOCAL void asdf_time_convert_besselian_to_tm(double b, struct tm *t, time_t *nsec);
ASDF_LOCAL double asdf_time_convert_jd_to_unix(double jd);
ASDF_LOCAL void asdf_time_info_dump(const struct asdf_time_info *t, FILE *stream);



ASDF_END_DECLS

#endif /* ASDF_CORE_TIME_H */
