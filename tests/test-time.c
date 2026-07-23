#include <stddef.h>
#include <stdint.h>

#include "munit.h"
#include "util.h"

#include "asdf/core/asdf.h"
#include "asdf/core/time.h"
#include "asdf/file.h"
#include "asdf/value.h"


MU_TEST(test_asdf_time) {
    const char *path = get_fixture_file_path("time.asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_value_t *value = NULL;

    /* buffer for formatted time string */
    char time_str[255] = {0};

    const char *fixture_keys[] = {
        "t_iso",
        "t_datetime",
        "t_yday",
        "t_unix",
        "t_jd",
        "t_mjd",
        "t_byear",
    };

    asdf_time_t *tm = NULL;

    for (size_t idx = 0; idx < sizeof(fixture_keys) / sizeof(fixture_keys[0]); idx++) {
        const char *key = fixture_keys[idx];
        assert_true(asdf_is_time(file, key));

        value = asdf_get_value(file, key);
        assert_not_null(value);

        asdf_value_err_t err = asdf_value_as_time(value, &tm);
        if (err != ASDF_VALUE_OK) {
            munit_logf(MUNIT_LOG_ERROR, "asdf_value_as_time failed: %s\n", key);
            asdf_time_destroy(tm);
            asdf_value_destroy(value);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        assert_not_null(tm);
        assert_not_null(tm->value);

        time_t tmt = tm->info.ts.tv_sec;
        strftime(time_str, sizeof(time_str), "%m/%d/%Y %T %Z", gmtime(&tmt));
        printf("[%zu] key: %10s, value: %30s,  time: %10s\n", idx, key, tm->value, time_str);

        asdf_time_destroy(tm);
        tm = NULL;
        memset(time_str, 0, sizeof(time_str));
        asdf_value_destroy(value);
        value = NULL;
    }

    asdf_close(file);

    return MUNIT_OK;
}


MU_TEST(test_asdf_time_serialize) {
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    assert_not_null(path);

    /* Write a time value to a new file */
    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);

    char time_value[] = "2025-10-14T13:26:41.0000";
    asdf_time_t time_obj = {
        .value = time_value,
        .format = ASDF_TIME_FORMAT_ISO,
        /* base_format left unset (zero-init), must read back as NONE */
        .scale = ASDF_TIME_SCALE_UTC,
    };

    asdf_value_err_t err = asdf_set_time(file, "t_write", &time_obj);
    assert_int(err, ==, ASDF_VALUE_OK);

    assert_int(asdf_write_to(file, path), ==, 0);
    asdf_close(file);

    /* Re-open and verify the round-trip */
    file = asdf_open(path, "r");
    assert_not_null(file);

    assert_true(asdf_is_time(file, "t_write"));

    asdf_time_t *t_out = NULL;
    err = asdf_get_time(file, "t_write", &t_out);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(t_out);
    assert_not_null(t_out->value);
    assert_string_equal(t_out->value, time_value);
    assert_int(t_out->format, ==, ASDF_TIME_FORMAT_ISO);
    /* base_format was NONE, so it should be omitted and read back as NONE */
    assert_int(t_out->base_format, ==, ASDF_TIME_FORMAT_NONE);

    asdf_time_destroy(t_out);
    asdf_close(file);

    return MUNIT_OK;
}


/**
 * Check that bare scalar time values (no explicit ``format`` key) have their
 * format auto-detected correctly, and that the parsed time info is sensible.
 * Also checks a mapping that has ``value`` but no ``format`` key.
 *
 * This test is expected to fail before the format-detection fixes are applied.
 */
MU_TEST(test_asdf_time_format_detection) {
    const char *path = get_fixture_file_path("time.asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    static const struct {
        const char *key;
        asdf_time_format_t expected_format;
        const char *expected_value;
        bool check_ts;
    } cases[] = {
        /* bare scalars: format must be inferred from value string */
        {"t_iso_bare", ASDF_TIME_FORMAT_ISO,  "2025-10-14T13:26:41.0000", true},
        {"t_byear_bare",    ASDF_TIME_FORMAT_BYEAR,     "B2025.78707178",           true},
        {"t_jyear_bare",    ASDF_TIME_FORMAT_JYEAR,     "J2025.78707178",           false},
        {"t_yday_bare",     ASDF_TIME_FORMAT_YDAY ,     "2025:287:13:26:41.0000",   true},
        /* mapping without explicit format key */
        {"t_yday_map_no_format", ASDF_TIME_FORMAT_YDAY, "2025:287:13:26:41.0000",   true},
        /* FITS long-year (signed, five-digit) is only auto-detected as fits;
         * an ordinary four-digit value would be guessed as iso instead */
        {"t_fits_long_bare", ASDF_TIME_FORMAT_FITS, "+12025-10-14T13:26:41.0000", true},
    };

    for (size_t idx = 0; idx < sizeof(cases) / sizeof(cases[0]); idx++) {
        const char *key = cases[idx].key;

        asdf_value_t *value = asdf_get_value(file, key);

        if (!value) {
            munit_logf(MUNIT_LOG_ERROR, "failed to get value at '%s'", key);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        asdf_time_t *tm = NULL;
        asdf_value_err_t err = asdf_value_as_time(value, &tm);

        if (err != ASDF_VALUE_OK) {
            munit_logf(MUNIT_LOG_ERROR, "asdf_value_as_time failed for '%s'", key);
            asdf_value_destroy(value);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        assert_not_null(tm);
        assert_not_null(tm->value);
        assert_string_equal(tm->value, cases[idx].expected_value);
        assert_int(tm->format, ==, cases[idx].expected_format);

        if (cases[idx].check_ts)
            assert_true(tm->info.ts.tv_sec > 0);

        asdf_time_destroy(tm);
        asdf_value_destroy(value);
    }

    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Check that time values with an explicit ``format`` key produce the correct
 * ``format` enum value after deserialization.
 *
 * This test is expected to fail before the explicit-format lookup fix is
 * applied (the old code only set ``format`` when a regex pattern matched
 * the value string, so formats like ``byear`` whose values lack a ``B`` prefix
 * were silently mis-classified).
 */
MU_TEST(test_asdf_time_explicit_format_types) {
    const char *path = get_fixture_file_path("time.asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    static const struct {
        const char *key;
        asdf_time_format_t expected_format;
    } cases[] = {
        {"t_iso", ASDF_TIME_FORMAT_ISO},
        {"t_isot",     ASDF_TIME_FORMAT_ISOT},
        {"t_datetime", ASDF_TIME_FORMAT_DATETIME},
        {"t_yday",     ASDF_TIME_FORMAT_YDAY},
        {"t_byear",    ASDF_TIME_FORMAT_BYEAR},
        {"t_unix",     ASDF_TIME_FORMAT_UNIX},
        {"t_jd",       ASDF_TIME_FORMAT_JD},
        {"t_mjd",      ASDF_TIME_FORMAT_MJD},
        {"t_fits",     ASDF_TIME_FORMAT_FITS},
        {"t_fits_long", ASDF_TIME_FORMAT_FITS},
    };

    for (size_t idx = 0; idx < sizeof(cases) / sizeof(cases[0]); idx++) {
        const char *key = cases[idx].key;

        asdf_value_t *value = asdf_get_value(file, key);
        if (!value) {
            munit_logf(MUNIT_LOG_ERROR, "failed to get value at '%s'", key);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        asdf_time_t *tm = NULL;
        asdf_value_err_t err = asdf_value_as_time(value, &tm);

        if (err != ASDF_VALUE_OK) {
            munit_logf(MUNIT_LOG_ERROR, "asdf_value_as_time failed for '%s'", key);
            asdf_value_destroy(value);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        assert_not_null(tm);
        assert_int(tm->format, ==, cases[idx].expected_format);

        asdf_time_destroy(tm);
        asdf_value_destroy(value);
    }

    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Check that ``jyear`` and ``decimalyear`` formats parse to a sensible
 * timestamp, and that a numeric (unquoted) value is captured verbatim from the
 * YAML rather than being re-stringified (which would lose precision).
 */
MU_TEST(test_asdf_time_jyear_decimalyear) {
    const char *path = get_fixture_file_path("time.asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    static const struct {
        const char *key;
        asdf_time_format_t expected_format;
        const char *expected_value;
        int expected_year;
    } cases[] = {
        /* string form: explicit J prefix is required for a string jyear */
        {"t_jyear",       ASDF_TIME_FORMAT_JYEAR,       "J2025.78707178", 2025},
        /* numeric form: a bare number is valid for jyear/decimalyear and the
         * verbatim scalar text is captured at full precision */
        {"t_jyear_num",   ASDF_TIME_FORMAT_JYEAR,       "1948.78707178",  1948},
        {"t_decimalyear", ASDF_TIME_FORMAT_DECIMALYEAR, "2025.5",         2025},
    };

    for (size_t idx = 0; idx < sizeof(cases) / sizeof(cases[0]); idx++) {
        const char *key = cases[idx].key;

        asdf_value_t *value = asdf_get_value(file, key);
        if (!value) {
            munit_logf(MUNIT_LOG_ERROR, "failed to get value at '%s'", key);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        asdf_time_t *tm = NULL;
        asdf_value_err_t err = asdf_value_as_time(value, &tm);

        if (err != ASDF_VALUE_OK) {
            munit_logf(MUNIT_LOG_ERROR, "asdf_value_as_time failed for '%s'", key);
            asdf_value_destroy(value);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        assert_not_null(tm);
        assert_not_null(tm->value);
        /* The numeric value must be captured verbatim. */
        assert_string_equal(tm->value, cases[idx].expected_value);
        assert_int(tm->format, ==, cases[idx].expected_format);
        assert_int(tm->info.tm.tm_year + 1900, ==, cases[idx].expected_year);

        asdf_time_destroy(tm);
        asdf_value_destroy(value);
    }

    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Check the ``plot_date`` format (matplotlib ordinal days from an 0001-01-01
 * UTC epoch): the numeric value is captured verbatim and the epoch offset
 * resolves to the expected calendar instant.  739538.560197 is the same instant
 * as the ``t_jd`` fixture (JD 2460963.060197), i.e. 2025-10-14 13:26:41 UTC.
 */
MU_TEST(test_asdf_time_plot_date) {
    const char *path = get_fixture_file_path("time.asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_value_t *value = asdf_get_value(file, "t_plot_date");
    assert_not_null(value);

    asdf_time_t *tm = NULL;
    asdf_value_err_t err = asdf_value_as_time(value, &tm);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(tm);

    assert_string_equal(tm->value, "739538.560197");
    assert_int(tm->format, ==, ASDF_TIME_FORMAT_PLOT_DATE);
    assert_int(tm->info.tm.tm_year + 1900, ==, 2025);
    assert_int(tm->info.tm.tm_mon + 1, ==, 10);
    assert_int(tm->info.tm.tm_mday, ==, 14);
    assert_int(tm->info.tm.tm_hour, ==, 13);
    assert_int(tm->info.tm.tm_min, ==, 26);

    asdf_time_destroy(tm);
    asdf_value_destroy(value);
    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Check that the optional ``scale`` mapping key is parsed (and defaults to UTC
 * when absent).
 */
MU_TEST(test_asdf_time_scale) {
    const char *path = get_fixture_file_path("time.asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    static const struct {
        const char *key;
        asdf_time_scale_t expected_scale;
    } cases[] = {
        {"t_iso_tai", ASDF_TIME_SCALE_TAI},
        {"t_iso",     ASDF_TIME_SCALE_UTC},
    };

    for (size_t idx = 0; idx < sizeof(cases) / sizeof(cases[0]); idx++) {
        const char *key = cases[idx].key;

        asdf_value_t *value = asdf_get_value(file, key);
        if (!value) {
            munit_logf(MUNIT_LOG_ERROR, "failed to get value at '%s'", key);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        asdf_time_t *tm = NULL;
        asdf_value_err_t err = asdf_value_as_time(value, &tm);

        if (err != ASDF_VALUE_OK) {
            munit_logf(MUNIT_LOG_ERROR, "asdf_value_as_time failed for '%s'", key);
            asdf_value_destroy(value);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        assert_not_null(tm);
        assert_int(tm->scale, ==, cases[idx].expected_scale);

        asdf_time_destroy(tm);
        asdf_value_destroy(value);
    }

    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Round-trip a non-UTC ``scale`` through serialize + deserialize to confirm the
 * scale is both written and read back correctly.
 */
MU_TEST(test_asdf_time_scale_roundtrip) {
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);

    char time_value[] = "2025-10-14T13:26:41.0000";
    asdf_time_t time_obj = {
        .value = time_value,
        .format = ASDF_TIME_FORMAT_ISO,
        .scale = ASDF_TIME_SCALE_TAI,
    };

    asdf_value_err_t err = asdf_set_time(file, "t_scale", &time_obj);
    assert_int(err, ==, ASDF_VALUE_OK);

    assert_int(asdf_write_to(file, path), ==, 0);
    asdf_close(file);

    file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_time_t *t_out = NULL;
    err = asdf_get_time(file, "t_scale", &t_out);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(t_out);
    assert_int(t_out->scale, ==, ASDF_TIME_SCALE_TAI);

    asdf_time_destroy(t_out);
    asdf_close(file);

    return MUNIT_OK;
}


/**
 * Check that the optional ``base_format`` mapping key is parsed (to the matching
 * format enum when present, and ASDF_TIME_FORMAT_NONE when absent).
 */
MU_TEST(test_asdf_time_base_format) {
    const char *path = get_fixture_file_path("time.asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    static const struct {
        const char *key;
        asdf_time_format_t expected_base_format;
    } cases[] = {
        {"t_iso_base_format", ASDF_TIME_FORMAT_DATETIME},
        {"t_iso", ASDF_TIME_FORMAT_NONE},
    };

    for (size_t idx = 0; idx < sizeof(cases) / sizeof(cases[0]); idx++) {
        const char *key = cases[idx].key;

        asdf_value_t *value = asdf_get_value(file, key);
        if (!value) {
            munit_logf(MUNIT_LOG_ERROR, "failed to get value at '%s'", key);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        asdf_time_t *tm = NULL;
        asdf_value_err_t err = asdf_value_as_time(value, &tm);

        if (err != ASDF_VALUE_OK) {
            munit_logf(MUNIT_LOG_ERROR, "asdf_value_as_time failed for '%s'", key);
            asdf_value_destroy(value);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        assert_not_null(tm);
        assert_int(tm->base_format, ==, cases[idx].expected_base_format);

        asdf_time_destroy(tm);
        asdf_value_destroy(value);
    }

    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Round-trip a ``base_format`` through serialize + deserialize to confirm it is
 * both written and read back correctly.
 */
MU_TEST(test_asdf_time_base_format_roundtrip) {
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);

    char time_value[] = "2025-10-14T13:26:41.0000";
    asdf_time_t time_obj = {
        .value = time_value,
        .format = ASDF_TIME_FORMAT_ISO,
        .base_format = ASDF_TIME_FORMAT_DATETIME,
        .scale = ASDF_TIME_SCALE_UTC,
    };

    asdf_value_err_t err = asdf_set_time(file, "t_base", &time_obj);
    assert_int(err, ==, ASDF_VALUE_OK);

    assert_int(asdf_write_to(file, path), ==, 0);
    asdf_close(file);

    file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_time_t *t_out = NULL;
    err = asdf_get_time(file, "t_base", &t_out);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(t_out);
    assert_int(t_out->base_format, ==, ASDF_TIME_FORMAT_DATETIME);

    asdf_time_destroy(t_out);
    asdf_close(file);

    return MUNIT_OK;
}


/**
 * Round-trip a ``fits`` time with the FITS "long" year form (an explicit sign
 * plus five digits) to confirm the value string and format are preserved.
 */
MU_TEST(test_asdf_time_fits_roundtrip) {
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);

    char time_value[] = "+12025-10-14T13:26:41.0000";
    asdf_time_t time_obj = {
        .value = time_value,
        .format = ASDF_TIME_FORMAT_FITS,
        .scale = ASDF_TIME_SCALE_UTC,
    };

    asdf_value_err_t err = asdf_set_time(file, "t_fits", &time_obj);
    assert_int(err, ==, ASDF_VALUE_OK);

    assert_int(asdf_write_to(file, path), ==, 0);
    asdf_close(file);

    file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_time_t *t_out = NULL;
    err = asdf_get_time(file, "t_fits", &t_out);
    assert_int(err, ==, ASDF_VALUE_OK);
    assert_not_null(t_out);
    assert_int(t_out->format, ==, ASDF_TIME_FORMAT_FITS);
    assert_string_equal(t_out->value, "+12025-10-14T13:26:41.0000");

    asdf_time_destroy(t_out);
    asdf_close(file);

    return MUNIT_OK;
}


/**
 * Check that time values tagged with older schema versions (time-1.0.0,
 * time-1.1.0) are still recognized and deserialized.
 */
MU_TEST(test_asdf_time_versions) {
    const char *path = get_fixture_file_path("time.asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    static const char *const keys[] = {
        "t_iso_1_1_0",
        "t_iso_1_0_0",
    };

    for (size_t idx = 0; idx < sizeof(keys) / sizeof(keys[0]); idx++) {
        const char *key = keys[idx];
        assert_true(asdf_is_time(file, key));

        asdf_value_t *value = asdf_get_value(file, key);
        if (!value) {
            munit_logf(MUNIT_LOG_ERROR, "failed to get value at '%s'", key);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        asdf_time_t *tm = NULL;
        asdf_value_err_t err = asdf_value_as_time(value, &tm);

        if (err != ASDF_VALUE_OK) {
            munit_logf(MUNIT_LOG_ERROR, "asdf_value_as_time failed for '%s'", key);
            asdf_value_destroy(value);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        assert_not_null(tm);
        assert_int(tm->format, ==, ASDF_TIME_FORMAT_ISO);

        asdf_time_destroy(tm);
        asdf_value_destroy(value);
    }

    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Check the jyear_str / byear_str formats: they parse identically to jyear /
 * byear (from a J- / B-prefixed string), and are rejected for a numeric value.
 */
MU_TEST(test_asdf_time_jyear_byear_str) {
    const char *path = get_fixture_file_path("time.asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    static const struct {
        const char *key;
        asdf_time_format_t expected_format;
        int expected_year;
    } cases[] = {
        {"t_jyear_str", ASDF_TIME_FORMAT_JYEAR_STR, 2025},
        {"t_byear_str", ASDF_TIME_FORMAT_BYEAR_STR, 2025},
    };

    for (size_t idx = 0; idx < sizeof(cases) / sizeof(cases[0]); idx++) {
        const char *key = cases[idx].key;

        asdf_value_t *value = asdf_get_value(file, key);
        if (!value) {
            munit_logf(MUNIT_LOG_ERROR, "failed to get value at '%s'", key);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        asdf_time_t *tm = NULL;
        asdf_value_err_t err = asdf_value_as_time(value, &tm);

        if (err != ASDF_VALUE_OK) {
            munit_logf(MUNIT_LOG_ERROR, "asdf_value_as_time failed for '%s'", key);
            asdf_value_destroy(value);
            asdf_close(file);
            return MUNIT_FAIL;
        }

        assert_not_null(tm);
        assert_int(tm->format, ==, cases[idx].expected_format);
        assert_int(tm->info.tm.tm_year + 1900, ==, cases[idx].expected_year);

        asdf_time_destroy(tm);
        asdf_value_destroy(value);
    }

    /* A numeric value for jyear_str must be rejected. */
    asdf_value_t *bad = asdf_get_value(file, "t_jyear_str_bad");
    assert_not_null(bad);
    asdf_time_t *tm_bad = NULL;
    assert_int(asdf_value_as_time(bad, &tm_bad), !=, ASDF_VALUE_OK);
    asdf_value_destroy(bad);

    asdf_close(file);
    return MUNIT_OK;
}


/**
 * Astropy-compatibility: a J-prefixed (Julian year) or B-prefixed (Besselian
 * year) string value is serialized with the jyear_str / byear_str format even
 * when stored as plain jyear / byear.
 */
MU_TEST(test_asdf_time_jyear_byear_str_serialize) {
    const char *path = get_temp_file_path(fixture->tempfile_prefix, ".asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open(NULL);
    assert_not_null(file);

    char jvalue[] = "J2000.0";
    asdf_time_t jtime = {.value = jvalue, .format = ASDF_TIME_FORMAT_JYEAR};
    assert_int(asdf_set_time(file, "t_jy", &jtime), ==, ASDF_VALUE_OK);

    char bvalue[] = "B1950.0";
    asdf_time_t btime = {.value = bvalue, .format = ASDF_TIME_FORMAT_BYEAR};
    assert_int(asdf_set_time(file, "t_by", &btime), ==, ASDF_VALUE_OK);

    assert_int(asdf_write_to(file, path), ==, 0);
    asdf_close(file);

    file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_time_t *t_out = NULL;
    assert_int(asdf_get_time(file, "t_jy", &t_out), ==, ASDF_VALUE_OK);
    assert_not_null(t_out);
    /* jyear + "J..." was written as jyear_str, so it reads back as jyear_str */
    assert_int(t_out->format, ==, ASDF_TIME_FORMAT_JYEAR_STR);
    assert_string_equal(t_out->value, jvalue);
    asdf_time_destroy(t_out);

    t_out = NULL;
    assert_int(asdf_get_time(file, "t_by", &t_out), ==, ASDF_VALUE_OK);
    assert_not_null(t_out);
    assert_int(t_out->format, ==, ASDF_TIME_FORMAT_BYEAR_STR);
    assert_string_equal(t_out->value, bvalue);
    asdf_time_destroy(t_out);

    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_time_extension,
    MU_RUN_TEST(test_asdf_time),
    MU_RUN_TEST(test_asdf_time_serialize),
    MU_RUN_TEST(test_asdf_time_format_detection),
    MU_RUN_TEST(test_asdf_time_explicit_format_types),
    MU_RUN_TEST(test_asdf_time_jyear_decimalyear),
    MU_RUN_TEST(test_asdf_time_plot_date),
    MU_RUN_TEST(test_asdf_time_scale),
    MU_RUN_TEST(test_asdf_time_scale_roundtrip),
    MU_RUN_TEST(test_asdf_time_base_format),
    MU_RUN_TEST(test_asdf_time_base_format_roundtrip),
    MU_RUN_TEST(test_asdf_time_fits_roundtrip),
    MU_RUN_TEST(test_asdf_time_versions),
    MU_RUN_TEST(test_asdf_time_jyear_byear_str),
    MU_RUN_TEST(test_asdf_time_jyear_byear_str_serialize)
);


MU_RUN_SUITE(test_asdf_time_extension);
