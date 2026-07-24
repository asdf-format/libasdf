/**
 * Data type and extension for the ``stsci.edu/schemas/asdf/time/time`` schema
 */

//

#ifndef ASDF_CORE_TIME_H
#define ASDF_CORE_TIME_H

#include <sys/time.h>
#include <time.h>

#include <asdf/extension.h>


ASDF_BEGIN_DECLS

/** .. _time-types:
 *
 * Types
 * -----
 */

/**
 * The representation format of a time.
 *
 * These correspond to the astropy time formats and the schema's ``format`` /
 * ``base_format`` enums.  The values from ``ASDF_TIME_FORMAT_BYEAR_STR`` onward
 * are the schema's ``other_format`` values, which are valid only in
 * ``base_format`` on the wire; see `asdf_time_t` ``format`` for how the two
 * collapse into a single effective format.
 */
typedef enum {
    /** ISO 8601 date-time, ``YYYY-MM-DDTHH:MM:SS.sss...`` (the default) */
    ASDF_TIME_FORMAT_ISO = 0,
    /** Year, day-of-year and time, ``YYYY:DOY:HH:MM:SS.sss...`` */
    ASDF_TIME_FORMAT_YDAY,
    /** Besselian epoch year, e.g. ``B1950.0`` */
    ASDF_TIME_FORMAT_BYEAR,
    /** Julian epoch year, e.g. ``J2000.0`` */
    ASDF_TIME_FORMAT_JYEAR,
    /** Decimal year, integer values being the start of the year */
    ASDF_TIME_FORMAT_DECIMALYEAR,
    /** Julian Date: days since the start of the Julian period */
    ASDF_TIME_FORMAT_JD,
    /** Modified Julian Date: days since 1858-11-17 00:00 */
    ASDF_TIME_FORMAT_MJD,
    /** GPS time: seconds from 1980-01-06 00:00:00 UTC */
    ASDF_TIME_FORMAT_GPS,
    /** Unix time: seconds from 1970-01-01 00:00:00 UTC, ignoring leap seconds */
    ASDF_TIME_FORMAT_UNIX,
    /** UT seconds from 1979-01-01 00:00:00 UTC, ignoring leap seconds */
    ASDF_TIME_FORMAT_UTIME,
    /** SI seconds from 1958-01-01 00:00:00, including leap seconds (TAI) */
    ASDF_TIME_FORMAT_TAI_SECONDS,
    /** Chandra X-ray Center seconds from 1998-01-01 00:00:00 TT */
    ASDF_TIME_FORMAT_CXCSEC,
    /** GALEX time: seconds from 1980-01-06 00:00:00 UTC */
    ASDF_TIME_FORMAT_GALEXSEC,
    /** SI seconds from 1970-01-01 00:00:00 TAI */
    ASDF_TIME_FORMAT_UNIX_TAI,
    /** Reserved; not a usable format */
    ASDF_TIME_FORMAT_RESERVED1,
    /* The schema's ``other_format`` values follow (see the note above). */
    /** Besselian epoch string form, e.g. ``B1950.0`` */
    ASDF_TIME_FORMAT_BYEAR_STR,
    /** A Python ``datetime.datetime`` (naive or timezone-aware) */
    ASDF_TIME_FORMAT_DATETIME,
    /** FITS date-time string; permits a signed five-digit "long" year */
    ASDF_TIME_FORMAT_FITS,
    /** ISO 8601 with a literal ``T`` date/time separator */
    ASDF_TIME_FORMAT_ISOT,
    /** Julian epoch string form, e.g. ``J2000.0`` */
    ASDF_TIME_FORMAT_JYEAR_STR,
    /** matplotlib ordinal: days from 0001-01-01 00:00:00 UTC plus one */
    ASDF_TIME_FORMAT_PLOT_DATE,
    /** Year/month/day/hour/minute/second fields */
    ASDF_TIME_FORMAT_YMDHMS,
    /** NumPy ``datetime64`` */
    ASDF_TIME_FORMAT_DATETIME64,
} asdf_time_format_t;


/** The time scale (time standard), as in the schema's ``scale`` field. */
typedef enum {
    /** Coordinated Universal Time (the default scale) */
    ASDF_TIME_SCALE_UTC = 0,
    /** International Atomic Time */
    ASDF_TIME_SCALE_TAI,
    /** Barycentric Coordinate Time */
    ASDF_TIME_SCALE_TCB,
    /** Geocentric Coordinate Time */
    ASDF_TIME_SCALE_TCG,
    /** Barycentric Dynamical Time */
    ASDF_TIME_SCALE_TDB,
    /** Terrestrial Time */
    ASDF_TIME_SCALE_TT,
    /** Universal Time (UT1) */
    ASDF_TIME_SCALE_UT1,
} asdf_time_scale_t;

/** Observer location, used by location-sensitive scales such as ``tdb``. */
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
 *   offsets.
 *
 *   The computed calendar reading is therefore in the format's own timescale,
 *   off from UTC by the relevant offset.  The ``tdb``, ``ut1``, ``tcb`` and
 *   ``tcg`` scales need still more external data and are likewise approximate.
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

/**
 * A single instant in time, as read from or written to a ``time/time`` tag.
 *
 * The instant is defined by `value` together with `format` and `scale`; `info`
 * is a derived, best-effort calendar representation for convenience.
 */
