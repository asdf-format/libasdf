project(stc)
add_library(stc STATIC
        ${STC_DIR}/src/cregex.c
        ${STC_DIR}/src/cspan.c
        ${STC_DIR}/src/cstr_core.c
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
