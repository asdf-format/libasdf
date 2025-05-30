# Tests
# 'check' comes from 'make check'
AM_CPPFLAGS = -I$(top_srcdir)/src

asdf_standard_dir = $(top_srcdir)/asdf-standard
asdf_standard_submodule = $(asdf_standard_dir)/.git
munit_submodule = $(srcdir)/tests/munit.git

$(asdf_standard_submodule) $(munit_submodule):
	echo "Initializing submodules..."
	git submodule update --init --recursive
	git config submodule.tests/munit.ignore untracked


@VALGRIND_CHECK_RULES@

check_PROGRAMS = test-event.unit test-parse.unit test-parse-util.unit

TESTS_ENVIRONMENT = SRCDIR=$(top_srcdir)
TESTS = $(check_PROGRAMS)

UNIT_TEST_CPPFLAGS = \
    $(AM_CPPFLAGS) \
    -DREFERENCE_FILES_DIR=\"$(asdf_standard_dir)/reference_files\" \
    -DFIXTURES_DIR=\"$(top_srcdir)/tests/fixtures\"

# Unit tests
# test-event.unit
test_event_unit_SOURCES = test-event.c util.c munit/munit.c
test_event_unit_CPPFLAGS = $(UNIT_TEST_CPPFLAGS)
test_event_unit_CFLAGS = $(FYAML_CFLAGS) $(CODE_COVERAGE_CFLAGS)
test_event_unit_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)
test_event_unit_LDADD = $(top_srcdir)/libasdf.la

# TODO: These are highly reptitive; should make a template for it
# test-parse.unit
test_parse_unit_SOURCES = test-parse.c util.c munit/munit.c
test_parse_unit_CPPFLAGS = $(UNIT_TEST_CPPFLAGS)
test_parse_unit_CFLAGS = $(FYAML_CFLAGS) $(CODE_COVERAGE_CFLAGS)
test_parse_unit_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)
test_parse_unit_LDADD = $(top_srcdir)/libasdf.la

# test-parse-util.unit
test_parse_util_unit_SOURCES = test-parse-util.c $(top_srcdir)/src/parse_util.c util.c munit/munit.c
test_parse_util_unit_CPPFLAGS = $(UNIT_TEST_CPPFLAGS)
test_parse_util_unit_CFLAGS = $(FYAML_CFLAGS) $(CODE_COVERAGE_CFLAGS)
test_parse_util_unit_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)

# Shell tests
TOOL_TESTS = test-info.sh test-events.sh

if ASDF_BUILD_TOOL
TESTS += $(TOOL_TESTS)
else
$(warning Skipping running asdf command-line tests; make sure to run ./configure --enable-tool; make to build the tool)
endif


AUTOMAKE_OPTIONS = parallel-tests
EXTRA_DIST = minunit.h

# Bit of a hack to make syncing the submodule a prerequisite to make check
check-am: $(asdf_standard_submodule)
