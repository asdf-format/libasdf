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

# Testing
option(ENABLE_TESTING OFF)
option(ENABLE_TESTING_SHELL OFF)

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
