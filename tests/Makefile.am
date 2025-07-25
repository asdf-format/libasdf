# Tests
# 'check' comes from 'make check'
AM_CPPFLAGS = \
    -I$(top_srcdir)/include \
    -I$(top_srcdir)/third_party/STC/include \
    -I$(top_srcdir)/src

asdf_standard_dir = $(top_srcdir)/asdf-standard
asdf_standard_submodule = $(asdf_standard_dir)/.git
munit_submodule = $(srcdir)/tests/munit.git
submodules = $(asdf_standard_submodule) $(munit_submodule)

$(asdf_standard_submodule) $(munit_submodule):
	@if test -d $(top_srcdir)/.git; then \
	    echo "Initializing submodules $@ ..."; \
	    git submodule update --init --recursive; \
	    git config submodule.tests/munit.ignore untracked; \
	else \
	    echo "Skipping submodule init: not in a Git repository"; \
	fi


@VALGRIND_CHECK_RULES@

check_PROGRAMS = \
    test-block.unit \
    test-event.unit \
    test-file.unit \
    test-parse.unit \
    test-parse-util.unit \
    test-stream.unit \
    test-value.unit \
    test-value-util.unit

TESTS_ENVIRONMENT = top_builddir=$(top_builddir) top_srcdir=$(top_srcdir)
TESTS = $(check_PROGRAMS)

unit_test_cppflags = \
    $(AM_CPPFLAGS) \
    -DREFERENCE_FILES_DIR=\"$(asdf_standard_dir)/reference_files\" \
    -DFIXTURES_DIR=\"$(srcdir)/fixtures\"
unit_test_cflags = $(FYAML_CFLAGS) $(CODE_COVERAGE_CFLAGS) $(ASDF_CFLAGS)

if DEBUG
unit_test_cflags += -DMUNIT_NO_FORK
endif

unit_test_ldflags = $(CODE_COVERAGE_LDFLAGS)
unit_test_ldadd = $(top_builddir)/libasdf.la libmunit.a

# Unit tests
check_LIBRARIES = libmunit.a
libmunit_a_SOURCES = util.c munit/munit.c
libmunit_a_CPPFLAGS = $(unit_test_cppflags)
libmunit_a_CFLAGS = $(unit_test_cflags) -w

# test-block.unit
test_block_unit_SOURCES = test-block.c
test_block_unit_CPPFLAGS = $(unit_test_cppflags)
test_block_unit_CFLAGS = $(unit_test_cflags)
test_block_unit_LDFLAGS = $(unit_test_ldflags)
test_block_unit_LDADD = $(unit_test_ldadd)

# test-event.unit
test_event_unit_SOURCES = test-event.c
test_event_unit_CPPFLAGS = $(unit_test_cppflags)
test_event_unit_CFLAGS = $(unit_test_cflags)
test_event_unit_LDFLAGS = $(unit_test_ldflags)
test_event_unit_LDADD = $(unit_test_ldadd)

# test-file.unit
test_file_unit_SOURCES = test-file.c
test_file_unit_CPPFLAGS = $(unit_test_cppflags)
test_file_unit_CFLAGS = $(unit_test_cflags)
test_file_unit_LDFLAGS = $(unit_test_ldflags)
test_file_unit_LDADD = $(unit_test_ldadd)

# test-parse.unit
test_parse_unit_SOURCES = test-parse.c
test_parse_unit_CPPFLAGS = $(unit_test_cppflags)
test_parse_unit_CFLAGS = $(unit_test_cflags)
test_parse_unit_LDFLAGS = $(unit_test_ldflags)
test_parse_unit_LDADD = $(unit_test_ldadd)

# test-parse-util.unit
test_parse_util_unit_SOURCES = \
    test-parse-util.c \
    $(top_srcdir)/src/block.c \
    $(top_srcdir)/src/error.c \
    $(top_srcdir)/src/parse_util.c
test_parse_util_unit_CPPFLAGS = $(unit_test_cppflags)
test_parse_util_unit_CFLAGS = $(unit_test_cflags)
test_parse_util_unit_LDFLAGS = $(unit_test_ldflags)
test_parse_util_unit_LDADD = libmunit.a

# test-stream.unit
test_stream_unit_SOURCES = \
    test-stream.c \
    $(top_srcdir)/src/context.c \
    $(top_srcdir)/src/error.c \
    $(top_srcdir)/src/log.c \
    $(top_srcdir)/src/stream.c
test_stream_unit_CPPFLAGS = $(unit_test_cppflags)
test_stream_unit_CFLAGS = $(unit_test_cflags)
test_stream_unit_LDFLAGS = $(unit_test_ldflags)
test_stream_unit_LDADD = $(FYAML_LIBS) libmunit.a

# test-value.unit
test_value_unit_SOURCES = test-value.c
test_value_unit_CPPFLAGS = $(unit_test_cppflags)
test_value_unit_CFLAGS = $(unit_test_cflags)
test_value_unit_LDFLAGS = $(unit_test_ldflags)
test_value_unit_LDADD = $(unit_test_ldadd)

# test-value-util.unit
test_value_util_unit_SOURCES = \
    test-value-util.c \
    $(top_srcdir)/src/value_util.c \
    $(top_srcdir)/third_party/STC/src/cstr_core.c
test_value_util_unit_CPPFLAGS = $(unit_test_cppflags)
test_value_util_unit_CFLAGS = $(unit_test_cflags)
test_value_util_unit_LDFLAGS = $(unit_test_ldflags)
test_value_util_unit_LDADD = libmunit.a

# Shell tests
TOOL_TESTS = test-events.sh test-info.sh

if ASDF_BUILD_TOOL
TESTS += $(TOOL_TESTS)
else
$(warning Skipping running asdf command-line tests; make sure to run ./configure --enable-tool; make to build the tool)
endif


AUTOMAKE_OPTIONS = parallel-tests

EXTRA_DIST = $(TOOL_TESTS)
EXTRA_DIST += \
    munit.h \
    shell-test.sh \
    util.h \
    fixtures/255.asdf \
    fixtures/events/basic.events.txt \
    fixtures/events/complex.events.txt \
    fixtures/events/compressed.events.txt \
    fixtures/events/int.events.txt \
    fixtures/info/anchor.info.txt \
    fixtures/info/ascii.info.txt \
    fixtures/info/basic.info.txt \
    fixtures/info/complex.info.txt \
    fixtures/info/compressed.info.txt \
    fixtures/info/endian.info.txt \
    fixtures/info/exploded0000.info.txt \
    fixtures/info/exploded.info.txt \
    fixtures/info/float.info.txt \
    fixtures/info/int.info.txt \
    fixtures/info/scalars.info.txt \
    fixtures/info/shared.info.txt \
    fixtures/info/stream.info.txt \
    fixtures/info/structured.info.txt \
    fixtures/info/unicode_bmp.info.txt \
    fixtures/info/unicode_spp.info.txt \
    fixtures/parse-minimal.asdf \
    fixtures/parse-minimal-extra-comment.asdf \
    fixtures/scalars.asdf \
    fixtures/tagged-scalars.asdf \
    fixtures/value-types.asdf \
    munit/COPYING \
    munit/munit.c \
    munit/munit.h


# Bit of a hack to make syncing the submodule a prerequisite to make check
check-am: $(submodules)

# Clean up temp files from tests
distclean-local:
	rm -rf $(builddir)/tmp

update-fixtures:
	./test-events.sh --update
	./test-info.sh --update
