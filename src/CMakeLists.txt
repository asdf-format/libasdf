project(libasdf)

set(libasdf_sources
    compression/bzp2.c
    compression/compressor_registry.c
    compression/compression.c
    compression/lz4.c
    compression/zlib.c
    core/asdf.c
    core/extension_metadata.c
    core/history_entry.c
    core/ndarray.c
    core/ndarray_convert.c
    core/software.c
    block.c
    context.c
    error.c
    emitter.c
    event.c
    extension_registry.c
    extension_util.c
    file.c
    info.c
    log.c
    parse_util.c
    parser.c
    stream.c
    util.c
    value.c
    value_util.c
    yaml.c
)

# Add optional gwcs extension sources
if (WITH_GWCS)
    set(libasdf_sources
	${libasdf_sources}
        gwcs/fitswcs_imaging.c
        gwcs/frame2d.c
        gwcs/frame.c
        gwcs/frame_celestial.c
        gwcs/step.c
        gwcs/transform.c
        gwcs/transforms/property/bounding_box.c
        gwcs/wcs.c
    )
endif()

# Generate an object library
# i.e. build the source code once, and reuse it elsewhere
add_library(libasdf_objs OBJECT ${libasdf_sources})
set_property(TARGET libasdf_objs PROPERTY POSITION_INDEPENDENT_CODE 1)

# Generate a shared library (always)
add_library(libasdf SHARED $<TARGET_OBJECTS:libasdf_objs>)

target_link_libraries(libasdf PUBLIC
    stc
    ${FYAML_LIBRARIES} ${BZIP2_LIBRARIES} ${ZLIB_LIBRARIES} ${LZ4_LIBRARIES} ${STATGRAB_LIBRARIES}
)

target_link_directories(libasdf PUBLIC
    ${BZIP2_LIBDIR}
    ${FYAML_LIBDIR}
    ${LZ4_LIBDIR}
    ${STATGRAB_LIBDIR}
    ${ZLIB_LIBDIR}
)

target_include_directories(libasdf_objs PUBLIC
    ${CMAKE_SOURCE_DIR}/third_party/STC/include
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
)

target_include_directories(libasdf_objs SYSTEM PRIVATE
    ${BZIP2_INCLUDEDIR}
    ${FYAML_INCLUDEDIR}
    ${LZ4_INCLUDEDIR}
    ${STATGRAB_INCLUDEDIR}
    ${ZLIB_INCLUDEDIR}
)

target_compile_options(libasdf_objs PUBLIC
    ${BZIP2_CFLAGS}
    ${FYAML_CFLAGS}
    ${LZ4_CFLAGS}
    ${STATGRAB_CFLAGS}
    ${ZLIB_CFLAGS}
)

# Avoid generating "liblibasdf"
set_target_properties(libasdf ${PROJECT_NAME} PROPERTIES
    SOVERSION ${PROJECT_SOVERSION}
    OUTPUT_NAME "asdf")

install(TARGETS libasdf
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)


# Optionally generate a static library
if(ENABLE_STATIC)
add_library(libasdf_static STATIC $<TARGET_OBJECTS:libasdf_objs>)
set_target_properties(libasdf_static ${PROJECT_NAME} PROPERTIES OUTPUT_NAME "asdf")
install(TARGETS libasdf_static
    DESTINATION ${CMAKE_INSTALL_LIBDIR}
)
endif()

# Generate CLI program
project(asdf-cli)
add_executable(asdf-cli main.c)
target_link_libraries(asdf-cli PRIVATE libasdf stc ${ARGP_LIBRARIES})
target_link_directories(asdf-cli PRIVATE ${ARGP_LIBDIR})
target_include_directories(asdf-cli PRIVATE
    ${STC_DIR}/include
    ${CMAKE_BINARY_DIR}/include
    ${CMAKE_CURRENT_BINARY_DIR}
    ${ARGP_INCLUDEDIR}
)

# Set CLI program's name on-disk
set_target_properties(asdf-cli ${PROJECT_NAME} PROPERTIES OUTPUT_NAME "asdf")
install(TARGETS asdf-cli
    DESTINATION ${CMAKE_INSTALL_BINDIR}
)


