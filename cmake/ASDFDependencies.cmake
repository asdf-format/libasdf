function(setup_dependency var_prefix lib_name findpkg_name pkgconfig_name default_state req)
    if (req)
        set(required REQUIRED)
    endif()
    option(${var_prefix}_NO_PKGCONFIG ${default_state})
    # Ubuntu decided not to provide a ${var_prefix} pkg-config file
    # Uses the find_package function instead.
    if(${var_prefix}_NO_PKGCONFIG)
        set(${var_prefix}_LIBRARIES "${lib_name}" PARENT_SCOPE)
        set(${var_prefix}_LIBDIR "" CACHE STRING "Directory containing ${var_prefix} library" PARENT_SCOPE)
        set(${var_prefix}_INCLUDEDIR "" CACHE STRING "Directory containing ${var_prefix} headers" PARENT_SCOPE)
        set(${var_prefix}_CFLAGS "" CACHE STRING "Compiler options for ${var_prefix}" PARENT_SCOPE)
        set(${var_prefix}_LDFLAGS "" CACHE STRING "Linker options for ${var_prefix}" PARENT_SCOPE)
        link_directories(${${var_prefix}_LIBDIR})
        include_directories(${${var_prefix}_INCLUDEDIR})
        add_link_options(${${var_prefix}_LDFLAGS})
        add_compile_options(${${var_prefix}_CFLAGS})
    else()
        if (NOT findpkg_name STREQUAL "")
            find_package(${findpkg_name} ${required})
        endif()
        if(NOT ${var_prefix}_FOUND)
            if(PKG_CONFIG_FOUND)
                pkg_check_modules(${var_prefix} ${pkgconfig_name} ${required})
            else()
                message("pkg-config not found. Install pkg-config, or use ${var_prefix}_NO_PKGCONFIG=YES.")
            endif()
        endif()
    endif()
endfunction()


setup_dependency(FYAML fyaml "" libfyaml OFF TRUE)
if(APPLE)
    setup_dependency(ARGP argp "" libargp OFF TRUE)
endif()

setup_dependency(BZIP2 bz2 BZip2 bzip2 OFF FALSE)
setup_dependency(LZ4 lz4 "" liblz4 OFF FALSE)
setup_dependency(ZLIB z ZLIB zlib OFF FALSE)


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