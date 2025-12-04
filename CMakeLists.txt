cmake_minimum_required(VERSION 3.21)

# Internal full package version
set(PACKAGE_VERSION 0.1.0a1)

# CMake-compatible project version
set(PROJECT_VERSION 0.1.0)

project(libasdf
    VERSION ${PROJECT_VERSION}
    LANGUAGES C
)

# Global includes
include(GNUInstallDirs)
include(FindPkgConfig)

# Local includes
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(SubmoduleInit)

set(ASDFSTD_DIR ${CMAKE_SOURCE_DIR}/asdf-standard)
set(STC_DIR ${CMAKE_SOURCE_DIR}/third_party/STC)
set(MUNIT_DIR ${CMAKE_SOURCE_DIR}/tests/munit)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(PACKAGE_NAME "${PROJECT_NAME}")

# Update git submodules
list(APPEND submodules
    asdf-standard
    third_party/STC
    tests/munit
)
submodule_init(${submodules})

include(ASDFOptions)
include(ASDFDependencies)
include(ASDFConfig)

add_subdirectory(third_party)
add_subdirectory(src)
add_subdirectory(include)
if(ENABLE_DOCS)
	add_subdirectory(docs)
endif()
if(ENABLE_TESTING)
	enable_testing()
	add_subdirectory(tests)
endif()

install(
    FILES
        LICENSE
	CHANGES.rst
	DESTINATION ${CMAKE_INSTALL_DATADIR}/${PROJECT_NAME}
)


