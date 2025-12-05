project(stc)
add_library(stc OBJECT
        ${STC_DIR}/src/cstr_core.c
)
target_link_libraries(stc m)
target_include_directories(stc PRIVATE
        STC/include
)
