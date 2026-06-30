# CMake-related files to include in the distribution
CMAKE_DIST = CMakeLists.txt

EXTRA_DIST = \
    $(CMAKE_DIST) \
    conf.py \
    api/asdf/core/datatype.h.rst \
    api/asdf/core/ndarray.h.rst \
    api/asdf/error.h.rst \
    api/asdf/emitter.h.rst \
    api/asdf/error.h.rst \
    api/asdf/extension.h.rst \
    api/asdf/file.h.rst \
    api/asdf/log.h.rst \
    api/asdf/value.h.rst \
    api/asdf/yaml.h.rst \
    environment.yml \
    index.rst \
    links.rst \
    usage/cli.rst \
    usage/compression.rst \
    usage/examples.rst \
    usage/extensions.rst \
    usage/ndarrays.rst \
    usage/opening.rst \
    usage/overview.rst \
    usage/values.rst \
    usage/writing.rst \
    _static/css/globalnav.css \
    _static/images/favicon.ico \
    _static/images/logo-dark-mode.png \
    _static/images/logo-light-mode.png


# Regenerate the checked-in asdf(1) man page from usage/cli.rst.  The man page
# (man/asdf.1) is committed to the repository so that building a release tarball
# or installing the tool does not require Sphinx; run this target after editing
# usage/cli.rst, then commit the updated man/asdf.1.
if BUILD_DOCS
man-page: $(srcdir)/man/asdf.1

$(srcdir)/man/asdf.1: $(srcdir)/usage/cli.rst $(srcdir)/conf.py
	$(SPHINXBUILD) -b man $(srcdir) $(builddir)/_build/man
	$(MKDIR_P) $(srcdir)/man
	cp $(builddir)/_build/man/asdf.1 $@
endif
.PHONY: man-page
