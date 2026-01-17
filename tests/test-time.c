#include <libfyaml.h>
#include <stddef.h>
#include <stdint.h>
#include <math.h>

#include "munit.h"
#include "util.h"

#include <asdf/core/asdf.h>
#include <asdf/core/extension_metadata.h>
#include "asdf/core/time.h"
#include <asdf/core/history_entry.h>
#include <asdf/file.h>

enum {
    JANUARY = 1,
    FEBRUARY,
    MARCH,
    APRIL,
    MAY,
    JUNE,
    JULY,
    AUGUST,
    SEPTEMBER,
    OCTOBER,
    NOVEMBER,
    DECEMBER
};
#define DECL_TM(YEAR, MONTH, DAY, HOUR, MINUTE, SECOND) {.tm_year=(YEAR) - 1900, .tm_mon=(MONTH) - 1, .tm_mday=(DAY), .tm_hour=(HOUR), .tm_min=(MINUTE), .tm_sec=(SECOND)}

MU_TEST(test_tm_to_besselian) {
    const struct testcase {
        const char *name;
        struct tm tm;
        double expected;
    } tc[] = {
        // When UTC->TT conversion is implemented this should work correctly
        // For now our besselian is UTC, and the result is approximate (~1900.000511)
        //{"B1900.0", DECL_TM(1899, DECEMBER, 31, 11, 59, 28), 1900.0},
        {"B1900.0", DECL_TM(1900, JANUARY, 1, 0, 0, 0), 1900.0},
    };

    for (size_t i = 0; i < sizeof(tc) / sizeof(tc[0]); i++) {
        const double result = asdf_time_convert_tm_to_besselian(&tc[i].tm);
        munit_logf(MUNIT_LOG_DEBUG, "\n%s: %lf (got: %lf)\n", tc[i].name, tc[i].expected, result);
        assert_true(floor(fabs(result)) == floor(fabs(tc[i].expected)));
    }
    return MUNIT_OK;
}

MU_TEST(test_tm_to_julian) {
    const struct testcase {
        const char *name;
        struct tm tm;
        double expected;
    } tc[] = {
        {"Common Era", DECL_TM(0001, JANUARY, 1, 0, 0, 0), .expected = 1721425.5},
        {"Gregorian Reform Day", DECL_TM(1582, OCTOBER, 15, 0, 0, 0), .expected = 2299160.5},
        {"Day before Gregorian Reform", DECL_TM(1582, OCTOBER, 14, 0, 0, 0), .expected = 2299159.5},
        {"UNIX Epoch", DECL_TM(1970, JANUARY, 1, 0, 0, 0), .expected = 2440587.5},
        {"J1900", DECL_TM(1899, DECEMBER, 31, 12, 0, 0), .expected = 2415020.0},
        {"J2000", DECL_TM(2000, JANUARY, 1, 12, 0, 0), .expected = 2451545.0},
        {.name = "Leap year check", DECL_TM(2000, FEBRUARY, 29, 0, 0, 0), .expected = 2451603.5},
    };

    for (size_t i = 0; i < sizeof(tc) / sizeof(tc[0]); i++) {
        const double result = asdf_time_convert_tm_to_julian(&tc[i].tm);
        munit_logf(MUNIT_LOG_DEBUG, "\n%s: %lf (got: %lf)\n", tc[i].name, tc[i].expected, result);
        assert_true(result == tc[i].expected);
    }
    return MUNIT_OK;
}

MU_TEST(test_asdf_time) {
    const char *path = get_fixture_file_path("time.asdf");
    assert_not_null(path);

    asdf_file_t *file = asdf_open_file(path, "r");
    assert_not_null(file);

    asdf_value_t *value = NULL;

    // buffer for time string
    char time_str[255] = {0};
    const int format_type[] = {
        ASDF_TIME_FORMAT_ISO_TIME,
        ASDF_TIME_FORMAT_DATETIME,
        ASDF_TIME_FORMAT_YDAY,
        ASDF_TIME_FORMAT_UNIX,
        ASDF_TIME_FORMAT_JD,
        ASDF_TIME_FORMAT_MJD,
        ASDF_TIME_FORMAT_BYEAR,
    };

    asdf_time_t *t = NULL;
    for (size_t i = 0; i < sizeof(format_type) / sizeof(format_type[0]); i++) {
        const char *fixture_key[] = {
            "t_iso_time",
            "t_datetime",
            "t_yday",
            "t_unix",
            "t_jd",
            "t_mjd",
            "t_byear",
        };
        const char *time_str_expected = "10/14/2025 13:26:41 GMT";
        const char *key = fixture_key[i];
        assert_true(asdf_is_time(file, key));

        value = asdf_get_value(file, key);
        if (asdf_value_as_time(value, &t) != ASDF_VALUE_OK) {
            munit_logf(MUNIT_LOG_ERROR, "asdf_value_as_time failed: %s\n", key);
            asdf_time_destroy(t);
            return MUNIT_ERROR;
        };
        assert_true(t != NULL);
        assert_true(t->value != NULL);
        time_t x = t->info.ts.tv_sec;
        strftime(time_str, sizeof(time_str), "%m/%d/%Y %T %Z", gmtime(&x));
        munit_logf(MUNIT_LOG_DEBUG, "[%zu] key: %10s, value: %30s,  time: %10s", i, key, t->value, time_str);
        #ifndef NDEBUG
        asdf_time_info_dump(&t->info, stderr);
        #endif
        assert_true(strcmp(time_str, time_str_expected) == 0);

        asdf_time_destroy(t);
        t = NULL;
        memset(time_str, 0, sizeof(time_str));
        asdf_value_destroy(value);
    }

    asdf_close(file);

    return MUNIT_OK;
}

MU_TEST_SUITE(
    test_asdf_time_extension,
    MU_RUN_TEST(test_tm_to_besselian),
    MU_RUN_TEST(test_tm_to_julian),
    MU_RUN_TEST(test_asdf_time)
);


MU_RUN_SUITE(test_asdf_time_extension);