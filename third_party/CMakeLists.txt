project(stc)
add_library(stc STATIC
    ${STC_DIR}/src/fmt.c
    ${STC_DIR}/src/csview.c
    ${STC_DIR}/src/cstr_io.c
    ${STC_DIR}/src/cspan.c
    ${STC_DIR}/src/cstr_core.c
    ${STC_DIR}/src/cstr_utf8.c
    ${STC_DIR}/src/random.c
    ${STC_DIR}/src/cregex.c
    ${STC_DIR}/src/stc_core.c

)
set_property(TARGET stc PROPERTY POSITION_INDEPENDENT_CODE 1)
# Set explicitly rather than relying on the top-level add_compile_options:
# We also need to add it here to prevent the libstc statically linked into
# libasdf from leaking symbols externally.
# This is set similarly in `third_party/Makefile.am` by appending
# -fvisibility=hidden explicitly to the CFLAGS for libstc.
set_property(TARGET stc PROPERTY C_VISIBILITY_PRESET hidden)
target_link_libraries(stc PUBLIC m)
target_include_directories(stc PRIVATE
        STC/include
)
install(
    FILES STC/LICENSE
    RENAME LICENSE_STC.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}
)
