# Configure config.h
include(CheckCSourceCompiles)
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

macro(check_endian_decl func)
    string(TOUPPER ${func} var_name)
    set(var_name "HAVE_DECL_${var_name}")
    check_source_runs(C "
        #include <stdio.h>
        #include <${ENDIAN_H}>
        int main() {
        #ifdef ${func}
            puts(\"YES\");
            return 0;
        #else
            puts(\"NO\");
            return 1;
        #endif
        }
    " ${var_name})
endmacro()

check_endian_decl(be64toh)
check_endian_decl(be32toh)
check_endian_decl(htobe16)
check_endian_decl(htobe32)
check_endian_decl(htobe64)
check_endian_decl(le32toh)


check_function_exists(strptime HAVE_STRPTIME)


# Check for userfaultfd (for lazy decompression support, Linux only currently)
check_include_file("linux/userfaultfd.h" HAVE_LINUX_USERFAULTFD_H)
check_c_source_compiles("
    #include <linux/userfaultfd.h>
    int main() {
        struct uffdio_api api;
        api.api = UFFD_API;
        return 0;
    }
" HAVE_USERFAULTFD_API)
check_c_source_compiles("
    #include <sys/syscall.h>
    int main() {
        long n = SYS_userfaultfd;
        return (int)n;
    }
" HAVE_DECL_SYS_USERFAULTFD)


# Check for libbsd md5.h support
check_include_file(md5.h HAVE_MD5_H)
if(HAVE_MD5_H)
    check_function_exists(MD5Init HAVE_MD5INIT)
    if(NOT HAVE_MD5INIT)
        # Try with libbsd explicitly linked
        message(STATUS "MD5Init may require linking with libmd, testing...")
        unset(HAVE_MD5INIT CACHE)
        set(CMAKE_REQUIRED_LIBRARIES md)
        check_function_exists(MD5Init HAVE_MD5INIT)
        unset(CMAKE_REQUIRED_LIBRARIES)
        if(HAVE_MD5INIT)
            set(MD5_LIBRARIES "md" CACHE INTERNAL "libraries for MD5 support")
        endif()
    endif()
endif()

if(HAVE_MD5INIT)
    set(HAVE_MD5 1)
endif()

if(BZIP2_FOUND)
    set(HAVE_BZIP2 1)
endif()

if(LZ4_FOUND)
    set(HAVE_LZ4 1)
endif()

if(ZLIB_FOUND)
    set(HAVE_ZLIB 1)
endif()

if(STATGRAB_FOUND)
    set(HAVE_STATGRAB 1)
endif()

if (HAVE_LINUX_USERFAULTFD_H AND HAVE_USERFAULTFD_API)
    set(HAVE_USERFAULTFD 1)
else()
    set(HAVE_USERFAULTFD 0)
endif()

# Configure include directories
include_directories(${CMAKE_SOURCE_DIR}/include ${CMAKE_BINARY_DIR}/include)

# Write out the header
configure_file(config.h.cmake include/config.h @ONLY)
