install(
    FILES
        asdf.h

    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    FILES
        ${CMAKE_BINARY_DIR}/include/asdf/config.h
        asdf/emitter.h
        asdf/error.h
        asdf/event.h
        asdf/extension.h
        asdf/extension_util.h
        asdf/file.h
        asdf/log.h
        asdf/parser.h
        asdf/util.h
        asdf/value.h
        asdf/version.h
        asdf/yaml.h

    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/asdf"
)

install(
    FILES
        asdf/core/asdf.h
        asdf/core/datatype.h
        asdf/core/extension_metadata.h
        asdf/core/history_entry.h
        asdf/core/ndarray.h
        asdf/core/software.h
        asdf/core/time.h

    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/asdf/core"
)

