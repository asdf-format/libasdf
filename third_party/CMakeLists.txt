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
target_link_libraries(stc PUBLIC m)
target_include_directories(stc PRIVATE
        STC/include
)
install(
    FILES STC/LICENSE
    RENAME LICENSE_STC.txt DESTINATION ${CMAKE_INSTALL_DATADIR}/${CMAKE_PROJECT_NAME}
)
