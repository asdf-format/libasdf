src_files = \
    src/block.c \
    src/compression/bzp2.c \
    src/compression/compressor_registry.c \
    src/compression/compression.c \
    src/compression/lz4.c \
    src/compression/zlib.c \
    src/context.c \
    src/core/asdf.c \
    src/core/datatype.c \
    src/core/extension_metadata.c \
    src/core/history_entry.c \
    src/core/ndarray.c \
    src/core/ndarray_convert.c \
    src/core/software.c \
    src/core/time.c \
    src/emitter.c \
    src/error.c \
    src/event.c \
    src/extension_registry.c \
    src/extension_util.c \
    src/tag.c \
    src/file.c \
    src/info.c \
    src/log.c \
    src/parse_util.c \
    src/parser.c \
    src/stream.c \
    src/util.c \
    src/value.c \
    src/value_util.c \
    src/version.c \
    src/yaml.c

src_headers = \
    src/block.h \
    src/compat/endian.h \
    src/compression/asdf_compressor_map.h \
    src/compression/compression.h \
    src/compression/compressor_registry.h \
    src/context.h \
    src/core/asdf.h \
    src/core/datatype.h \
    src/core/extension_metadata.h \
    src/core/history_entry.h \
    src/core/ndarray.h \
    src/core/ndarray_convert.h \
    src/core/software.h \
    src/emitter.h \
    src/error.h \
    src/extension_registry.h \
    src/extension_util.h \
    src/tag.h \
    src/event.h \
    src/file.h \
    src/info.h \
    src/log.h \
    src/parse_util.h \
    src/parser.h \
    src/stream.h \
    src/stream_intern.h \
    src/types/asdf_block_index.h \
    src/types/asdf_block_info_vec.h \
    src/types/asdf_common_tag_map.h \
    src/types/asdf_extension_map.h \
    src/types/asdf_str_map.h \
    src/util.h \
    src/value.h \
    src/value_util.h \
    src/yaml.h

include_dir = $(top_srcdir)/include

AM_CFLAGS = $(ASDF_CFLAGS) -fvisibility=hidden -fmacro-prefix-map=$(top_srcdir)/=
AM_CPPFLAGS = $(ASDF_CPPFLAGS) -I$(top_srcdir)/third_party/STC/include -I$(top_srcdir)/include
AM_LDFLAGS = $(ASDF_LDFLAGS)
ACLOCAL_AMFLAGS = -Im4

noinst_HEADERS = $(src_headers)

# libasdf library
#include .c and .h in SOURCES so that both appear in dist
lib_LTLIBRARIES = libasdf.la
libasdf_la_SOURCES = $(src_files)
libasdf_la_CFLAGS = $(AM_CFLAGS) $(FYAML_CFLAGS) $(STATGRAB_CFLAGS) $(LZ4_CFLAGS) $(CODE_COVERAGE_CFLAGS)
libasdf_la_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)
libasdf_la_LIBADD =  third_party/libstc.la $(ASDF_LIBS)

# Static convenience library for malloc-wrap tests; built only when the
# linker supports --wrap.  Linking test-malloc-fail.unit against this archive
# (rather than against the shared libasdf) lets --wrap intercept allocations
# made anywhere inside the library without having to compile individual
# translation units directly into the test binary.
if HAVE_LINKER_WRAP
noinst_LTLIBRARIES = libasdf_static.la
libasdf_static_la_SOURCES = $(src_files)
libasdf_static_la_CFLAGS = $(libasdf_la_CFLAGS)
libasdf_static_la_LIBADD = third_party/libstc.la $(ASDF_LIBS)
endif # HAVE_LINKER_WRAP

# The asdf command line tool
if ASDF_BUILD_TOOL
bin_PROGRAMS = asdf
asdf_SOURCES = src/main.c
asdf_CFLAGS = $(AM_CFLAGS) $(FYAML_CFLAGS) $(STATGRAB_CFLAGS) $(LZ4_CFLAGS) $(CODE_COVERAGE_CFLAGS)
asdf_LDFLAGS = $(CODE_COVERAGE_LDFLAGS)
asdf_LDADD =  third_party/libstc.la libasdf.la $(ARGP_LIBS) $(ASDF_LIBS)
endif # ASDF_BUILD_TOOL


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libasdf.pc
CLEANFILES = libasdf.pc


