#include <string.h>

#include <asdf/event.h>
#include <asdf/parser.h>
#include <asdf/yaml.h>

#include "munit.h"
#include "util.h"

#include "block.h"
#include "event.h"
#include "parser.h"


/**
 * Helper macros for checking ASDF and YAML events
 */
#define CHECK_NEXT_EVENT_TYPE(type) do { \
    assert_not_null((event = asdf_event_iterate(parser))); \
    assert_int(asdf_event_type(event), ==, (type)); \
} while (0)


#define __CHECK_NEXT_YAML_EVENT_1(type) do { \
    CHECK_NEXT_EVENT_TYPE(ASDF_YAML_EVENT); \
    assert_int(asdf_yaml_event_type(event), ==, (type)); \
} while (0)


#define __CHECK_NEXT_YAML_EVENT_2(type, tag) do { \
    __CHECK_NEXT_YAML_EVENT_1(type); \
    size_t __len = 0; \
    const char *__tag = asdf_yaml_event_tag(event, &__len); \
    if ((tag) == NULL) { \
        assert_int(__len, ==, 0); \
    } else { \
        char __buf[__len + 1]; \
        memcpy(__buf, __tag, __len); \
        __buf[__len] = '\0'; \
        assert_string_equal(__buf, (tag)); \
    } \
} while (0)


#define __CHECK_NEXT_YAML_EVENT_3(type, tag, value) do { \
    __CHECK_NEXT_YAML_EVENT_2(type, tag); \
    assert_int(asdf_yaml_event_type(event), ==, ASDF_YAML_SCALAR_EVENT); \
    size_t __len = 0; \
    const char *__value = asdf_yaml_event_scalar_value(event, &__len); \
    if ((value) == NULL) { \
        assert_null(__value); \
    } else { \
        assert_int(__len, ==, strlen(value)); \
        char __buf[__len + 1]; \
        memcpy(__buf, __value, __len); \
        __buf[__len] = '\0'; \
        assert_string_equal(__buf, (value)); \
    } \
} while (0)


#define __CHECK_NEXT_YAML_EVENT_DISPATCH(_1, _2, _3, NAME, ...) NAME

#define CHECK_NEXT_YAML_EVENT(...) \
    __CHECK_NEXT_YAML_EVENT_DISPATCH( \
        __VA_ARGS__, \
        __CHECK_NEXT_YAML_EVENT_3, \
        __CHECK_NEXT_YAML_EVENT_2, \
        __CHECK_NEXT_YAML_EVENT_1 \
    )(__VA_ARGS__)


const off_t expected_offsets[] = {664};


