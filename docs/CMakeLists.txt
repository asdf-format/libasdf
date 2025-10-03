set(SPHINX_FLAGS PARENT_SCOPE)

set(docsrc "${CMAKE_SOURCE_DIR}/docs")
set(outdir "${CMAKE_CURRENT_BINARY_DIR}/html")

add_custom_target(docs_clean
    COMMAND cmake -E remove_directory "${outdir}"
)

add_custom_target(docs
    COMMAND ${SPHINX_BUILD_PROG} -v ${SPHINX_FLAGS} -b html ${docsrc} "${outdir}"
)

add_custom_target(docs_rebuild
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target docs_clean
    COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR} --target docs
)

install(DIRECTORY "${outdir}"
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
)