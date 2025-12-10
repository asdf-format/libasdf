# third_party headers to include
stc_dir = STC

# Bare minimum subset of headers needed from STC
# This is determined by trial-and-error with `make distcheck` ;)
stc_headers = \
    $(stc_dir)/include/stc/common.h \
    $(stc_dir)/include/stc/cstr.h \
    $(stc_dir)/include/stc/hmap.h \
    $(stc_dir)/include/stc/priv/cstr_prv.c \
    $(stc_dir)/include/stc/priv/cstr_prv.h \
    $(stc_dir)/include/stc/priv/linkage.h \
    $(stc_dir)/include/stc/priv/linkage2.h \
    $(stc_dir)/include/stc/priv/template.h \
    $(stc_dir)/include/stc/priv/template2.h \
    $(stc_dir)/include/stc/priv/utf8_prv.h \
    $(stc_dir)/include/stc/vec.h \
    $(stc_dir)/include/stc/types.h

stc_src = $(stc_dir)/src/cstr_core.c

noinst_HEADERS = $(stc_headers)
noinst_LTLIBRARIES = libstc.la

libstc_la_SOURCES = $(stc_src)
libstc_la_CFLAGS = $(AM_CFLAGS)

EXTRA_DIST = \
    $(stc_dir)/LICENSE
