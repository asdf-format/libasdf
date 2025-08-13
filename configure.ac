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

if test "x$enable_debug" = "xyes"; then
  ASDF_CFLAGS="$ASDF_CFLAGS -g -O0"
  AC_DEFINE([DEBUG], [1], [Define if debugging is enabled])
fi

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

if test "x$with_asan" = "xyes"; then
  ASDF_CFLAGS="$ASDF_CFLAGS -O1 -g -fsanitize=address -fno-omit-frame-pointer"
  ASDF_LDFLAGS="$ASDF_LDFLAGS -fsanitize=address"
  AC_DEFINE([WITH_ASAN], [1], [Defined if building with AddressSanitizer])
fi

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

AC_SUBST([ASDF_CFLAGS])
AC_SUBST([ASDF_LDFLAGS])
AC_CONFIG_HEADERS([config.h]) # use config.h instead of passing -D in the command line
AC_CONFIG_FILES([Makefile include/Makefile tests/Makefile third_party/Makefile])

AC_OUTPUT

# Report
CC_VERSION=`$CC --version | head -n 1`
FYAML_VERSION=`$PKG_CONFIG --modversion libfyaml`
printf "\n==================== Configuration Summary ====================\n"
printf "  %18s %s\n" "$PACKAGE_NAME version:" "$VERSION"
printf "  %18s %s\n" "compiler version:" "$CC_VERSION"
printf "  %18s %s\n" "libfyaml version:" "$FYAML_VERSION"
printf "===============================================================\n\n"