MU_TEST(basic) {
    // TODO: Move all of this setup into setup/teardown functions; lots of repetition here
    const char *filename = get_reference_file_path("1.6.0/basic.asdf");
    asdf_parser_cfg_t parser_cfg = {.flags = ASDF_PARSER_OPT_EMIT_YAML_EVENTS};

    asdf_parser_t *parser = asdf_parser_create(&parser_cfg);
    asdf_event_t *event = NULL;

    if (!parser)
        munit_error("failed to initialize asdf parser");

    const char *stream = munit_parameters_get(params, "stream");
    size_t file_len = 0;
    char *file_contents = NULL;

    if (0 == strcmp(stream, "file")) {
        if (asdf_parser_set_input_file(parser, filename) != 0)
            munit_errorf("failed to set asdf parser file '%s'", filename);

    } else if (0 == strcmp(stream, "memory")) {
        file_contents = read_file(filename, &file_len);
        assert_not_null(file_contents);
        if (asdf_parser_set_input_mem(parser, file_contents, file_len) != 0)
            munit_error("failed to set asdf parser file");
    } else {
        munit_errorf("invalid test parameter for stream: %s", stream);
    }

    CHECK_NEXT_EVENT_TYPE(ASDF_ASDF_VERSION_EVENT);
    assert_string_equal(event->payload.version->version, "1.0.0");

    CHECK_NEXT_EVENT_TYPE(ASDF_STANDARD_VERSION_EVENT);
    assert_string_equal(event->payload.version->version, "1.6.0");

    CHECK_NEXT_EVENT_TYPE(ASDF_BLOCK_INDEX_EVENT);
    assert_int(event->payload.block_index->size, ==, 1);
    assert_memory_equal(1, event->payload.block_index->offsets, expected_offsets);

    CHECK_NEXT_EVENT_TYPE(ASDF_TREE_START_EVENT);
    assert_int(event->payload.tree->start, ==, 0x21);

    CHECK_NEXT_YAML_EVENT(ASDF_YAML_STREAM_START_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_DOCUMENT_START_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_START_EVENT, "tag:stsci.edu:asdf/core/asdf-1.1.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "asdf_library");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_START_EVENT, "tag:stsci.edu:asdf/core/software-1.0.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "author");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "The ASDF Developers");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "homepage");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "http://github.com/asdf-format/asdf");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "name");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "asdf");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "version");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "4.1.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "history");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_START_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "extensions");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SEQUENCE_START_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_START_EVENT, "tag:stsci.edu:asdf/core/extension_metadata-1.0.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "extension_class");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "asdf.extension._manifest.ManifestExtension");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "extension_uri");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "asdf://asdf-format.org/core/extensions/core-1.6.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "manifest_software");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_START_EVENT, "tag:stsci.edu:asdf/core/software-1.0.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "name");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "asdf_standard");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "version");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "1.1.1");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "software");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_START_EVENT, "tag:stsci.edu:asdf/core/software-1.0.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "name");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "asdf");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "version");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "4.1.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SEQUENCE_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "data");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_START_EVENT, "tag:stsci.edu:asdf/core/ndarray-1.1.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "source");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "datatype");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "int64");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "byteorder");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "little");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "shape");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SEQUENCE_START_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "8");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SEQUENCE_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_DOCUMENT_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_STREAM_END_EVENT);

    CHECK_NEXT_EVENT_TYPE(ASDF_TREE_END_EVENT);
    assert_int(event->payload.tree->start, ==, 0x21);
    assert_int(event->payload.tree->end, ==, 0x298);
    assert_null(event->payload.tree->buf);

    CHECK_NEXT_EVENT_TYPE(ASDF_BLOCK_EVENT);
    const asdf_block_info_t *block = event->payload.block;
    assert_int(block->header_pos, ==, 664);
    assert_int(block->data_pos, ==, 718);
    // 718 - 664 == 54 ?? But recall, the header_size field of the block_header
    // does not include the block magic and the header_size field itself (6 bytes)
    assert_int(block->header.header_size, ==, 48);
    assert_int(block->header.flags, ==, 0);
    assert_memory_equal(4, block->header.compression, "\0\0\0\0");
    assert_int(block->header.allocated_size, ==, 64);
    assert_int(block->header.used_size, ==, 64);
    assert_int(block->header.data_size, ==, 64);

    CHECK_NEXT_EVENT_TYPE(ASDF_END_EVENT);

    asdf_event_free(parser, event);
    asdf_parser_destroy(parser);

    // TODO: Add teardown fixtures too
    free(file_contents);
    return MUNIT_OK;
}


/**
 * Like `basic` but with YAML events disabled
 */
MU_TEST(basic_no_yaml) {
    const char *filename = get_reference_file_path("1.6.0/basic.asdf");
    asdf_parser_cfg_t parser_cfg = {0};

    asdf_parser_t *parser = asdf_parser_create(&parser_cfg);
    asdf_event_t *event = NULL;

    if (!parser)
        munit_error("failed to initialize asdf parser");

    const char *stream = munit_parameters_get(params, "stream");
    size_t file_len = 0;
    char *file_contents = NULL;

    if (0 == strcmp(stream, "file")) {
        if (asdf_parser_set_input_file(parser, filename) != 0)
            munit_errorf("failed to set asdf parser file '%s'", filename);
    } else if (0 == strcmp(stream, "memory")) {
        file_contents = read_file(filename, &file_len);
        assert_not_null(file_contents);
        if (asdf_parser_set_input_mem(parser, file_contents, file_len) != 0)
            munit_error("failed to set asdf parser file");
    } else {
        munit_errorf("invalid test parameter for stream: %s", stream);
    }

    CHECK_NEXT_EVENT_TYPE(ASDF_ASDF_VERSION_EVENT);
    assert_string_equal(event->payload.version->version, "1.0.0");

    CHECK_NEXT_EVENT_TYPE(ASDF_STANDARD_VERSION_EVENT);
    assert_string_equal(event->payload.version->version, "1.6.0");

    CHECK_NEXT_EVENT_TYPE(ASDF_BLOCK_INDEX_EVENT);
    assert_int(event->payload.block_index->size, ==, 1);
    assert_memory_equal(1, event->payload.block_index->offsets, expected_offsets);

    CHECK_NEXT_EVENT_TYPE(ASDF_TREE_START_EVENT);
    assert_int(event->payload.tree->start, ==, 0x21);

    CHECK_NEXT_EVENT_TYPE(ASDF_TREE_END_EVENT);
    assert_int(event->payload.tree->start, ==, 0x21);
    assert_int(event->payload.tree->end, ==, 0x298);
    assert_null(event->payload.tree->buf);

    CHECK_NEXT_EVENT_TYPE(ASDF_BLOCK_EVENT);
    const asdf_block_info_t *block = event->payload.block;
    assert_int(block->header_pos, ==, 664);
    assert_int(block->data_pos, ==, 718);
    // 718 - 664 == 54 ?? But recall, the header_size field of the block_header
    // does not include the block magic and the header_size field itself (6 bytes)
    assert_int(block->header.header_size, ==, 48);
    assert_int(block->header.flags, ==, 0);
    assert_memory_equal(4, block->header.compression, "\0\0\0\0");
    assert_int(block->header.allocated_size, ==, 64);
    assert_int(block->header.used_size, ==, 64);
    assert_int(block->header.data_size, ==, 64);

    CHECK_NEXT_EVENT_TYPE(ASDF_END_EVENT);

    asdf_event_free(parser, event);
    asdf_parser_destroy(parser);
    // TODO: Add teardown fixtures too
    free(file_contents);
    return MUNIT_OK;
}


