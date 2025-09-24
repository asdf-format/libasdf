find_program(BASH_PROGRAM bash)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR}/tests)
set(CTEST_BINARY_DIRECTORY ${CMAKE_BINARY_DIR}/tests)
set(nix_gnu_cflags ${CMAKE_C_FLAGS} -Wall -Wextra)
set(nix_clang_cflags ${CMAKE_C_FLAGS} -Wall -Wextra)
set(win_msvc_cflags ${CMAKE_C_FLAGS} /Wall)

file(GLOB source_files "test-*.c")
file(GLOB shell_files "test-*.sh")
set(ext_pattern "(^.*/|\\.[^.]*$)")

file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

# Build munit static library
add_library(munit ${MUNIT_DIR}/munit.c)

# Build util static library
set(REFERENCE_FILES_DIR "${ASDFSTD_DIR}/reference_files")
set(FIXTURES_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fixtures")

add_library(util util.c)
add_compile_definitions(REFERENCE_FILES_DIR=\"${REFERENCE_FILES_DIR}\")
add_compile_definitions(FIXTURES_DIR=\"${FIXTURES_DIR}\")

set(runtime "WITH_CMAKE=set:YES;OBJC_DISABLE_INITIALIZE_FORK_SAFETY=set:YES;srcdir=set:${CMAKE_CURRENT_SOURCE_DIR};top_srcdir=set:${CMAKE_SOURCE_DIR};top_builddir=set:${CMAKE_BINARY_DIR}")

if (BASH_PROGRAM AND ENABLE_TESTING_SHELL)
    foreach(shell_file ${shell_files})
        file(REAL_PATH ${shell_file} shell_name)
        string(REGEX REPLACE ${ext_pattern} "" shell_name ${shell_file})
        add_test (${shell_name} ${BASH_PROGRAM} ${shell_file})
        set_property(TEST ${shell_name}
		PROPERTY ENVIRONMENT_MODIFICATION "${runtime}")
    endforeach()
endif()

set(ASDFSTD_DIR PARENT_SCOPE)

foreach(source_file ${source_files})
    string(REGEX REPLACE ${ext_pattern} "" test_executable ${source_file})
    add_executable(${test_executable} ${source_file})
    if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${test_executable} PRIVATE ${nix_cflags} ${nix_gnu_cflags})
    elseif (CMAKE_C_COMPILER_ID MATCHES "Clang")
        target_compile_options(${test_executable} PRIVATE ${nix_cflags} ${nix_clang_cflags})
    elseif (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
        target_compile_options(${test_executable} PRIVATE ${win_cflags} ${win_msvc_cflags})
    endif()

    target_include_directories(${test_executable} PRIVATE
	    ${FYAML_INCLUDEDIR}
	    ${ARGP_INCLUDEDIR}
	    ${STC_DIR}/include
	    ${CMAKE_SOURCE_DIR}/include
            ${CMAKE_SOURCE_DIR}/src
            ${CMAKE_CURRENT_SOURCE_DIR}/include
	    ${CMAKE_CURRENT_BINARY_DIR}/include
    )
    target_link_libraries(${test_executable} PRIVATE
	    libasdf
	    munit
	    util
    )
    add_test(${test_executable} ${test_executable})
    set_tests_properties(${test_executable}
        PROPERTIES TIMEOUT 240)
    set_tests_properties(${test_executable}
        PROPERTIES
        SKIP_RETURN_CODE 127)

    set(ENV{OBJC_DISABLE_INITIALIZE_FORK_SAFETY} "YES")
    set_property(TEST ${test_executable}
        PROPERTY ENVIRONMENT_MODIFICATION "${runtime}")
endforeach()
