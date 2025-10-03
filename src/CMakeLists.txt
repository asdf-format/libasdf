project(libasdf)

add_library(libasdf STATIC)
target_sources(libasdf PUBLIC
    core/asdf.c
    core/extension_metadata.c
    core/history_entry.c
    core/ndarray.c
    core/ndarray_convert.c
    core/software.c
    block.c
    context.c
    error.c
    event.c
    extension_registry.c
    file.c
    info.c
    log.c
    parse.c
    parse_util.c
    stream.c
    value.c
    value_util.c
    yaml.c
)
target_link_libraries(libasdf PUBLIC
    stc
    ${FYAML_LIBRARIES}
)
target_include_directories(libasdf PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/STC/include
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)
# Avoid generating "liblibasdf"
set_target_properties(libasdf ${PROJECT_NAME} PROPERTIES OUTPUT_NAME "asdf")
install(TARGETS libasdf
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

project(asdf-cli)
add_executable(asdf-cli main.c)
target_link_libraries(asdf-cli PRIVATE libasdf stc ${ARGP_LIBRARIES})
target_include_directories(asdf-cli PRIVATE
    ${STC_DIR}/include
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)
set_target_properties(asdf-cli ${PROJECT_NAME} PROPERTIES OUTPUT_NAME "asdf")
install(TARGETS asdf-cli
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)


