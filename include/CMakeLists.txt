install(
    FILES
        asdf.h

    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    FILES
        asdf/util.h
        asdf/parse.h
        asdf/extension.h
        asdf/yaml.h
        asdf/file.h
        asdf/value.h
        asdf/log.h
        asdf/event.h

    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/asdf"
)

install(
    FILES
        asdf/core/history_entry.h
        asdf/core/asdf.h
        asdf/core/extension_metadata.h
        asdf/core/software.h
        asdf/core/ndarray.h

    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/asdf/core"
)
