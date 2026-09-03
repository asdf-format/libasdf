AC_PREREQ([2.69])
AC_INIT(
  [libasdf], [0.1.0rc2],
  [https://github.com/asdf-format/libasdf/issues],
  [],
  [https://github.com/asdf-format/libasdf],
)
AC_CONFIG_AUX_DIR([build-aux])
AC_CANONICAL_HOST
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
AX_CHECK_ENDIAN_DECL([be64toh])
AX_CHECK_ENDIAN_DECL([be32toh])
AX_CHECK_ENDIAN_DECL([htobe16])
AX_CHECK_ENDIAN_DECL([htobe32])
AX_CHECK_ENDIAN_DECL([htobe64])
AX_CHECK_ENDIAN_DECL([le32toh])
AX_CHECK_ENDIAN_DECL([htole32])

# Check functions
AC_CHECK_FUNCS([strptime])

# Check for usable _Float16 for datatype support
# Requires that `isinf` works, as that is used in some of the datatype
# conversion routines.
AC_MSG_CHECKING([for usable _Float16])
AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM([[
    #include <math.h>
  ]], [[
    _Float16 h = 1.5f16;
    float f = (float)h; /* widen */
    h = (_Float16)f;    /* narrow */
    int i = (int)h;     /* round-to-int */
    (void)i; (void)isinf(f);
  ]])],
  [have_float16=yes],
  [have_float16=no]
)
AC_MSG_RESULT([$have_float16])
AS_IF([test "x$have_float16" = "xyes"], [
  AC_DEFINE([HAVE_FLOAT16], [1],
            [Define to 1 if the _Float16 type and its conversions are usable])
])

# Check for userfaultfd (for lazy decompression support, Linux only currently)
AC_CHECK_HEADERS([linux/userfaultfd.h], [], [])
AC_COMPILE_IFELSE(
  [AC_LANG_PROGRAM([[
    #include <linux/userfaultfd.h>
  ]], [[
    struct uffdio_api api;
    api.api = UFFD_API;
  ]])],
  [have_userfaultfd=yes],
  [have_userfaultfd=no]
)
AM_CONDITIONAL([HAVE_USERFAULTFD], [test "x$have_userfaultfd" = "xyes"])
AS_IF([test "x$have_userfaultfd" = "xyes"], [
  AC_DEFINE([HAVE_USERFAULTFD], [1],
            [Define to 1 if userfaultfd support is available])
])
AC_CHECK_DECLS([SYS_userfaultfd], [], [], [[#include <sys/syscall.h>]])

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

# Check whether the linker supports --wrap (GNU ld / lld only; Apple ld does not)
AC_MSG_CHECKING([whether linker supports -Wl,--wrap])
save_LDFLAGS="$LDFLAGS"
LDFLAGS="$LDFLAGS -Wl,--wrap=malloc"
AC_LINK_IFELSE([AC_LANG_PROGRAM([], [])],
  [have_linker_wrap=yes],
  [have_linker_wrap=no])
LDFLAGS="$save_LDFLAGS"
AC_MSG_RESULT([$have_linker_wrap])
AM_CONDITIONAL([HAVE_LINKER_WRAP], [test "x$have_linker_wrap" = "xyes"])


# Additional enable flags
# --enable-debug
AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug], [Enable debug build (-g -O0)])],
  [],
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
  [],
  [with_asan=no])

AS_IF([test "x$with_asan" = "xyes"], [
  ASDF_CFLAGS="$ASDF_CFLAGS -O1 -g -fsanitize=address -fno-omit-frame-pointer"
  ASDF_LDFLAGS="$ASDF_LDFLAGS -fsanitize=address"
  AC_DEFINE([WITH_ASAN], [1], [Defined if building with AddressSanitizer])
  ASDF_BUILD_MODE="${ASDF_BUILD_MODE}asan "
])

AM_CONDITIONAL([ASAN], [test "x$with_asan" = "xyes"])

AM_CONDITIONAL([HOST_LINUX], [case "$host_os" in linux*) true;; *) false;; esac])

