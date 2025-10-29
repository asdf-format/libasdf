add_executable(asdf_dump_ndarray asdf_dump_ndarray.c)
target_include_directories(asdf_dump_ndarray PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${BZIP2_INCLUDEDIR}
    ${LZ4_INCLUDEDIR}
    ${ZLIB_INCLUDEDIR}
)
target_link_libraries(asdf_dump_ndarray
    libasdf
    ${BZIP2_LIBRARIES}
    ${LZ4_LIBRARIES}
    ${ZLIB_LIBRARIES}
)

