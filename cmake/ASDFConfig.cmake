# Configure config.h
include(CheckSourceRuns)
include(CheckSymbolExists)
include(CheckFunctionExists)
include(CheckIncludeFile)

add_compile_options(-fvisibility=hidden)

if(ASDF_DEBUG)
    add_compile_definitions(DEBUG)
endif()
add_compile_definitions(HAVE_CONFIG_H)
add_compile_definitions(__USE_MISC)
add_compile_definitions(_XOPEN_SOURCE)
add_compile_definitions(_GNU_SOURCE)

if(APPLE)
    # for time.h: timegm
    add_compile_definitions(_DARWIN_C_SOURCE)
endif()

check_include_file(endian.h HAVE_ENDIAN_H)
check_include_file(machine/endian.h HAVE_MACHINE_ENDIAN_H)
check_include_file(sys/endian.h HAVE_SYS_ENDIAN_H)

if(HAVE_ENDIAN_H)
    set(ENDIAN_H endian.h)
elseif(HAVE_MACHINE_ENDIAN_H)
    set(ENDIAN_H machine/endian.h)
elseif(HAVE_SYS_ENDIAN_H)
    set(ENDIAN_H sys/endian.h)
endif()

check_source_runs(C "
    #include <stdio.h>
    #include <${ENDIAN_H}>
    int main() {
    #ifdef be64toh
        puts(\"YES\");
        return 0;
    #else
        puts(\"NO\");
        return 1;
    #endif
    }
    "
        HAVE_DECL_BE64TOH)

check_function_exists(strptime HAVE_STRPTIME)


# Write out the header
include_directories(${CMAKE_SOURCE_DIR}/include)
configure_file(config.h.cmake include/config.h @ONLY)

