# Configure options for ASDF library
include(AddressAnalyzer)

if(ASDF_LOG_ENABLED)
    option(ASDF_LOG_COLOR "Enable colored log output" ON)
endif()
set(ASDF_DEFAULT_LOG_LEVEL TRACE CACHE STRING "One of TRACE|DEBUG|INFO|WARN|ERROR|FATAL|NONE")
set(ASDF_LOG_MIN_LEVEL TRACE CACHE STRING "One of TRACE|DEBUG|INFO|WARN|ERROR|FATAL|NONE")
set(ASDF_DEBUG OFF CACHE BOOL "Enable DEBUG code")

# Documentation
option(ENABLE_DOCS OFF)
if (ENABLE_DOCS)
    set(SPHINX_FLAGS "-W" CACHE STRING "Flags to pass to sphinx-build")
endif ()

# Example binaries
option(BUILD_EXAMPLES OFF)

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
