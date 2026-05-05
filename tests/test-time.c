#include <stddef.h>
#include <stdint.h>

#include "munit.h"
#include "util.h"

#include <asdf/core/asdf.h>
#include <asdf/core/time.h>
#include <asdf/file.h>
#include <asdf/value.h>


MU_TEST(test_asdf_time) {
    const char *path = get_fixture_file_path("time.asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open(path, "r");
    assert_not_null(file);

    asdf_value_t *value = NULL;

    /* buffer for formatted time string */
    char time_str[255] = {0};

    const char *fixture_keys[] = {
        "t_iso_time",
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
        .format = {.is_base_format = true, .type = ASDF_TIME_FORMAT_ISO_TIME},
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
    assert_int(t_out->format.type, ==, ASDF_TIME_FORMAT_ISO_TIME);

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
        asdf_time_base_format_t expected_format;
        const char *expected_value;
        bool check_ts;
    } cases[] = {
        /* bare scalars: format must be inferred from value string */
        {"t_iso_time_bare", ASDF_TIME_FORMAT_ISO_TIME,  "2025-10-14T13:26:41.0000", true},
        {"t_byear_bare",    ASDF_TIME_FORMAT_BYEAR,     "B2025.78707178",           true},
        {"t_jyear_bare",    ASDF_TIME_FORMAT_JYEAR,     "J2025.78707178",           false},
        {"t_yday_bare",     ASDF_TIME_FORMAT_YDAY ,     "2025:287:13:26:41.0000",   true},
        /* mapping without explicit format key */
        {"t_yday_map_no_format", ASDF_TIME_FORMAT_YDAY, "2025:287:13:26:41.0000",   true}
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
        assert_int(tm->format.type, ==, cases[idx].expected_format);

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
 * ``format.type`` enum value after deserialization.
 *
 * This test is expected to fail before the explicit-format lookup fix is
 * applied (the old code only set ``format.type`` when a regex pattern matched
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
        asdf_time_base_format_t expected_format;
    } cases[] = {
        {"t_iso_time", ASDF_TIME_FORMAT_ISO_TIME},
        {"t_datetime", ASDF_TIME_FORMAT_DATETIME},
        {"t_yday",     ASDF_TIME_FORMAT_YDAY},
        {"t_byear",    ASDF_TIME_FORMAT_BYEAR},
        {"t_unix",     ASDF_TIME_FORMAT_UNIX},
        {"t_jd",       ASDF_TIME_FORMAT_JD},
        {"t_mjd",      ASDF_TIME_FORMAT_MJD},
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
        assert_int(tm->format.type, ==, cases[idx].expected_format);

        asdf_time_destroy(tm);
        asdf_value_destroy(value);
    }

    asdf_close(file);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    test_asdf_time_extension,
    MU_RUN_TEST(test_asdf_time),
    MU_RUN_TEST(test_asdf_time_serialize),
    MU_RUN_TEST(test_asdf_time_format_detection),
    MU_RUN_TEST(test_asdf_time_explicit_format_types)
);


MU_RUN_SUITE(test_asdf_time_extension);
