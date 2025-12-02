#include "munit.h"
#include "util.h"

#include "event.h"
#include "parse.h"
#include "yaml.h"


#define CHECK_NEXT_EVENT_TYPE(type) do { \
    assert_not_null((event = asdf_event_iterate(parser))); \
    assert_int(asdf_event_type(event), ==, (type)); \
} while (0)


/**
 * Test parsing the absolute bare minimum ASDF file that can be parsed without errors
 *
 * It just consists of the #ASDF and #ASDF_STANDARD comments, and nothing more
 */
MU_TEST(parse_minimal) {
    const char *filename = get_fixture_file_path("parse-minimal.asdf");

    asdf_parser_t *parser = asdf_parser_create(NULL);
    asdf_event_t *event = NULL;

    if (!parser)
        munit_error("failed to initialize asdf parser");

    if (asdf_parser_set_input_file(parser, filename) != 0)
        munit_errorf("failed to set asdf parser file '%s'", filename);

    CHECK_NEXT_EVENT_TYPE(ASDF_ASDF_VERSION_EVENT);
    assert_string_equal(event->payload.version->version, "1.0.0");

    CHECK_NEXT_EVENT_TYPE(ASDF_STANDARD_VERSION_EVENT);
    assert_string_equal(event->payload.version->version, "1.6.0");

    CHECK_NEXT_EVENT_TYPE(ASDF_END_EVENT);

    // If we try to get further events asdf_event_iterate returns NULL
    assert_null(asdf_event_iterate(parser));
    asdf_parser_destroy(parser);
    return MUNIT_OK;
}


/**
 * Like `parse_minimal` but with an additional non-standard comment event
 */
MU_TEST(parse_minimal_extra_comment) {
    const char *filename = get_fixture_file_path("parse-minimal-extra-comment.asdf");

    asdf_parser_t *parser = asdf_parser_create(NULL);
    asdf_event_t *event = NULL;

    if (!parser)
        munit_error("failed to initialize asdf parser");

    if (asdf_parser_set_input_file(parser, filename) != 0)
        munit_errorf("failed to set asdf parser file '%s'", filename);

    CHECK_NEXT_EVENT_TYPE(ASDF_ASDF_VERSION_EVENT);
    assert_string_equal(event->payload.version->version, "1.0.0");

    CHECK_NEXT_EVENT_TYPE(ASDF_STANDARD_VERSION_EVENT);
    assert_string_equal(event->payload.version->version, "1.6.0");

    CHECK_NEXT_EVENT_TYPE(ASDF_COMMENT_EVENT);
    assert_string_equal(asdf_event_comment(event), "NONSTANDARD HEADER COMMENT");

    CHECK_NEXT_EVENT_TYPE(ASDF_END_EVENT);

    // If we try to get further events asdf_event_iterate returns NULL
    assert_null(asdf_event_iterate(parser));
    asdf_parser_destroy(parser);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    parse,
    MU_RUN_TEST(parse_minimal),
    MU_RUN_TEST(parse_minimal_extra_comment)
);


MU_RUN_SUITE(parse);
