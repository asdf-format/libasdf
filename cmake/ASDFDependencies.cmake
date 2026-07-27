include(CheckIncludeFile)
include(CheckFunctionExists)


# asdf_check_homebrew_pkg(<PREFIX> <package>)
#
# Detect a Homebrew-installed package on macOS (or Linux if you for some
# reason use Homebrew on Linux) and populate cache-style variables analogous
# to pkg_check_modules:
#
#   <PREFIX>_FOUND       -- TRUE if the package was found via Homebrew
#   <PREFIX>_PREFIX      -- Homebrew prefix for the package
#   <PREFIX>_INCLUDEDIR  -- <prefix>/include
#   <PREFIX>_LIBDIR      -- <prefix>/lib
#
# This is a fallback for packages that ship without a pkg-config (.pc) file,
# such as the Homebrew argp-standalone formula.  It is a no-op (aside from a
# status message) if the brew program cannot be found.
function(asdf_check_homebrew_pkg prefix package)
    message(STATUS "Checking for Homebrew package '${package}'")

    find_program(BREW_PROGRAM brew)
    if(NOT BREW_PROGRAM)
        message(STATUS "  Homebrew (brew) not found; cannot check for package '${package}'")
        set(${prefix}_FOUND FALSE PARENT_SCOPE)
        return()
    endif()

    execute_process(
        COMMAND ${BREW_PROGRAM} --prefix --installed ${package}
        OUTPUT_VARIABLE brew_prefix
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE brew_result
        ERROR_QUIET
    )

    if(NOT brew_result EQUAL 0 OR brew_prefix STREQUAL "")
        message(STATUS "  Package '${package}' not found. Try: brew install ${package}")
        set(${prefix}_FOUND FALSE PARENT_SCOPE)
        return()
    endif()

    message(STATUS "  Found Homebrew package '${package}' at ${brew_prefix}")
    set(${prefix}_FOUND TRUE PARENT_SCOPE)
    set(${prefix}_PREFIX "${brew_prefix}" PARENT_SCOPE)
    set(${prefix}_INCLUDEDIR "${brew_prefix}/include" PARENT_SCOPE)
    set(${prefix}_LIBDIR "${brew_prefix}/lib" PARENT_SCOPE)
endfunction()


option(BZIP2_NO_PKGCONFIG NO)
# Ubuntu decided not to provide a bzip2 pkg-config file
# Uses the find_package function instead.
if(BZIP2_NO_PKGCONFIG)
    set(BZIP2_LIBRARIES "bz2")
    set(BZIP2_LIBDIR "" CACHE STRING "Directory containing libbz2 library")
    set(BZIP2_INCLUDEDIR "" CACHE STRING "Directory containing libbz2 headers")
    set(BZIP2_CFLAGS "" CACHE STRING "Compiler options for libbz2")
    set(BZIP2_LDFLAGS "" CACHE STRING "Linker options for libbz2")
else()
    find_package(BZip2)
endif()

option(LZ4_NO_PKGCONFIG NO)
if(LZ4_NO_PKGCONFIG)
    set(LZ4_LIBRARIES "lz4")
    set(LZ4_LIBDIR "" CACHE STRING "Directory containing lz4 library")
    set(LZ4_INCLUDEDIR "" CACHE STRING "Directory containing lz4 headers")
    set(LZ4_CFLAGS "" CACHE STRING "Compiler options for lz4")
    set(LZ4_LDFLAGS "" CACHE STRING "Linker options for lz4")
else()
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(LZ4 liblz4)
    else()
        message("pkg-config not found. Install pkg-config, or use LZ4_NO_PKGCONFIG=YES.")
    endif()
endif()

option(ZLIB_NO_PKGCONFIG NO)
if(ZLIB_NO_PKGCONFIG)
    set(ZLIB_LIBRARIES "z")
    set(ZLIB_LIBDIR "" CACHE STRING "Directory containing libz library")
    set(ZLIB_INCLUDEDIR "" CACHE STRING "Directory containing libz headers")
    set(ZLIB_CFLAGS "" CACHE STRING "Compiler options for libz")
    set(ZLIB_LDFLAGS "" CACHE STRING "Linker options for libz")
else()
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(ZLIB zlib)
    else()
        message("pkg-config not found. Install pkg-config, or use ZLIB_NO_PKGCONFIG=YES.")
    endif()
endif()

option(FYAML_NO_PKGCONFIG NO)
if(FYAML_NO_PKGCONFIG)
    set(FYAML_LIBRARIES "fyaml")
    set(FYAML_LIBDIR "" CACHE STRING "Directory containing libfyaml library")
    set(FYAML_INCLUDEDIR "" CACHE STRING "Directory containing libfyaml headers")
    set(FYAML_CFLAGS "" CACHE STRING "Compiler options for libfyaml")
    set(FYAML_LDFLAGS "" CACHE STRING "Linker options for libfyaml")
else()
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(FYAML libfyaml REQUIRED)
    else()
        message("pkg-config not found. Install pkg-config, or use FYAML_NO_PKGCONFIG=YES.")
    endif()
endif()

