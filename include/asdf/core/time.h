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
    /** The default format is ISO */
    ASDF_TIME_FORMAT_ISO = 0,
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

/**
 * Best-effort calendar representation of a parsed time
 *
 * These fields are *computed* from ``value`` / ``format`` / ``scale`` purely
 * for convenience.  The authoritative instant is always the ``value``,
 * ``format`` and ``scale``, which libasdf preserves verbatim and round-trips
 * losslessly; only this derived representation is approximate.
 *
 * .. warning::
 *
 *   For any time not on the UTC scale, i.e. ``scale`` other than
 *   `ASDF_TIME_SCALE_UTC`, and the atomic-scale formats ``gps``, ``unix_tai``,
 *   ``cxcsec`` and ``tai_seconds``, these fields ignore leap seconds.
 *   libasdf has no leap-second table, so it does not apply the TAI/TT-to-UTC
 *   offsets
 *
 *   The computed calendar reading is therefore the in the format's own
 *   timescale, off from UTC by the relevant offset.  The ``tdb``, ``ut1``,
 *   ``tcb`` and ``tcg`` scales need still more external data and are likewise
 *   approximate.
 *
 * Consumers needing an exact UTC instant for a non-UTC-scale time should
 * convert the raw ``value`` / ``format`` / ``scale`` themselves (e.g. via ERFA
 * or astropy).
 */
typedef struct {
    /** Seconds + nanoseconds from the Unix epoch (approximate; see above) */
    struct timespec ts;
    /** Derived calendar fields (approximate; see above) */
    struct tm tm;
} asdf_time_info_t;

typedef struct {
    char *value;
    /**
     * Derived calendar representation; best-effort and, for non-UTC scales,
     * approximate.  See asdf_time_info_t for the leap-second caveat.
     */
    asdf_time_info_t info;
    /**
     * The effective (real) format of the time.
     *
     * This may be any format, including one of the schema's ``other_format``
     * values (e.g. ``fits``, ``isot``, ``plot_date``) which the schema only
     * permits in the ``base_format`` field on the wire.  On deserialization the
     * ``format`` and ``base_format`` keys are collapsed into this single
     * effective format (``base_format`` overrides ``format`` when present); on
     * serialization the wire ``format`` / ``base_format`` split is derived back
     * from it.
     */
    asdf_time_format_t format;
    /**
     * The time scale.  A non-UTC scale means the derived ``info`` fields are
     * approximate (leap seconds are not applied); see asdf_time_info_t.
     */
    asdf_time_scale_t scale;
    asdf_time_location_t location;
} asdf_time_t;

ASDF_DECLARE_EXTENSION(time, asdf_time_t);

ASDF_EXPORT int asdf_time_parse(asdf_time_t *time);
ASDF_EXPORT const char *asdf_time_format_string(asdf_time_format_t format);

ASDF_END_DECLS

#endif /* ASDF_CORE_TIME_H */