# --with-ubsan
AC_ARG_WITH([ubsan],
  [AS_HELP_STRING([--with-ubsan], [Build with UndefinedBehaviorSanitizer support])],
  [],
  [with_ubsan=no])

AS_IF([test "x$with_ubsan" = "xyes"], [
  ASDF_CFLAGS="$ASDF_CFLAGS -O1 -g -fsanitize=undefined -fno-omit-frame-pointer"
  ASDF_LDFLAGS="$ASDF_LDFLAGS -fsanitize=undefined"
  AC_DEFINE([WITH_UBSAN], [1], [Defined if building with UndefinedBehaviorSanitizer])
  ASDF_BUILD_MODE="${ASDF_BUILD_MODE}ubsan "
])

AM_CONDITIONAL([UBSAN], [test "x$with_ubsan" = "xyes"])

# Logging flags
AC_ARG_ENABLE([logging],
  [AS_HELP_STRING([--enable-logging], [Enable logging (default: yes)])],
  [enable_logging=$enableval],
  [enable_logging=yes]
)

AC_ARG_ENABLE([log-color],
  [AS_HELP_STRING([--enable-log-color],
                  [Enable color output in logs (default: yes)])],
  [enable_log_color=$enableval],
  [enable_log_color=yes]
)

AC_ARG_WITH([log-default],
  [AS_HELP_STRING([--with-log-default=LEVEL], [Set default runtime log level])],
  [AS_CASE([$with_log_default],
    [TRACE|DEBUG|INFO|WARN|ERROR|FATAL|NONE], [],
    [AC_MSG_ERROR([Invalid log level: $with_log_default])])],
  [with_log_default=WARN]
)

AC_ARG_WITH([log-min],
  [AS_HELP_STRING([--with-log-min=LEVEL], [Set compile-time minimum log level])],
  [AS_CASE([$with_log_min],
    [TRACE|DEBUG|INFO|WARN|ERROR|FATAL|NONE], [],
    [AC_MSG_ERROR([Invalid min log level: $with_log_min])])],
  [with_log_min=TRACE]
)


# Substitutions for the installed include/asdf/config.h.  Unlike the internal
# config.h this is limited to the macros that are part of the public API, and
# is written with plain @VAR@ substitutions so that the same template can be
# used by the CMake build.
AS_IF([test "x$have_float16" = "xyes"],
  [ASDF_HAVE_FLOAT16_DEFINE="#define ASDF_HAVE_FLOAT16 1"],
  [ASDF_HAVE_FLOAT16_DEFINE="/* #undef ASDF_HAVE_FLOAT16 */"])
AC_SUBST([ASDF_HAVE_FLOAT16_DEFINE])

AS_IF([test "x$enable_logging" = "xyes"],
  [ASDF_LOG_ENABLED_DEFINE="#define ASDF_LOG_ENABLED 1"],
  [ASDF_LOG_ENABLED_DEFINE="/* #undef ASDF_LOG_ENABLED */"])
AC_SUBST([ASDF_LOG_ENABLED_DEFINE])

AS_IF([test "x$enable_log_color" = "xyes"],
  [ASDF_LOG_COLOR_DEFINE="#define ASDF_LOG_COLOR 1"],
  [ASDF_LOG_COLOR_DEFINE="/* #undef ASDF_LOG_COLOR */"])
AC_SUBST([ASDF_LOG_COLOR_DEFINE])

ASDF_LOG_DEFAULT_LEVEL_DEFINE="#define ASDF_LOG_DEFAULT_LEVEL ASDF_LOG_$with_log_default"
AC_SUBST([ASDF_LOG_DEFAULT_LEVEL_DEFINE])

ASDF_LOG_MIN_LEVEL_DEFINE="#define ASDF_LOG_MIN_LEVEL ASDF_LOG_$with_log_min"
AC_SUBST([ASDF_LOG_MIN_LEVEL_DEFINE])


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

