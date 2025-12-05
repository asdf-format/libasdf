install(
    FILES
        asdf.h

    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)

install(
    FILES
        asdf/event.h
        asdf/extension.h
        asdf/file.h
        asdf/log.h
        asdf/parser.h
        asdf/util.h
        asdf/value.h
        asdf/yaml.h

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

if (WITH_GWCS)
    install(
        FILES
            asdf/gwcs/core.h
            asdf/gwcs/fitswcs_imaging.h
            asdf/gwcs/frame2d.h
            asdf/gwcs/frame_celestial.h
            asdf/gwcs/gwcs.h
            asdf/gwcs/step.h
            asdf/gwcs/transform.h
            asdf/gwcs/wcs.h

        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/asdf/gwcs"
    )

    install(
        FILES
            asdf/gwcs/transforms/property/bounding_box.h

        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/asdf/gwcs/transforms/property"
    )
endif()
