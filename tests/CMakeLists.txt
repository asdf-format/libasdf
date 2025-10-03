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

# -------- Run optional shell command tests ----------------------------------
if (BASH_PROGRAM AND ENABLE_TESTING_SHELL)
    foreach(shell_file ${shell_files})
        file(REAL_PATH ${shell_file} shell_name)
        string(REGEX REPLACE ${ext_pattern} "" shell_name ${shell_file})
        add_test (${shell_name} ${BASH_PROGRAM} ${shell_file})
        set_property(TEST ${shell_name}
            PROPERTY ENVIRONMENT_MODIFICATION "${runtime}")
    endforeach()
endif()

# -------- Build and run C unit tests ----------------------------------------
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


# -------- Build and run optional C++ header test ----------------------------
if (ENABLE_TESTING_CPP)
    enable_language(CXX)

    if (NOT CMAKE_CXX_COMPILER)
        message(FATAL_ERROR "C++ compiler required for ENABLE_TESTING_CPP but none found")
    endif()

    set(GENERATE_CPP_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_test_cpp_headers.sh)
    set(CPP_HEADER_TEST ${CTEST_BINARY_DIRECTORY}/test-cpp-headers.cpp)
    file(GLOB ASDF_PUBLIC_HEADERS ${CMAKE_SOURCE_DIR}/include/asdf/*.h)

    add_custom_command(
        OUTPUT ${CPP_HEADER_TEST}
        COMMAND ${GENERATE_CPP_HEADERS} ${CMAKE_SOURCE_DIR}/include ${CPP_HEADER_TEST}
        DEPENDS ${ASDF_PUBLIC_HEADERS} ${GENERATE_CPP_HEADERS}
        COMMENT "Generating C++ header test file"
        VERBATIM
    )

    add_executable(test-cpp-headers ${CPP_HEADER_TEST})
    target_link_libraries(test-cpp-headers PRIVATE libasdf)
    target_compile_features(test-cpp-headers PRIVATE cxx_std_17)
    add_test(NAME test-cpp-headers COMMAND test-cpp-headers)
endif()


# -------- Build and run optional doc tests ----------------------------------
if(ENABLE_TESTING_DOCS)
    find_package(Python3 COMPONENTS Interpreter REQUIRED)
    set(DOC_EXAMPLES_DIR ${CMAKE_CURRENT_BINARY_DIR}/doc_examples)
    # For now explicitly list files containing extractable doc examples; the
    # only one currently is README.rst
    set(DOC_FILES ${CMAKE_SOURCE_DIR}/README.rst)
    file(MAKE_DIRECTORY ${DOC_EXAMPLES_DIR})
    set(EXTRACT_DOC_EXAMPLES ${CMAKE_CURRENT_SOURCE_DIR}/scripts/extract_doc_examples.py)
    set(TEST_DOC_EXAMPLES ${DOC_EXAMPLES_DIR}/test-doc-examples.sh)

    # Just list the names of the doctests that should be built
    execute_process(
        COMMAND ${Python3_EXECUTABLE} ${EXTRACT_DOC_EXAMPLES}
                --list-tests ${DOC_FILES}
        OUTPUT_VARIABLE DOC_TEST_LIST
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    string(REGEX REPLACE "\n" ";" DOC_TESTS "${DOC_TEST_LIST}")

    set(DOC_TEST_SOURCES "")
    foreach (doctest IN LISTS DOC_TESTS)
        list(APPEND DOC_TEST_SOURCES ${DOC_EXAMPLES_DIR}/${doctest}.c)
    endforeach()

    # This command will actually build the tests; in this case the
    # test-doc-examples.sh script it outputs is just being used as a stamp
    # file (it was originally needed for the automake version of this test)
    add_custom_command(
        OUTPUT ${TEST_DOC_EXAMPLES}
        COMMAND ${Python3_EXECUTABLE}
                ${EXTRACT_DOC_EXAMPLES}
                ${DOC_FILES}
                --out-dir ${DOC_EXAMPLES_DIR}
                --fixtures-dir ${CMAKE_CURRENT_SOURCE_DIR}/fixtures
        DEPENDS ${DOC_FILES} ${EXTRACT_DOC_EXAMPLES} libasdf
        BYPRODUCTS ${DOC_TEST_SOURCES}
        COMMENT "Generating doc example sources"
        VERBATIM
    )

    add_custom_target(extract-doc-examples
        DEPENDS ${TEST_DOC_EXAMPLES}
    )

    foreach (doctest IN LISTS DOC_TESTS)
        set(source_file ${DOC_EXAMPLES_DIR}/${doctest}.c)
        set(exe_file ${DOC_EXAMPLES_DIR}/${doctest})
        add_executable(${doctest} ${source_file})
        target_include_directories(${doctest} PRIVATE ${CMAKE_SOURCE_DIR}/include)
        target_link_libraries(${doctest} PRIVATE libasdf)
        add_dependencies(${doctest} extract-doc-examples)
        # All the tests are currently hard-coded to work with this specific
        # test file; may change that later if we add any more of these
        # doctests
        add_test(
            NAME ${doctest}
            COMMAND ${doctest} ${CMAKE_CURRENT_BINARY_DIR}/fixtures/cube.asdf
        )
    endforeach()
endif()