dnl Workaround for https://github.com/pantoniou/libfyaml/pull/275
dnl This can be deleted if libfyaml is fixed upstream, though system
dnl packagers are likely to fix this too
FYAML_LIBS=$(printf '%s\n' "$FYAML_LIBS" | sed 's/none required//g')

# Check for libstatgrab
AC_ARG_WITH([libstatgrab],
    [AS_HELP_STRING([--with-libstatgrab],
        [Use libstatgrab for system memory info (default: yes)])],
    [with_libstatgrab=$withval],
    [with_libstatgrab=yes])

AS_IF([test "x$with_libstatgrab" = "xyes"], [
  PKG_CHECK_MODULES([STATGRAB], [libstatgrab], [
    AC_DEFINE([HAVE_STATGRAB], [1], [Define if libstatgrab is available])
  ], [
    AC_MSG_ERROR([libstatgrab is required but was not found])
  ])
], [
    AC_MSG_NOTICE([libstatgrab disabled])
])

# Compression libraries
PKG_CHECK_MODULES([ZLIB], [zlib], [], [
  AC_MSG_ERROR([zlib is required but was not found])
])

# Annoyingly my system does not provide a pc file for bzip2, so fall back on
# checking without pkg-config (should probably do same for the other deps)
PKG_CHECK_MODULES([BZIP2], [bzip2], [], [
  AC_CHECK_HEADER([bzlib.h], [have_bzip2_h=yes], [have_bzip2_h=no])
  AC_CHECK_LIB([bz2], [BZ2_bzCompressInit],
    [have_bzip2_lib=yes; BZIP2_LIBS="-lbz2"],
    [have_bzip2_lib=no])

  AS_IF([test "x$have_bzip2_h" = xyes && test "x$have_bzip2_lib" = xyes], [
    AC_DEFINE([HAVE_BZIP2], [1], [Define if bzip2 is available])
    BZIP2_CFLAGS=""
  ], [
    AC_MSG_ERROR([bz2 is required but was not found])
  ])
])


PKG_CHECK_MODULES([LZ4], [liblz4], [], [
  AC_MSG_ERROR([liblz4 is required but was not found])
])


# On macOS libmd comes from Homebrew, whose include/lib directories are not on
# the compiler's default search path.  This has to come *before*
# the md5.h check below.
ASDF_CHECK_HOMEBREW_PKG([libmd])

# Detect support for md5.h from libmd (on older systems this came from libbsd,
# which now depends on libmd for it);
# should try other options as well e.g. openssl
AC_CHECK_HEADERS([md5.h], [have_md5_h=yes], [have_md5_h=no])
AS_IF([test "x$have_md5_h" = "xyes"], [
    AC_CHECK_FUNCS([MD5Init], [have_md5=yes], [have_md5=no])
    AS_IF([test "x$have_md5" = "xno"], [
        AC_MSG_NOTICE([MD5Init not found, trying with -lmd])
        unset ac_cv_func_MD5Init
        save_LIBS="$LIBS"
        LIBS="-lmd $LIBS"
        AC_CHECK_FUNCS([MD5Init], [have_md5=yes], [have_md5=no])
        LIBS="$save_LIBS"
        AS_IF([test "x$have_md5" = "xyes"], [
            md5_libs="-lmd"
        ])
    ])
], [
    AC_MSG_WARN([libmd is required for MD5 support; compiling without checksum support])
])

