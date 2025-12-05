#include <math.h>

#include <asdf/core/asdf.h>
#include <asdf/core/time.h>
#include <asdf/extension.h>

#include "../log.h"
#include "../util.h"
#include "../value.h"
#include "stc/cregex.h"

#ifdef HAVE_STRPTIME
const char *ASDF_TIME_SFMT_ISO_TIME[] = {"%Y-%m-%d %H:%M:%S", "%Y-%m-%d"};
const char *ASDF_TIME_SFMT_JD[] = {"%j"};
const char *ASDF_TIME_SFMT_YDAY[] = {"%Y:%j:%H:%M:%S", "%Y:%j"};
const char *ASDF_TIME_SFMT_UNIX[] = {"%s"};

#define check_format_strptime(TYPE, BUF, TM, HAS_TIME, STATUS) \
    { \
        size_t i = 0; \
        do { \
            (STATUS) = strptime((BUF), (TYPE)[i], (TM)); \
            if ((STATUS)) { \
                (HAS_TIME) = true; \
                break; \
            } \
        } while (i++ && i < sizeof((TYPE)) / sizeof(*(TYPE))); \
    }

int is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int get_days_in_month(int month, int year) {
    int days[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (month == 2 && is_leap_year(year)) return 29;
    return days[month - 1];
}

#define JD_B1900 2415020.31352
#define JD_J2000 2451545.0
#define JD_MJD   2400000.5
#define JD_UNIX  2440587.5

// Calendar constants
const double JD_GREGORIAN_START = 2299161.0;
const double JD_CORRECTION_REF  = 1867216.25;
const double JD_CALENDAR_OFFSET = 122.1;
const int    JD_BASE_YEAR = 4716;
const double JD_YEAR_LENGTH = 365.25;
const double JD_EPOCH_BASE = 1524.0;
const double GREGORIAN_OFFSET = (int) JD_EPOCH_BASE;
const double JD_EPOCH_SHIFT = JD_EPOCH_BASE + 0.5;

const double AVG_MONTH_LENGTH = 30.6001;
const double AVG_YEAR_LENGTH = 365.242198781;
const double DAYS_IN_CENTURY = 36524.2198781;
const int    HOURS_PER_DAY = 24;
const int    SECONDS_PER_DAY = 86400;
const int    SECONDS_PER_HOUR = 3600;
const int    SECONDS_PER_MINUTE = 60;

void show_timespec(const struct timespec *t) {
    fprintf(stderr, "seconds = %lu\n", t->tv_sec);
    fprintf(stderr, "nanoseconds = %lu\n", t->tv_nsec);
}


void show_tm(const struct tm *t) {
    fprintf(stderr, "year = %d\n", t->tm_year + 1900);
    fprintf(stderr, "month = %d\n", t->tm_mon + 1);
    fprintf(stderr, "day = %d\n", t->tm_mday);
    fprintf(stderr, "hour = %d\n", t->tm_hour);
    fprintf(stderr, "minute = %d\n", t->tm_min);
    fprintf(stderr, "second = %d\n", t->tm_sec);
    fprintf(stderr, "weekday = %d\n", t->tm_wday);
    fprintf(stderr, "year day = %d\n", t->tm_yday);
    fprintf(stderr, "dst = %d\n", t->tm_isdst);
    fprintf(stderr, "gmt offset = %lu\n", t->tm_gmtoff);
    fprintf(stderr, "timezone: %s\n", t->tm_zone);
}

void show_asdf_time_info(const struct asdf_time_info_t *t) {
    #if !defined(NDEBUG)
    show_tm(&t->tm);
    printf("\n");
    show_timespec(&t->ts);
    printf("\n");
    #else
    (void *) t;
    #endif
}

double jd_to_unix(const double jd) {
    return (jd - JD_UNIX) / SECONDS_PER_DAY;
}

// Julian Date to Gregorian calendar conversion
void julian_to_tm(const double jd, struct tm *t, time_t *nanoseconds) {
    const double jd_shift = jd + 0.5;
    const int jd_int = (int)jd_shift;
    const double day_fraction = jd_shift - jd_int;

    int jd_adjust;
    if (jd_int < JD_GREGORIAN_START) {
        jd_adjust = jd_int;
    } else {
        const int leap_adjust = (int)((jd_int - JD_CORRECTION_REF) / DAYS_IN_CENTURY);
        jd_adjust = jd_int + 1 + leap_adjust - leap_adjust / 4;
    }

    const int calendar_day = jd_adjust + GREGORIAN_OFFSET;
    const int year_base = (int)((calendar_day - JD_CALENDAR_OFFSET) / JD_YEAR_LENGTH);
    const int days_in_years = (int)(JD_YEAR_LENGTH * year_base);
    const int month_base = (int)((calendar_day - days_in_years) / AVG_MONTH_LENGTH);

    const int day = calendar_day - days_in_years - (int)(AVG_MONTH_LENGTH * month_base) + day_fraction;
    const int month = month_base < 14 ? month_base - 1 : month_base - 13;
    const int year = month > 2 ? year_base - JD_BASE_YEAR : year_base - JD_BASE_YEAR - 1;

    const double total_seconds = day_fraction * SECONDS_PER_DAY + 0.5;
    const int hour = (int)(total_seconds / SECONDS_PER_HOUR);
    const int minute = (int)((total_seconds - hour * SECONDS_PER_HOUR) / SECONDS_PER_MINUTE);
    const double seconds_whole = total_seconds - hour * SECONDS_PER_HOUR - minute * SECONDS_PER_MINUTE;
    const int second = (int)seconds_whole;
    const double fractional_seconds = seconds_whole - second;

    t->tm_year = year - 1900;
    t->tm_mon = month - 1;
    t->tm_mday = day;
    t->tm_hour = hour;
    t->tm_min = minute;
    t->tm_sec = second;

    if (nanoseconds) {
        *nanoseconds = (time_t) (fractional_seconds * 1e9) + 0.5;
    }
}

double tm_to_julian(const struct tm *t) {
    const int calendar_year = t->tm_year + 1900;
    const int calendar_month = t->tm_mon + 1;
    const int day_of_month = t->tm_mday;
    const int hour = t->tm_hour;
    const int minute = t->tm_min;
    const int second = t->tm_sec;

    // Adjust year and month for Julian date formula
    const int adjusted_year = (calendar_month <= 2) ? calendar_year - 1 : calendar_year;
    const int adjusted_month = (calendar_month <= 2) ? calendar_month + 12 : calendar_month;

    // Gregorian calendar correction
    const int century = adjusted_year / 100;
    const int gregorian_correction = 2 - century + (century / 4);

    // Fractional day from time
    const double fractional_day = (hour + minute / (double)SECONDS_PER_MINUTE + second / (double)SECONDS_PER_HOUR) / HOURS_PER_DAY;

    // Julian Date calculation using approximate year/month lengths
    const double julian_date = floor(JD_YEAR_LENGTH * (adjusted_year + JD_BASE_YEAR))
                             + floor(AVG_MONTH_LENGTH * (adjusted_month + 1))
                             + day_of_month + fractional_day + gregorian_correction - JD_EPOCH_SHIFT;

    return julian_date;
}

double julian_to_mjd(const double jd) {
    return jd - JD_MJD;
}

void mjd_to_tm(const double mjd, struct tm *t, time_t *nsec) {
    const double jd = mjd + JD_MJD;
    julian_to_tm(jd, t, nsec);
}

double julian_to_besselian(const double jd) {
    return 1900.0 + (jd - JD_B1900) / AVG_YEAR_LENGTH;
}

double besselian_to_julian(const double b) {
    return JD_B1900 + AVG_YEAR_LENGTH * (b - 1900.0);
}

void besselian_to_tm(const double b, struct tm *t, time_t *nsec) {
    const double jd = besselian_to_julian(b);
    julian_to_tm(jd, t, nsec);
}

int asdf_time_parse_std(const char *s, const asdf_time_format_t *format, struct asdf_time_info_t *out) {
    if (!s || !out) {
        return -1;
    }

    struct tm tm = {0};
    char tz_sign = 0;
    int tz_hour = 0;
    int tz_min = 0;
    long nsec = 0;
    bool has_time = false;
    char *rest = NULL;
    char *buf = strdup(s);

    if (!buf) {
        return -1;
    }

    // Normalize separators (replace 'T' or 't' with space)
    for (char *c = buf; *c; ++c) {
        if (*c == 'T' || *c == 't')
            *c = ' ';
    }

    switch (format->type) {
        case ASDF_TIME_FORMAT_DATETIME:
        case ASDF_TIME_FORMAT_ISO_TIME:
            check_format_strptime(ASDF_TIME_SFMT_ISO_TIME, buf, &tm, has_time, rest);
            break;
        case ASDF_TIME_FORMAT_YDAY:
            check_format_strptime(ASDF_TIME_SFMT_YDAY, buf, &tm, has_time, rest);
            break;
        case ASDF_TIME_FORMAT_UNIX:
            check_format_strptime(ASDF_TIME_SFMT_UNIX, buf, &tm, has_time, rest);
            break;
        default:
            return -1;
    }

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
            tz_sign = *tz == '-' ? -1 : 1;
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

    t -= tz_sign * (tz_hour * SECONDS_PER_HOUR + tz_min * SECONDS_PER_MINUTE);

    out->tm = *gmtime(&t);
    out->ts.tv_sec = t;
    out->ts.tv_nsec = nsec;
    free(buf);
    return 0;
}


int asdf_time_parse_jd(UNUSED(const char *s), struct asdf_time_info_t *out) {
    const double jd = strtod(s, NULL);
    struct tm jd_tm;
    time_t nsec = 0;
    julian_to_tm(jd, &jd_tm, &nsec);
    const time_t t = timegm(&jd_tm);

    if (out) {
        out->tm = jd_tm;
        out->ts.tv_sec = t;
        out->ts.tv_nsec = nsec;
    } else {
        return -1;
    }
    return 0;
}

int asdf_time_parse_mjd(UNUSED(const char *s), struct asdf_time_info_t *out) {
    const double mjd = julian_to_mjd(strtod(s, NULL)) + JD_MJD;
    struct tm mjd_tm;
    time_t nsec = 0;
    mjd_to_tm(mjd, &mjd_tm, &nsec);
    const time_t t = timegm(&mjd_tm);

    if (out) {
        out->tm = mjd_tm;
        out->ts.tv_sec = t;
        out->ts.tv_nsec = nsec;
    } else {
        return -1;
    }
    return 0;
}

int asdf_time_parse_byear(UNUSED(const char *s), struct asdf_time_info_t *out) {
    const double byear = strtod(s, NULL);
    const double jd = besselian_to_julian(byear);
    struct tm tm;
    time_t nsec = 0;

    julian_to_tm(jd, &tm, &nsec);
    const time_t t = timegm(&tm);

    if (out) {
        out->tm = *gmtime(&t);
        out->ts.tv_sec = t;
        out->ts.tv_nsec = nsec;
    } else {
        return -1;
    }
    return 0;
}
#else
#warning "strptime() not available, times will not be parsed"
static int asdf_time_parse_std(UNUSED(const char *s), struct timespec *out) {
    if (out) {
        out->tv_sec = 0;
        out->tv_nsec = 0;
    }
    return 0;
}
#endif

static int asdf_time_parse_time(UNUSED(const char *s), const asdf_time_format_t *format, struct asdf_time_info_t *out) {
    int status = -1;
    switch (format->type) {
        case ASDF_TIME_FORMAT_YDAY:
        case ASDF_TIME_FORMAT_ISO_TIME:
        case ASDF_TIME_FORMAT_DATETIME:
        case ASDF_TIME_FORMAT_UNIX:
            status = asdf_time_parse_std(s, format, out);
            break;
        case ASDF_TIME_FORMAT_MJD:
            status = asdf_time_parse_mjd(s, out);
            break;
        case ASDF_TIME_FORMAT_JD:
            status = asdf_time_parse_jd(s, out);
            break;
        case ASDF_TIME_FORMAT_BYEAR:
            status = asdf_time_parse_byear(s, out);
            break;
        default:
            break;
    }
    return status;
}

static asdf_value_err_t asdf_time_deserialize(asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    const char *value_s = NULL;
    const char *format_s = NULL;

    asdf_value_t *prop = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;

    asdf_time_t *time = calloc(1, sizeof(asdf_time_t));
    if (!time) {
        return ASDF_VALUE_ERR_OOM;
    }

    if (asdf_value_is_mapping(value)) {
        prop = asdf_mapping_get(value, "value");
    } else {
        prop = value;
    }

    time->value = calloc(ASDF_TIME_TIMESTR_MAXLEN, sizeof(*time->value));
    if (!time->value) {
        return ASDF_VALUE_ERR_OOM;
    }

    const asdf_value_type_t type = asdf_value_get_type(prop);
    switch (type) {
        case ASDF_VALUE_INT64: {
            time_t value_tmp = 0;
            asdf_value_as_int64(value, &value_tmp);
            snprintf(time->value, ASDF_TIME_TIMESTR_MAXLEN, "%ld", value_tmp);
        }
            break;
        case ASDF_VALUE_DOUBLE: {
            double value_tmp = 0.0;
            asdf_value_as_double(value, &value_tmp);
            snprintf(time->value, ASDF_TIME_TIMESTR_MAXLEN, "%lf", value_tmp);
        }
            break;
        case ASDF_VALUE_FLOAT: {
            float value_tmp = 0.0f;
            asdf_value_as_float(value, &value_tmp);
            snprintf(time->value, ASDF_TIME_TIMESTR_MAXLEN, "%f", value_tmp);
        }
            break;
        case ASDF_VALUE_STRING: {
            asdf_value_as_string0(prop, &value_s);
            strncpy(time->value, value_s, ASDF_TIME_TIMESTR_MAXLEN - 1);
        }
            break;
        default:
            fprintf(stderr, "unhandled property conversion from scalar enum %d\n", prop->type);
            goto failure;
    }

    if (prop != value) {
        asdf_value_destroy(prop);
    }

    if (asdf_value_is_mapping(value)) {
        prop = asdf_mapping_get(value, "format");
        if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &format_s)) {
            goto failure;
        }
        asdf_value_destroy(prop);
    }

    const char *time_auto_keys[] = {
        "iso_time",
        "byear",
        "jyear",
        "yday",
    };

    const char *time_auto_patterns[] = {
        // iso_time
        "[0-9]{4}-(0[1-9])|(1[0-2])-(0[1-9])|([1-2][0-9])|(3[0-1])[T ]([0-1][0-9])|(2[0-4]):[0-5][0-9]:[0-5][0-9](.[0-9]+)?",
        // byear
        "B[0-9]+(.[0-9]+)?",
        // jyear
        "J[0-9]+(.[0-9]+)?",
        // yday
        "[0-9]{4}:(00[1-9])|(0[1-9][0-9])|([1-2][0-9][0-9])|(3[0-5][0-9])|(36[0-5]):([0-1][0-9])|([0-1][0-9])|(2[0-4]):[0-5][0-9]:[0-5][0-9](.[0-9]+)?"
    };

    time->format.is_base_format = true;
    for (size_t i = 0; i < sizeof(time_auto_patterns) / sizeof(time_auto_patterns[0]); i++) {
        cregex re = cregex_make(time_auto_patterns[i], CREG_DEFAULT);
        if (cregex_is_match(&re, time->value) == true) {
            const char *fmt_have = format_s ? format_s : time_auto_keys[i];
            if (!strcmp(fmt_have, "iso_time")) {
                time->format.type = ASDF_TIME_FORMAT_ISO_TIME;
            } else if (!strcmp(fmt_have, "byear") || !strncmp(time->value, "B", 1)) {
                time->format.type = ASDF_TIME_FORMAT_BYEAR;
            } else if (!strcmp(fmt_have, "jd")) {
                time->format.type = ASDF_TIME_FORMAT_JD;
            } else if (!strcmp(fmt_have, "mjd")) {
                time->format.type = ASDF_TIME_FORMAT_MJD;
            } else if (!strcmp(fmt_have, "jyear") || !strncmp(time->value, "J", 1)) {
                time->format.type = ASDF_TIME_FORMAT_JYEAR;
            } else if (!strcmp(fmt_have, "yday")) {
                time->format.type = ASDF_TIME_FORMAT_YDAY;
            } else if (!strcmp(fmt_have, "unix")) {
                time->format.type = ASDF_TIME_FORMAT_UNIX;
            }
            time->scale = ASDF_TIME_SCALE_UTC;
            cregex_drop(&re);
            break;
        }
        cregex_drop(&re);
    }

    if (time->format.type > ASDF_TIME_FORMAT_RESERVED1) {
        time->format.is_base_format = false;
    }

    asdf_time_parse_time(time->value, &time->format, &time->info);

    *out = time;

    return ASDF_VALUE_OK;
failure:
    free(time->value);
    asdf_value_destroy(prop);
    return err;
}


static void asdf_time_dealloc(void *value) {
    asdf_time_t *t = (asdf_time_t *)value;
    free(t->value);
    free(t);
}


ASDF_REGISTER_EXTENSION(time, ASDF_STANDARD_TAG_PREFIX "time/time-1.4.0", asdf_time_t, &libasdf_software,
                        asdf_time_deserialize, asdf_time_dealloc, NULL);
