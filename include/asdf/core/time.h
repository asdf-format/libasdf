/* Data type an extension for http://stsci.edu/schemas/asdf/core/time-1.0.0 schema */
#ifndef ASDF_CORE_TIME_H
#define ASDF_CORE_TIME_H

#include <asdf/extension.h>
#include <sys/time.h>


ASDF_BEGIN_DECLS

#define ASDF_TIME_TIMESTR_MAXLEN 255

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

struct asdf_time_info_t {
    struct timespec ts;
    struct tm tm;
};

typedef struct {
    char *value;
    struct asdf_time_info_t info;
    asdf_time_format_t format;
    asdf_time_scale scale;
    asdf_time_location_t location;
} asdf_time_t;

ASDF_DECLARE_EXTENSION(time, asdf_time_t);

ASDF_LOCAL int asdf_time_parse_std(const char *s, const asdf_time_format_t *format, struct asdf_time_info_t *out);
ASDF_LOCAL int asdf_time_parse_byear(const char *s, struct asdf_time_info_t *out);
ASDF_LOCAL int asdf_time_parse_yday(const char *s, struct asdf_time_info_t *out);
ASDF_LOCAL void show_asdf_time_info(const struct asdf_time_info_t *t);


ASDF_END_DECLS

#endif /* ASDF_CORE_TIME_H */
