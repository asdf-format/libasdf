# Configure options for ASDF library
include(AddressAnalyzer)

# Logging (mirrors the Autotools --enable-logging / --enable-log-color /
# --with-log-default / --with-log-min options)
set(_ASDF_LOG_LEVELS TRACE DEBUG INFO WARN ERROR FATAL NONE)

# Compile libasdf's own log statements into the library.  When OFF,
# ASDF_LOG_ENABLED is left undefined and the log statements compile to nothing.
option(ENABLE_LOG "Compile in libasdf's internal log statements" ON)
if(ENABLE_LOG)
    set(ASDF_LOG_ENABLED ON)
endif()

# Colorized log output
option(ENABLE_LOG_COLOR "Enable colored log output" ON)
if(ENABLE_LOG_COLOR)
    set(ASDF_LOG_COLOR ON)
endif()

# Default runtime log level: the threshold used when none is set explicitly via
# asdf_config_t or the ASDF_LOG_LEVEL environment variable.
set(LOG_DEFAULT WARN CACHE STRING "Default runtime log level (one of ${_ASDF_LOG_LEVELS})")
if(NOT LOG_DEFAULT IN_LIST _ASDF_LOG_LEVELS)
    message(FATAL_ERROR "LOG_DEFAULT must be one of: ${_ASDF_LOG_LEVELS}")
endif()

# Compile-time minimum log level: messages below this level are compiled out.
set(LOG_MIN TRACE CACHE STRING "Compile-time minimum log level (one of ${_ASDF_LOG_LEVELS})")
if(NOT LOG_MIN IN_LIST _ASDF_LOG_LEVELS)
    message(FATAL_ERROR "LOG_MIN must be one of: ${_ASDF_LOG_LEVELS}")
endif()

set(ASDF_DEBUG OFF CACHE BOOL "Enable DEBUG code")

# Additional feature flags
option(ENABLE_STATIC "Build as a static library" OFF)

# Build the asdf command-line tool (depends on argp)
option(ENABLE_TOOL "Build the asdf command-line tool" ON)

# Documentation
option(ENABLE_DOCS OFF)
if (ENABLE_DOCS)
    set(SPHINX_FLAGS "-W" CACHE STRING "Flags to pass to sphinx-build")
endif ()

# Testing
option(ENABLE_TESTING "Enable unit tests" OFF)
option(ENABLE_TESTING_SHELL "Enable additional shell command tests" OFF)
option(ENABLE_TESTING_CPP "Enable testing linkage with C++" OFF)
option(ENABLE_TESTING_DOCS "Enable testing doc examples" OFF)
option(ENABLE_TESTING_ALL "Enable all tests (unit, shell, etc.)" OFF)

if(ENABLE_TESTING_ALL)
    set(ENABLE_TESTING YES CACHE BOOL "" FORCE)
    set(ENABLE_TESTING_SHELL YES CACHE BOOL "" FORCE)
    set(ENABLE_TESTING_CPP YES CACHE BOOL "" FORCE)
    set(ENABLE_TESTING_DOCS YES CACHE BOOL "" FORCE)
endif()

# Distribution
set(CPACK_PACKAGE_VENDOR "STScI")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
        \\.git/
        \\.github/
        \\.idea/
        "cmake-.*/"
        build/
        ".*~$"
)
set(CPACK_VERBATIM_VARIABLES YES)
include(CPack)
