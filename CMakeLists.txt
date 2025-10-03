cmake_minimum_required(VERSION 3.21)
project(libasdf
    VERSION 0.0.0
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
set(PACKAGE_VERSION "${PROJECT_VERSION}")

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


