# third_party headers to include
stc_dir = STC

# Bare minimum subset of headers needed from STC
# This is determined by trial-and-error with `make distcheck` ;)
stc_headers = \
    $(stc_dir)/include/stc/common.h \
    $(stc_dir)/include/stc/types.h \
    $(stc_dir)/include/stc/priv/utf8_tab.c \
    $(stc_dir)/include/stc/cbits.h \
    $(stc_dir)/include/stc/vec.h \
    $(stc_dir)/include/stc/sset.h \
    $(stc_dir)/include/stc/cspan.h \
    $(stc_dir)/include/stc/box.h \
    $(stc_dir)/include/stc/cregex.h \
    $(stc_dir)/include/stc/algorithm.h \
    $(stc_dir)/include/stc/random.h \
    $(stc_dir)/include/stc/coroutine.h \
    $(stc_dir)/include/stc/cstr.h \
    $(stc_dir)/include/stc/hmap.h \
    $(stc_dir)/include/stc/queue.h \
    $(stc_dir)/include/stc/utf8.h \
    $(stc_dir)/include/stc/arc.h \
    $(stc_dir)/include/stc/coption.h \
    $(stc_dir)/include/stc/smap.h \
    $(stc_dir)/include/stc/zsview.h \
    $(stc_dir)/include/stc/pqueue.h \
    $(stc_dir)/include/stc/hset.h \
    $(stc_dir)/include/stc/stack.h \
    $(stc_dir)/include/stc/deque.h \
    $(stc_dir)/include/stc/csview.h \
    $(stc_dir)/include/stc/sort.h \
    $(stc_dir)/include/stc/list.h \
    $(stc_dir)/include/stc/sys/crange.h \
    $(stc_dir)/include/stc/sys/filter.h \
    $(stc_dir)/include/stc/sys/sumtype.h \
    $(stc_dir)/include/stc/sys/utility.h \
    $(stc_dir)/include/stc/priv/linkage2.h \
    $(stc_dir)/include/stc/priv/sort_prv.h \
    $(stc_dir)/include/stc/priv/utf8_prv.h \
    $(stc_dir)/include/stc/priv/queue_prv.h \
    $(stc_dir)/include/stc/priv/template.h \
    $(stc_dir)/include/stc/priv/template2.h \
    $(stc_dir)/include/stc/priv/linkage.h \
    $(stc_dir)/include/stc/priv/utf8_prv.c \
    $(stc_dir)/include/stc/priv/ucd_prv.c \
    $(stc_dir)/include/stc/priv/cregex_prv.c \
    $(stc_dir)/include/stc/priv/cstr_prv.h \
    $(stc_dir)/include/stc/priv/cstr_prv.c \
    $(stc_dir)/include/c11/fmt.h

stc_src = \
    $(stc_dir)/src/fmt.c \
    $(stc_dir)/src/csview.c \
    $(stc_dir)/src/cstr_io.c \
    $(stc_dir)/src/cspan.c \
    $(stc_dir)/src/cstr_core.c \
    $(stc_dir)/src/cstr_utf8.c \
    $(stc_dir)/src/random.c \
    $(stc_dir)/src/cregex.c \
    $(stc_dir)/src/stc_core.c

noinst_HEADERS = $(stc_headers)
noinst_LTLIBRARIES = libstc.la

libstc_la_SOURCES = $(stc_src)
libstc_la_CFLAGS = $(AM_CFLAGS)
libstc_la_LDFLAGS = $(AM_LDFLAGS) -lm

# CMake-related files to include in the distribution
CMAKE_DIST = CMakeLists.txt

EXTRA_DIST = \
    $(CMAKE_DIST) \
    $(stc_dir)/LICENSE
