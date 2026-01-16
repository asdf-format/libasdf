#include "munit.h"
#include "util.h"

#include "event.h"
#include "parser.h"


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


/** Helper for tests of variants of the file '255.asdf' */
static void test_255_parse_events(
    const char *filename, bool expect_tree, size_t expected_block_offset) {
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

    CHECK_NEXT_EVENT_TYPE(ASDF_BLOCK_INDEX_EVENT);
    asdf_block_index_t *block_index = event->payload.block_index;
    assert_not_null(block_index);
    assert_int(asdf_block_index_size(block_index), ==, 1);
    const off_t *index0 = asdf_block_index_at(block_index, 0);
    assert_int(*index0, ==, expected_block_offset);

    if (expect_tree) {
        CHECK_NEXT_EVENT_TYPE(ASDF_TREE_START_EVENT);
        CHECK_NEXT_EVENT_TYPE(ASDF_TREE_END_EVENT);
    }

    CHECK_NEXT_EVENT_TYPE(ASDF_BLOCK_EVENT);
    const asdf_block_info_t *block = asdf_event_block_info(event);
    assert_not_null(block);
    assert_int(block->header_pos, ==, expected_block_offset);
    assert_int(block->data_pos, ==, expected_block_offset + 54);
    // 718 - 664 == 54 ?? But recall, the header_size field of the block_header
    // does not include the block magic and the header_size field itself (6 bytes)
    assert_int(block->header.header_size, ==, 48);
    assert_int(block->header.flags, ==, 0);
    assert_memory_equal(4, block->header.compression, "\0\0\0\0");
    assert_int(block->header.allocated_size, ==, 256);
    assert_int(block->header.used_size, ==, 256);
    assert_int(block->header.data_size, ==, 256);
    char checksum[] = {
        '\xe2', '\xc8', '\x65', '\xdb', '\x41', '\x62', '\xbe', '\xd9',
        '\x63', '\xbf', '\xaa', '\x9e', '\xf6', '\xac', '\x18', '\xf0'};
    assert_memory_equal(16, block->header.checksum, checksum);

    CHECK_NEXT_EVENT_TYPE(ASDF_END_EVENT);

    // If we try to get further events asdf_event_iterate returns NULL
    assert_null(asdf_event_iterate(parser));
    asdf_parser_destroy(parser);
}


/** Parse a valid ASDF file containing just the header and a binary block but no YAML */
MU_TEST(parse_no_tree) {
    const char *filename = get_fixture_file_path("255-no-tree.asdf");
    test_255_parse_events(filename, false, 33);
    return MUNIT_OK;
}


MU_TEST(parse_padding_after_header) {
    const char *filename = get_fixture_file_path("255-padding-after-header.asdf");
    test_255_parse_events(filename, true, 921);
    return MUNIT_OK;
}


MU_TEST(parse_padding_after_tree) {
    const char *filename = get_fixture_file_path("255-padding-after-tree.asdf");
    test_255_parse_events(filename, true, 1002);
    return MUNIT_OK;
}


MU_TEST(parse_padding_no_newline_before_tree) {
    const char *filename = get_fixture_file_path("255-no-newline-before-tree.asdf");
    test_255_parse_events(filename, true, 920);
    return MUNIT_OK;
}


MU_TEST(parse_padding_no_tree_padding_after_header) {
    const char *filename = get_fixture_file_path("255-no-tree-padding-after-header.asdf");
    test_255_parse_events(filename, false, 44);
    return MUNIT_OK;
}


MU_TEST_SUITE(
    parse,
    MU_RUN_TEST(parse_minimal),
    MU_RUN_TEST(parse_minimal_extra_comment),
    MU_RUN_TEST(parse_no_tree),
    MU_RUN_TEST(parse_padding_after_header),
    MU_RUN_TEST(parse_padding_after_tree),
    MU_RUN_TEST(parse_padding_no_newline_before_tree),
    MU_RUN_TEST(parse_padding_no_tree_padding_after_header)
);


MU_RUN_SUITE(parse);
