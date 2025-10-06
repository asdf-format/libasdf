AC_PREREQ([2.69])
AC_INIT(
  [libasdf],
  [0.0.0],
  [https://github.com/asdf-format/libasdf/issues],
  [],
  [https://github.com/asdf-format/libasdf],
)
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([foreign subdir-objects])

# needed for strptime, at the very least...
AC_USE_SYSTEM_EXTENSIONS

LT_INIT([disable-static])

# silent make https://autotools.io/automake/silent.html
# silent rules enabled by default with 'yes'
# disable silent runles with ./configure --disable-silent-rules
AM_SILENT_RULES([yes]) # less verbose make output 

AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_MACRO_DIR([m4])

AC_LANG([C])
AC_PROG_CC

AS_IF([test "x$ac_cv_prog_cc_c11" = "xno"],
  [AC_MSG_ERROR([C11 support is required but not available])])


AC_PROG_INSTALL

AC_HEADER_ASSERT # ./configure --disable-assert to define NDEBUG 

# Check headers
AC_CHECK_HEADER([stdlib.h])
AC_CHECK_HEADERS([endian.h machine/endian.h sys/endian.h])
AC_CHECK_DECLS([be64toh], [], [], [[
#ifdef HAVE_ENDIAN_H
#  include <endian.h>
#endif
#ifdef HAVE_MACHINE_ENDIAN_H
#  include <machine/endian.h>
#endif
#ifdef HAVE_SYS_ENDIAN_H
#  include <sys/endian.h>
#endif
]])

# Check functions
AC_CHECK_FUNCS([strptime])


# http://www.gnu.org/software/autoconf-archive/ax_valgrind_check.html
# - make check-valgrind
AX_VALGRIND_CHECK
# http://www.gnu.org/software/autoconf-archive/ax_code_coverage.html#ax_code_coverage
# - make check-code-coverage generates coverage report
AX_CODE_COVERAGE


# Check and add compiler flags
AX_CHECK_COMPILE_FLAG([-Wall],          [ASDF_CFLAGS="$ASDF_CFLAGS -Wall"])
AX_CHECK_COMPILE_FLAG([-Wextra],        [ASDF_CFLAGS="$ASDF_CFLAGS -Wextra"])
AX_CHECK_COMPILE_FLAG([-Wparentheses],  [ASDF_CFLAGS="$ASDF_CFLAGS -Wparentheses"])
AX_CHECK_COMPILE_FLAG([-Wpointer-sign], [ASDF_CFLAGS="$ASDF_CFLAGS -Wpointer-sign"])
AX_CHECK_COMPILE_FLAG([-Wshadow],       [ASDF_CFLAGS="$ASDF_CFLAGS -Wshadow"])


# Additional enable flags
# --enable-debug
AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug], [Enable debug build (-g -O0)])],
  [enable_debug=yes],
  [enable_debug=no]
)


ASDF_BUILD_MODE=

AS_IF([test "x$enable_debug" = "xyes"], [
  ASDF_CFLAGS="$ASDF_CFLAGS -g -O0"
  AC_DEFINE([DEBUG], [1], [Define if debugging is enabled])
  ASDF_BUILD_MODE="${ASDF_BUILD_MODE}debug "
])

AM_CONDITIONAL([DEBUG], [test "x$enable_debug" = "xyes"])

# --disable-tool
AC_ARG_ENABLE([tool],
  [AS_HELP_STRING([--disable-tool], [Do not build the asdf command-line tool])],
  [asdf_build_tool=$enableval],
  [asdf_build_tool=yes])

AM_CONDITIONAL([ASDF_BUILD_TOOL], [test "x$asdf_build_tool" = xyes])

# --with-asan
AC_ARG_WITH([asan],
  [AS_HELP_STRING([--with-asan], [Build with AddressSanitizer support])],
  [with_asan=yes],
  [with_asan=no])

AS_IF([test "x$with_asan" = "xyes"], [
  ASDF_CFLAGS="$ASDF_CFLAGS -O1 -g -fsanitize=address -fno-omit-frame-pointer"
  ASDF_LDFLAGS="$ASDF_LDFLAGS -fsanitize=address"
  AC_DEFINE([WITH_ASAN], [1], [Defined if building with AddressSanitizer])
  ASDF_BUILD_MODE="${ASDF_BUILD_MODE}asan "
])

# Logging flags
AC_ARG_ENABLE([logging],
  [AS_HELP_STRING([--enable-logging], [Enable logging (default: yes)])],
  [enable_logging=$enableval],
  [enable_logging=yes]
)

AS_IF([test "x$enable_logging" = "xyes"],
  [AC_DEFINE([ASDF_LOG_ENABLED], [1], [Enable logging])]
)

AC_ARG_ENABLE([log-color],
  [AS_HELP_STRING([--enable-log-color],
                  [Enable color output in logs (default: yes)])],
  [enable_log_color=$enableval],
  [enable_log_color=yes]
)

if test "x$enable_log_color" = "xyes"; then
  AC_DEFINE([ASDF_LOG_COLOR], [1], [Enable color output in logs])
fi

AC_ARG_WITH([log-default],
  [AS_HELP_STRING([--with-log-default=LEVEL], [Set default runtime log level])],
  [AS_CASE([$with_log_default],
    [TRACE|DEBUG|INFO|WARN|ERROR|FATAL|NONE], [],
    [AC_MSG_ERROR([Invalid log level: $with_log_default])])],
  [with_log_default=WARN]
)

AC_DEFINE_UNQUOTED([ASDF_LOG_DEFAULT_LEVEL], [ASDF_LOG_$with_log_default],
                   [Default runtime log level])