/**
 * Like `basic_no_yaml` but with YAML buffering enabled
 */
MU_TEST(basic_no_yaml_buffer_yaml) {
    const char *filename = get_reference_file_path("1.6.0/basic.asdf");
    asdf_parser_cfg_t parser_cfg = {.flags = ASDF_PARSER_OPT_BUFFER_TREE};

    asdf_parser_t *parser = asdf_parser_create(&parser_cfg);
    asdf_event_t *event = NULL;

    if (!parser)
        munit_error("failed to initialize asdf parser");

    const char *stream = munit_parameters_get(params, "stream");
    size_t file_len = 0;
    char *file_contents = NULL;

    if (0 == strcmp(stream, "file")) {
        if (asdf_parser_set_input_file(parser, filename) != 0)
            munit_errorf("failed to set asdf parser file '%s'", filename);
    } else if (0 == strcmp(stream, "memory")) {
        file_contents = read_file(filename, &file_len);
        assert_not_null(file_contents);
        if (asdf_parser_set_input_mem(parser, file_contents, file_len) != 0)
            munit_error("failed to set asdf parser file");
    } else {
        munit_errorf("invalid test parameter for stream: %s", stream);
    }

    CHECK_NEXT_EVENT_TYPE(ASDF_ASDF_VERSION_EVENT);
    CHECK_NEXT_EVENT_TYPE(ASDF_STANDARD_VERSION_EVENT);
    CHECK_NEXT_EVENT_TYPE(ASDF_BLOCK_INDEX_EVENT);
    CHECK_NEXT_EVENT_TYPE(ASDF_TREE_START_EVENT);
    CHECK_NEXT_EVENT_TYPE(ASDF_TREE_END_EVENT);
    assert_int(event->payload.tree->start, ==, 0x21);
    assert_int(event->payload.tree->end, ==, 0x298);
    assert_not_null(event->payload.tree->buf);

    size_t reference_len = 0;
    char *reference_data = tail_file(filename, 2, &reference_len);
    assert_not_null(reference_data);
    size_t tree_size = event->payload.tree->end - event->payload.tree->start;
    assert_memory_equal(tree_size, event->payload.tree->buf, reference_data);
    free(reference_data);

    CHECK_NEXT_EVENT_TYPE(ASDF_BLOCK_EVENT);
    const asdf_block_info_t *block = event->payload.block;
    assert_int(block->header_pos, ==, 664);
    assert_int(block->data_pos, ==, 718);
    // 718 - 664 == 54 ?? But recall, the header_size field of the block_header
    // does not include the block magic and the header_size field itself (6 bytes)
    assert_int(block->header.header_size, ==, 48);
    assert_int(block->header.flags, ==, 0);
    assert_memory_equal(4, block->header.compression, "\0\0\0\0");
    assert_int(block->header.allocated_size, ==, 64);
    assert_int(block->header.used_size, ==, 64);
    assert_int(block->header.data_size, ==, 64);

    CHECK_NEXT_EVENT_TYPE(ASDF_END_EVENT);

    asdf_event_free(parser, event);
    asdf_parser_destroy(parser);
    // TODO: Add teardown fixtures too
    free(file_contents);
    return MUNIT_OK;
}


/**
 * Like `basic` but with YAML buffering enabled
 */