option(USE_STATGRAB "Use libstatgrab for memory info" ON)
option(STATGRAB_NO_PKGCONFIG "Detect libstatgrab without using pkg-config" NO)
if(STATGRAB_NO_PKGCONFIG)
    set(STATGRAB_LIBRARIES "statgrab")
    set(STATGRAB_LIBDIR "" CACHE STRING "Directory containing libstatgrab library")
    set(STATGRAB_INCLUDEDIR "" CACHE STRING "Directory containing libstatgrab headers")
    set(STATGRAB_CFLAGS "" CACHE STRING "Compiler options for libstatgrab")
    set(STATGRAB_LDFLAGS "" CACHE STRING "Linker options for libstatgrab")
else()
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(STATGRAB libstatgrab REQUIRED)
    else()
        message("pkg-config not found. Install pkg-config, or use STATGRAB_NO_PKGCONFIG=YES.")
    endif()
endif()


# MD5 support is provided by libmd, which installs <md5.h> and MD5Init.  (On
# older systems this header came from libbsd, which nowadays depends on libmd
# for it.)  libmd ships no pkg-config file, so probe for the header directly.
check_include_file(md5.h HAVE_MD5_H)

if(NOT HAVE_MD5_H)
    # On macOS check for libmd installed via Homebrew and retry the probe
    asdf_check_homebrew_pkg(MD5 libmd)
    if(MD5_FOUND)
        unset(HAVE_MD5_H CACHE)
        set(CMAKE_REQUIRED_INCLUDES "${MD5_INCLUDEDIR}")
        check_include_file(md5.h HAVE_MD5_H)
        unset(CMAKE_REQUIRED_INCLUDES)
    endif()
endif()

if(HAVE_MD5_H)
    # MD5_INCLUDEDIR/MD5_LIBDIR are empty unless libmd was located via
    # Homebrew above, in which case the probes below need those paths too.
    set(CMAKE_REQUIRED_INCLUDES "${MD5_INCLUDEDIR}")
    if(MD5_LIBDIR)
        set(CMAKE_REQUIRED_LINK_OPTIONS "-L${MD5_LIBDIR}")
    endif()

    check_function_exists(MD5Init HAVE_MD5INIT)

    if(NOT HAVE_MD5INIT)
        # Not in libc, so libmd has to be linked explicitly
        message(STATUS "MD5Init not found, trying with -lmd")
        unset(HAVE_MD5INIT CACHE)
        set(CMAKE_REQUIRED_LIBRARIES md)
        check_function_exists(MD5Init HAVE_MD5INIT)
        unset(CMAKE_REQUIRED_LIBRARIES)
        if(HAVE_MD5INIT)
            set(MD5_LIBRARIES "md" CACHE INTERNAL "libraries for MD5 support")
        endif()
    endif()

    unset(CMAKE_REQUIRED_INCLUDES)
    unset(CMAKE_REQUIRED_LINK_OPTIONS)
else()
    message(WARNING
        "libmd is required for MD5 support; compiling without checksum support")
endif()


# argp is only needed by the command-line tool
if(ENABLE_TOOL AND APPLE)
    option(ARGP_NO_PKGCONFIG NO)
    if(ARGP_NO_PKGCONFIG)
        set(ARGP_LIBRARIES "argp")
        set(ARGP_LIBDIR "" CACHE STRING "Directory containing libargp library")
        set(ARGP_INCLUDEDIR "" CACHE STRING "Directory containing libargp headers")
        set(ARGP_CFLAGS "" CACHE STRING "Compiler options for libargp")
        set(ARGP_LDFLAGS "" CACHE STRING "Linker options for libargp")
    else()
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(ARGP libargp)
        else()
            message("pkg-config not found. Install pkg-config, or use ARGP_NO_PKGCONFIG=YES.")
        endif()

        # The Homebrew argp-standalone formula currently ships without a
        # pkg-config (.pc) file, so fall back on locating it via Homebrew
        # directly.  This can be dropped once argp-standalone provides a .pc
        # file upstream; see https://github.com/asdf-format/libasdf/issues/176
        # for the ongoing saga.
        if(NOT ARGP_FOUND)
            asdf_check_homebrew_pkg(ARGP argp-standalone)
            if(ARGP_FOUND)
                set(ARGP_LIBRARIES "argp")
            endif()
        endif()

        if(NOT ARGP_FOUND)
            message(FATAL_ERROR
                "argp is required for the command-line tool but was not found. "
                "Install argp-standalone (e.g. 'brew install argp-standalone'), "
                "or disable the tool with -DENABLE_TOOL=OFF.")
        endif()
    endif()
endif()

if(ENABLE_DOCS)
    find_package(Python3 REQUIRED)
    if (PYTHON3_FOUND)
        get_filename_component(python_prefix "${Python3_EXECUTABLE}" DIRECTORY)
        set(python_bindirs
            "${python_prefix}/bin"
            "${python_prefix}/Scripts" # windows
        )
    endif()

    find_program(SPHINX_BUILD_PROG
        NAMES sphinx-build sphinx-build.exe
        HINTS ${python_bindirs}
        REQUIRED
    )
    find_package_handle_standard_args(Sphinx DEFAULT_MSG SPHINX_BUILD_PROG)
endif()