AC_ARG_WITH([log-min],
  [AS_HELP_STRING([--with-log-min=LEVEL], [Set compile-time minimum log level])],
  [AS_CASE([$with_log_min],
    [TRACE|DEBUG|INFO|WARN|ERROR|FATAL|NONE], [],
    [AC_MSG_ERROR([Invalid min log level: $with_log_min])])],
  [with_log_min=TRACE]
)

AC_DEFINE_UNQUOTED([ASDF_LOG_MIN_LEVEL], [ASDF_LOG_$with_log_min],
                   [Compile-time minimum log level])


# Initalize submodules needed for the build
AS_IF([test ! -f "$srcdir/third_party/STC/LICENSE"],
  [AC_MSG_NOTICE([Initializing submodule third_party/STC...])
   (cd "$srcdir" && git submodule update --init third_party/STC && \
     git config submodule.third_party/STC.ignore untracked || \
     AC_MSG_ERROR([Failed to init submodule third_party/STC]))])


# Checks for libraries.
# Preferably using pkg-config, could fall back on AC_SEARCH_LIBS if not
AC_PATH_PROG([PKG_CONFIG], [pkg-config], [no])
if test "$PKG_CONFIG" = "no"; then
  AC_MSG_ERROR([pkg-config is required but was not found])
fi

PKG_CHECK_MODULES([FYAML], [libfyaml], [], [
  AC_MSG_ERROR([libfyaml is required but was not found])
])

# Check for homebrew packages in macOS
AS_IF([test "x$asdf_build_tool" = "xyes"], [
  ASDF_CHECK_HOMEBREW_PKG([argp-standalone])

# Shouldn't be needed with glibc but is if we're using the homebrew package, e.g.
  AC_CHECK_LIB([argp], [argp_parse], [
    AC_SUBST([ARGP_LIB], [-largp])
  ])
])

# ------ Optional features ---------------------------------------------------

AC_ARG_WITH([gwcs],
  [AS_HELP_STRING([--with-gwcs],
                  [Build with gwcs extension support--will later be moved to an external plugin])],
  [with_gwcs=$withval],
  [with_gwcs=no])

# fits-wcs option (implies gwcs)
AC_ARG_WITH([fits-wcs],
  [AS_HELP_STRING([--with-fits-wcs],
                  [Include support for FITS WCS via WCSLIB; implies --with-gwcs])],
  [with_fits_wcs=$withval],
  [with_fits_wcs=no])

# if fits-wcs requested, force gwcs to yes
AS_IF([test "x$with_fits_wcs" = "xyes"], [
  AS_IF([test "x$with_gwcs" != "xyes"], [
    with_gwcs=yes
    AC_MSG_NOTICE([--with-fits-wcs implies --with-gwcs, enabling gwcs support])
  ])
])

AS_IF([test "x$with_gwcs" = "xyes"], [
  AC_DEFINE([WITH_GWCS], [1], [Defined if building with gwcs enabled])
  ASDF_OPTIONAL="${ASDF_OPTIONAL}gwcs "
])
AM_CONDITIONAL([HAVE_GWCS], [test "x$with_gwcs" = "xyes"])

AS_IF([test "x$with_fits_wcs" = "xyes"], [
  # Check for wcslib now
  AC_MSG_CHECKING([for WCSLIB header wcslib/wcs.h])
  AC_CHECK_HEADERS([wcslib/wcs.h], [have_wcslib=yes], [have_wcslib=no])
  AC_MSG_RESULT([$have_wcslib])

  AC_MSG_CHECKING([for WCSLIB library])
  AC_CHECK_LIB([wcs], [wcsini], [have_wcslib_lib=yes], [have_wcslib_lib=no])
  AC_MSG_RESULT([$have_wcslib_lib])

  AS_IF([test "x$have_wcslib" != "xyes" || test "x$have_wcslib_lib" != "xyes"], [
    AC_MSG_ERROR([--with-fits-wcs requested but WCSLIB not found])
  ])

  AC_DEFINE([WITH_FITS_WCS], [1], [Defined if building with FITS WCS enabled])
  ASDF_OPTIONAL="${ASDF_OPTIONAL}fits-wcs "
])


# Enable/check doc build requirements
ASDF_CHECK_SPHINX

# Check for Python--used to generate some of the tests if available
AC_PATH_PROG([PYTHON3], [python3], [no])
AM_CONDITIONAL([HAVE_PYTHON], [test "$PYTHON3" != "no"])
AC_SUBST([PYTHON3])

AC_SUBST([ASDF_CFLAGS])
AC_SUBST([ASDF_LDFLAGS])
AC_CONFIG_HEADERS([config.h]) # use config.h instead of passing -D in the command line
AC_CONFIG_FILES([Makefile include/Makefile tests/Makefile third_party/Makefile docs/Makefile])

AC_OUTPUT

# Report
CC_VERSION=`$CC --version | head -n 1`
FYAML_VERSION=`$PKG_CONFIG --modversion libfyaml`
printf "\n===================== Configuration Summary =====================\n"
printf "  %18s %s\n" "$PACKAGE_NAME version:" "$VERSION"
printf "  %18s %s\n" "compiler version:" "$CC_VERSION"
printf "  %18s %s\n" "libfyaml version:" "$FYAML_VERSION"
AS_IF([test "x$ASDF_BUILD_MODE" != "x"], [
  printf "  %18s %s\n" "build:" "$ASDF_BUILD_MODE"
])
AS_IF([test "x$ASDF_OPTIONAL" != "x"], [
  printf "  %18s %s\n" "optional:" "$ASDF_OPTIONAL"
])
printf "=================================================================\n\n"
