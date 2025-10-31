#include <libfyaml.h>
#include <stddef.h>
#include <stdint.h>

#include "munit.h"
#include "util.h"

#include <asdf/core/asdf.h>
#include <asdf/core/extension_metadata.h>
#include "asdf/core/time.h"
#include <asdf/core/history_entry.h>
#include <asdf/file.h>


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

        const char *key = fixture_key[i];
        assert_true(asdf_is_time(file, key));

        value = asdf_get_value(file, key);
        if (asdf_value_as_time(value, &t) != ASDF_VALUE_OK) {
            fprintf(stderr, "asdf_value_as_time failed: %s\n", key);
            asdf_time_destroy(t);
            return 1;
        };
        assert_true(t != NULL);
        assert_true(t->value != NULL);
        time_t x = t->info.ts.tv_sec;
        strftime(time_str, sizeof(time_str), "%m/%d/%Y %T %Z", gmtime(&x));
        printf("[%zu] key: %10s, value: %30s,  time: %10s\n", i, key, t->value, time_str);
        show_asdf_time_info(&t->info);

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
    MU_RUN_TEST(test_asdf_time)
);


MU_RUN_SUITE(test_asdf_time_extension);