MU_TEST(basic_buffer_yaml) {
    const char *filename = get_reference_file_path("1.6.0/basic.asdf");
    FILE *file = fopen(filename, "r");
    assert_not_null(file);
    asdf_parser_cfg_t parser_cfg = {.flags = ASDF_PARSER_OPT_EMIT_YAML_EVENTS | ASDF_PARSER_OPT_BUFFER_TREE};

    asdf_parser_t *parser = asdf_parser_create(&parser_cfg);
    asdf_event_t *event = NULL;

    if (!parser)
        munit_error("failed to initialize asdf parser");

    const char *stream = munit_parameters_get(params, "stream");
    size_t file_len = 0;
    char *file_contents = NULL;

    if (0 == strcmp(stream, "file")) {
        if (asdf_parser_set_input_file(parser, filename) != 0)
            munit_errorf("failed to set asdf parser file '%s'", filename);
    } else if (0 == strcmp(stream, "memory")) {
        file_contents = read_file(filename, &file_len);
        assert_not_null(file_contents);
        if (asdf_parser_set_input_mem(parser, file_contents, file_len) != 0)
            munit_error("failed to set asdf parser file");
    } else {
        munit_errorf("invalid test parameter for stream: %s", stream);
    }

    CHECK_NEXT_EVENT_TYPE(ASDF_ASDF_VERSION_EVENT);
    CHECK_NEXT_EVENT_TYPE(ASDF_STANDARD_VERSION_EVENT);
    CHECK_NEXT_EVENT_TYPE(ASDF_BLOCK_INDEX_EVENT);
    CHECK_NEXT_EVENT_TYPE(ASDF_TREE_START_EVENT);
    assert_int(event->payload.tree->start, ==, 0x21);

    CHECK_NEXT_YAML_EVENT(ASDF_YAML_STREAM_START_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_DOCUMENT_START_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_START_EVENT, "tag:stsci.edu:asdf/core/asdf-1.1.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "asdf_library");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_START_EVENT, "tag:stsci.edu:asdf/core/software-1.0.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "author");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "The ASDF Developers");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "homepage");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "http://github.com/asdf-format/asdf");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "name");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "asdf");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "version");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "4.1.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "history");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_START_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "extensions");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SEQUENCE_START_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_START_EVENT, "tag:stsci.edu:asdf/core/extension_metadata-1.0.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "extension_class");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "asdf.extension._manifest.ManifestExtension");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "extension_uri");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "asdf://asdf-format.org/core/extensions/core-1.6.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "manifest_software");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_START_EVENT, "tag:stsci.edu:asdf/core/software-1.0.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "name");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "asdf_standard");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "version");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "1.1.1");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "software");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_START_EVENT, "tag:stsci.edu:asdf/core/software-1.0.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "name");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "asdf");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "version");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "4.1.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SEQUENCE_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "data");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_START_EVENT, "tag:stsci.edu:asdf/core/ndarray-1.1.0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "source");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "0");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "datatype");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "int64");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "byteorder");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "little");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "shape");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SEQUENCE_START_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SCALAR_EVENT, NULL, "8");
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_SEQUENCE_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_MAPPING_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_DOCUMENT_END_EVENT);
    CHECK_NEXT_YAML_EVENT(ASDF_YAML_STREAM_END_EVENT);

    CHECK_NEXT_EVENT_TYPE(ASDF_TREE_END_EVENT);
    assert_int(event->payload.tree->start, ==, 0x21);
    assert_int(event->payload.tree->end, ==, 0x298);
    assert_not_null(event->payload.tree->buf);

    size_t reference_len = 0;
    char *reference_data = tail_file(filename, 2, &reference_len);
    assert_not_null(reference_data);
    size_t tree_size = event->payload.tree->end - event->payload.tree->start;
    assert_memory_equal(tree_size, event->payload.tree->buf, reference_data);
    free(reference_data);

    CHECK_NEXT_EVENT_TYPE(ASDF_BLOCK_EVENT);
    const asdf_block_info_t *block = event->payload.block;
    assert_int(block->header_pos, ==, 664);
    assert_int(block->data_pos, ==, 718);
    // 718 - 664 == 54 ?? But recall, the header_size field of the block_header
    // does not include the block magic and the header_size field itself (6 bytes)
    assert_int(block->header.header_size, ==, 48);
    assert_int(block->header.flags, ==, 0);
    assert_memory_equal(4, block->header.compression, "\0\0\0\0");
    assert_int(block->header.allocated_size, ==, 64);
    assert_int(block->header.used_size, ==, 64);
    assert_int(block->header.data_size, ==, 64);

    CHECK_NEXT_EVENT_TYPE(ASDF_END_EVENT);

    asdf_event_free(parser, event);
    asdf_parser_destroy(parser);
    // TODO: Add teardown fixtures too
    free(file_contents);
    return MUNIT_OK;
}


/* Parameterize all tests to work on file and memory buffers */
static char *stream_params[] = {"file", "memory", NULL};
static MunitParameterEnum test_params[] = {
    {"stream", stream_params},
    {NULL, NULL}
};


MU_TEST_SUITE(
    event,
    MU_RUN_TEST(basic, test_params),
    MU_RUN_TEST(basic_no_yaml, test_params),
    MU_RUN_TEST(basic_no_yaml_buffer_yaml, test_params),
    MU_RUN_TEST(basic_buffer_yaml, test_params)
);


MU_RUN_SUITE(event);
