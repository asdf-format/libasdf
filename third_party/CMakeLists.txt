project(stc)
add_library(stc OBJECT
        ${STC_DIR}/src/cregex.c
        ${STC_DIR}/src/cspan.c
        ${STC_DIR}/src/cstr_core.c
        ${STC_DIR}/src/cstr_io.c
        ${STC_DIR}/src/cstr_utf8.c
        ${STC_DIR}/src/csview.c
        ${STC_DIR}/src/fmt.c
        ${STC_DIR}/src/random.c
        ${STC_DIR}/src/stc_core.c
)
target_link_libraries(stc m)
target_include_directories(stc PRIVATE
        STC/include
)