AS_IF([test "x$have_md5" = "xyes"], [
    AC_DEFINE([HAVE_MD5], [1], [Define if MD5 support is available])

    # Detect an MD5Final that is reachable *without* libmd.
    #
    # So, what is this BS?
    #
    # macOS's libcommonCrypto, which comes along with libSystem, exports a
    # real MD5Final function (its MD5Init/MD5Update are only #defines onto
    # CC_MD5_Init/CC_MD5_Update, so those do not collide).  Its context type
    # is not md5.h's MD5_CTX, so if the linker resolves MD5Final there the
    # caller's context gets scribbled over. We cannot do anything at link
    # time to force macOS's linker to resolve the symbol from the correct
    # library.
    #
    # What's worse, the libcommonCrypto version is marked by Apple as
    # *deprecated* and warns loudly about this if you compile against it. Yes,
    # Apple, we are all adults who understand MD5 is not cryptographically
    # secure, but it's still a commonly used checksum!
    #
    # So where a competing MD5Final exists at all we avoid naming the symbol;
    # see asdf_md5_final in src/block.c.
    #
    # Deliberately checked by linking *without* $md5_libs: if this succeeds
    # then something other than libmd is providing MD5Final.  Should Apple
    # ever drop it, this check stops firing on its own.
    AC_CACHE_CHECK([whether MD5Final is provided outside libmd],
      [asdf_cv_md5final_shadowed],
      [AC_LINK_IFELSE(
        [AC_LANG_PROGRAM([[#include <md5.h>]],
                         [[MD5_CTX ctx; unsigned char digest[16]; MD5Final(digest, &ctx);]])],
        [asdf_cv_md5final_shadowed=yes],
        [asdf_cv_md5final_shadowed=no])])

    AS_IF([test "x$asdf_cv_md5final_shadowed" = "xyes"], [
        # The workaround needs MD5Pad, which is part of the same md5.h API
        save_LIBS="$LIBS"
        LIBS="$md5_libs $LIBS"
        AC_CACHE_CHECK([for MD5Pad in libmd], [asdf_cv_func_md5pad],
          [AC_LINK_IFELSE(
            [AC_LANG_PROGRAM([[#include <md5.h>]], [[MD5_CTX ctx; MD5Pad(&ctx);]])],
            [asdf_cv_func_md5pad=yes],
            [asdf_cv_func_md5pad=no])])
        LIBS="$save_LIBS"

        AS_IF([test "x$asdf_cv_func_md5pad" = "xyes"], [
            AC_DEFINE([ASDF_MD5_FINAL_WORKAROUND], [1],
                      [Define if MD5Final must be open-coded because a
                       conflicting MD5Final is provided outside libmd])
        ], [
            AC_MSG_WARN([a conflicting MD5Final was found but MD5Pad is not
                         available to work around it; block checksums may be
                         wrong or crash])
        ])
    ])
])
AM_CONDITIONAL([HAVE_MD5], [test "x$have_md5" = "xyes"])

AC_SUBST([MD5_LIBS], [$md5_libs])

# Check for homebrew packages in macOS
AS_IF([test "x$asdf_build_tool" = "xyes"], [
  ASDF_CHECK_HOMEBREW_PKG([argp-standalone])

# Shouldn't be needed with glibc but is if we're using the homebrew package, e.g.
  AC_CHECK_LIB([argp], [argp_parse], [
    AC_SUBST([ARGP_LIBS], [-largp])
  ])
])

ASDF_LIBS="$FYAML_LIBS $ZLIB_LIBS $BZIP2_LIBS $LZ4_LIBS $STATGRAB_LIBS $MD5_LIBS"
AC_SUBST([ASDF_LIBS])

# ------ Optional features ---------------------------------------------------

# Enable/check doc build requirements
ASDF_CHECK_SPHINX

# Check for Python--used to generate some of the tests if available
AC_PATH_PROG([PYTHON3], [python3], [no])
AM_CONDITIONAL([HAVE_PYTHON], [test "$PYTHON3" != "no"])
AC_SUBST([PYTHON3])

AC_SUBST([ASDF_CFLAGS])
AC_SUBST([ASDF_LDFLAGS])
AC_CONFIG_HEADERS([config.h]) # use config.h instead of passing -D in the command line

# The ASDF_LOG_* and ASDF_HAVE_FLOAT16 macros are part of the installed public
# configuration (include/asdf/config.h); pull them in here rather than
# duplicating the definitions in the internal config.h.
AH_BOTTOM([#include "asdf/config.h"])
AC_CONFIG_FILES([Makefile include/Makefile tests/Makefile third_party/Makefile docs/Makefile libasdf.pc include/asdf/config.h])

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