typedef struct {
    /** The time value, exactly as it appears (or will appear) in the file. */
    char *value;
    /**
     * Derived calendar representation; best-effort and, for non-UTC scales,
     * approximate.  See `asdf_time_info_t` for the leap-second caveat.
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
    /** Observer location; used only by location-sensitive scales (e.g. ``tdb``). */
    asdf_time_location_t location;
} asdf_time_t;

ASDF_DECLARE_EXTENSION(time, asdf_time_t);


/** .. _time-accessors:
 *
 * Accessors
 * ---------
 */


// clang-format off

/**
 * .. c:function:: bool asdf_is_time(asdf_file_t *file, const char *path)
 *
 *   Test whether the value at ``path`` in the tree is a ``time/time`` object
 *
 *   :param file: The `asdf_file_t *` for the file
 *   :param path: The :ref:`yaml-pointer` to the value
 *   :return: ``true`` if the value exists and is a time, otherwise ``false``
 */


/**
 * .. c:function:: asdf_value_err_t asdf_get_time(asdf_file_t *file, const char *path, asdf_time_t **out)
 *
 *   Get an `asdf_time_t *` out of the ASDF tree
 *
 *   The returned object is owned by the caller and must be freed with
 *   `asdf_time_destroy`.
 *
 *   :param file: The `asdf_file_t *` for the file
 *   :param path: The :ref:`yaml-pointer` to the time
 *   :param out: An `asdf_time_t **` into which the `asdf_time_t *` is returned
 *   :return: `ASDF_VALUE_OK` if the value exists and is a time, otherwise
 *     `ASDF_VALUE_ERR_NOT_FOUND` or `ASDF_VALUE_ERR_TYPE_MISMATCH`.
 */


/**
 * .. c:function:: asdf_value_err_t asdf_set_time(asdf_file_t *file, const char *path, const asdf_time_t *time)
 *
 *   Store an `asdf_time_t *` at a path in the ASDF tree
 *
 *   Only the ``value``, ``format``, ``scale`` and ``location`` fields need be
 *   populated; the derived `asdf_time_info_t` ``info`` is not required for
 *   writing.
 *
 *   :param file: The `asdf_file_t *` for the file
 *   :param path: The :ref:`yaml-pointer` for the time
 *   :param time: An `asdf_time_t *` to store
 *   :return: `ASDF_VALUE_OK` on success, otherwise an error code
 */


/**
 * .. c:function:: bool asdf_value_is_time(asdf_value_t *value)
 *
 *   Test whether a generic `asdf_value_t *` is a ``time/time`` object
 *
 *   :param value: The `asdf_value_t *` handle
 *   :return: ``true`` if ``value`` is a time, otherwise ``false``
 */


/**
 * .. c:function:: asdf_value_err_t asdf_value_as_time(asdf_value_t *value, asdf_time_t **out)
 *
 *   Interpret a generic `asdf_value_t *` as a time value, if possible
 *
 *   The returned object is owned by the caller and must be freed with
 *   `asdf_time_destroy`.
 *
 *   :param value: The `asdf_value_t *` handle
 *   :param out: An `asdf_time_t **` into which the `asdf_time_t *` is returned
 *   :return: `ASDF_VALUE_OK` if ``value`` is a time, otherwise
 *     `ASDF_VALUE_ERR_TYPE_MISMATCH`.
 */


/**
 * .. c:function:: void asdf_time_destroy(asdf_time_t *time)
 *
 *   Free an `asdf_time_t *` returned by `asdf_get_time` or `asdf_value_as_time`
 *
 *   :param time: The `asdf_time_t *` to free
 */

// clang-format on


/** .. _time-parsing:
 *
 * Parsing and formatting
 * ----------------------
 */


/**
 * Parse `time`'s ``value`` (according to its ``format``) into the derived
 * `asdf_time_info_t` ``info`` fields.
 *
 * This is called automatically during deserialization; it may be called again
 * after changing ``value`` or ``format``.  See `asdf_time_info_t` for the
 * accuracy caveat on non-UTC scales.
 *
 * :param time: The time to parse; its ``info`` is filled in on success.
 * :return: ``0`` on success, or ``-1`` on failure (e.g. an unsupported or
 *     unparseable ``value``).
 */
ASDF_EXPORT int asdf_time_parse(asdf_time_t *time);

/**
 * Return the schema string name of a time format (e.g. ``"iso"``, ``"jd"``).
 *
 * :param format: A time format.
 * :return: The format's name, or ``NULL`` if `format` is out of range or has no
 *     string representation.
 */
ASDF_EXPORT const char *asdf_time_format_string(asdf_time_format_t format);


/** .. _time-defines:
 *
 * Defines
 * -------
 */

/** Tag URI prefix for time objects; a version is appended (e.g. ``"1.4.0"``). */
#define ASDF_CORE_TIME_TAG_BASE "tag:stsci.edu:asdf/time/time-"

/**
 * The time tag written by libasdf, and the newest version it reads.
 *
 * Older versions (``ASDF_CORE_TIME_TAG_BASE`` ``"1.x.0"``) are also recognized
 * when reading; see the ``ASDF_REGISTER_EXTENSION`` call in ``time.c``.
 */
#define ASDF_CORE_TIME_TAG ASDF_CORE_TIME_TAG_BASE "1.4.0"

/** Maximum length in bytes of a time `asdf_time_t` ``value`` string. */
#define ASDF_TIME_TIMESTR_MAXLEN 255

ASDF_END_DECLS

#endif /* ASDF_CORE_TIME_H */