# Misc make targets
format:
	clang-format -i $(addprefix $(srcdir)/,$(src_files)) $(addprefix $(srcdir)/,$(src_headers))


TIDY_FILES = $(addprefix $(srcdir)/,$(src_files))
# NOTE: pass STC/include as -isystem to prevent clang-tidy from producing warnings in STC
# However, this still doesn't appear to get the job done :(
tidy:
	clang-tidy $(TIDY_FILES) -- \
	    -isystem $(top_srcdir)/third_party/STC/include $(DEFS) $(DEFAULT_INCLUDES) \
            $(AM_CPPFLAGS) $(CPPFLAGS) $(asdf_CFLAGS)

if BUILD_DOCS
docs:
	$(SPHINXBUILD) $(SPHINX_FLAGS) -b html $(srcdir)/docs $(srcdir)/docs/_build/html

distcheck-docs:
	$(MAKE) docs SPHINX_FLAGS=-W
endif
.PHONY: docs distcheck-docs


CMAKE_EXTRA_FLAGS=

# Try building and running tests with cmake to ensure the cmake build works
# from the distribution tarball
#
# - Pass BZIP2_LIBRARIES etc. since my system at least is missing a pc file
#   for bzip2.  Maybe should do this for other dependencies as well but only
#   on an as-needed basis
distcheck-cmake:
	@echo "=== Running CMake build inside distcheck ==="
	rm -rf $(distdir)/_build/cmake
	mkdir $(distdir)/_build/cmake
	cd $(distdir)/_build/cmake && \
	    cmake ../.. \
                -DENABLE_TESTING_ALL=YES \
                -DCMAKE_BUILD_TYPE=Release \
                -DBZIP2_LIBRARIES=$(BZIP2_LIBS) \
                $(CMAKE_EXTRA_FLAGS) && \
            $(MAKE) && \
            $(MAKE) test

.PHONY: distcheck-cmake


# 1. Check building docs from the distribution if possible
# 2. Test the CMake build
distcheck-hook:
	$(MAKE) distcheck-docs
	$(MAKE) distcheck-cmake


CODE_COVERAGE_IGNORE_PATTERN = \
    "$(realpath $(abs_top_srcdir)/tests)/*" \
    "$(realpath $(abs_top_srcdir)/third_party)/*" \
    "/usr/include/*"

# For rules generated by AX_AM_MACROS_STATIC; e.g. code coverage rules
include $(top_srcdir)/aminclude_static.am

SUBDIRS = include tests third_party docs

# Include sample files from asdf-standard in the distribution (used in tests)
# Eventually test against older reference files as well
reference_files = $(top_srcdir)/asdf-standard/reference_files/1.6.0

# CMake-related files to include in the distribution
CMAKE_DIST = \
    CMakeLists.txt \
    cmake/AddressAnalyzer.cmake \
    cmake/ASDFConfig.cmake \
    cmake/ASDFDependencies.cmake \
    cmake/ASDFOptions.cmake \
    cmake/SubmoduleInit.cmake \
    config.h.cmake \
    src/CMakeLists.txt


EXTRA_DIST = \
    $(CMAKE_DIST) \
    CHANGES.rst \
    CODE_OF_CONDUCT.md \
    LICENSE \
    README.rst \
    $(reference_files)/anchor.asdf \
    $(reference_files)/ascii.asdf \
    $(reference_files)/basic.asdf \
    $(reference_files)/complex.asdf \
    $(reference_files)/compressed.asdf \
    $(reference_files)/endian.asdf \
    $(reference_files)/exploded0000.asdf \
    $(reference_files)/exploded.asdf \
    $(reference_files)/float.asdf \
    $(reference_files)/int.asdf \
    $(reference_files)/scalars.asdf \
    $(reference_files)/shared.asdf \
    $(reference_files)/stream.asdf \
    $(reference_files)/structured.asdf \
    $(reference_files)/unicode_bmp.asdf \
    $(reference_files)/unicode_spp.asdf
