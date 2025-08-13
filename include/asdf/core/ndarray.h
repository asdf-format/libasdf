/**
 * Minimalistic prototype implementation of the core/ndarray-1.1.0 schema
 *
 * Support is not yet fully complete.  What is implemented:
 *
 * It can provide direct access to the raw data of an ndarray (via a ``void *``); users must use
 * the metadata provided in the `asdf_ndarray_t` struct to interpret the data.  However, data
 * can also be copied as tiles using the `asdf_ndarray_read_tile_ndim` and
 * `asdf_ndarray_read_tile_2d` functions.
 *
 * * ASDF internal block sources
 * * All int and most float data types (other data types can be read but are not fully implemented)
 * * ``shape``, ``byteorder``, and ``offset``
 * * ``strides`` are read but not yet supported in read operations
 *
 * What is not yet supported:
 *
 * * Shape containing '*'
 * * Reading ``complex64`` or ``complex128``, or ``float16`` datatypes
 * * Reading string datatypes (``ascii`` or ``ucs4``)
 * * Structured datatypes (these are not even parsed, and currently returns an error indicating
     lack of support)
 * * Reading arbitrarily strided data
 * * Reading inline array ``data``
 * * Masks are not parsed or used at all, whether simple mask values or mask arrays (though if
     present a warning is logged indicating lack of support)
 *
 * The current limitations are purely aritificial--it is so that we can rapidly develop the minimal
 * viable product needed to make ASDF ndarray data available specifically to SourceXtractor++ in
 * order to implement issue #24: https://github.com/asdf-format/libasdf/issues/24
 * Complete ndarray support will follow gradually.
 */
#ifndef ASDF_CORE_NDARRAY_H
#define ASDF_CORE_NDARRAY_H

#include <stddef.h>
#include <stdint.h>

#include <asdf/extension.h>
#include <asdf/util.h>

ASDF_BEGIN_DECLS

/*
 * Enum for basic ndarray datatypes
 *
 * The special datatype `ASDF_DATATYPE_RECORD` is reserved for the case where the datatype is a
 * structured record (not yet supported beyond setting this datatype).
 *
 * This should not be confused with `asdf_value_t` which are the scalar value types supported for
 * YAML tree values.
 */
typedef enum {
    ASDF_DATATYPE_UNKNOWN = 0, /* Reserved for invalid/unsupported datatypes */
    ASDF_DATATYPE_INT8,
    ASDF_DATATYPE_UINT8,
    ASDF_DATATYPE_INT16,
    ASDF_DATATYPE_UINT16,
    ASDF_DATATYPE_INT32,
    ASDF_DATATYPE_UINT32,
    ASDF_DATATYPE_INT64,
    ASDF_DATATYPE_UINT64,
    ASDF_DATATYPE_FLOAT16,
    ASDF_DATATYPE_FLOAT32,
    ASDF_DATATYPE_FLOAT64,
    ASDF_DATATYPE_COMPLEX64,
    ASDF_DATATYPE_COMPLEX128,
    ASDF_DATATYPE_BOOL8,
    ASDF_DATATYPE_ASCII,
    ASDF_DATATYPE_UCS4,
    ASDF_DATATYPE_RECORD
} asdf_datatype_t;


typedef enum {
    ASDF_BYTEORDER_BIG = '>',
    ASDF_BYTEORDER_LITTLE = '<'
} asdf_byteorder_t;


/* Error codes for reading ndarray data */
typedef enum {
    ASDF_NDARRAY_OK = 0,
    ASDF_NDARRAY_ERR_OUT_OF_BOUNDS,
    ASDF_NDARRAY_ERR_OOM,
    ASDF_NDARRAY_ERR_INVAL,
} asdf_ndarray_err_t;


/* Partial definition of the `asdf_ndarray_t` type
 *
 * For convenience some basic fields are made public for now, though this may not be ABI-stable
 * in future releases.
 */
#ifndef ASDF_CORE_NDARRAY_INTERNAL
typedef struct asdf_ndarray {
    size_t source;
    uint32_t ndim;
    uint64_t *shape;
    asdf_datatype_t datatype;
    asdf_byteorder_t byteorder;
    uint64_t offset;
    int64_t *strides;
} asdf_ndarray_t;
#else
typedef struct asdf_ndarray asdf_ndarray_t;
#endif


ASDF_DECLARE_EXTENSION(ndarray, asdf_ndarray_t);


/* ndarray methods */
ASDF_EXPORT void *asdf_ndarray_data_raw(asdf_ndarray_t *ndarray, size_t *size);
ASDF_EXPORT asdf_ndarray_err_t asdf_ndarray_read_tile_ndim(
    asdf_ndarray_t *ndarray, const uint64_t *origin, const uint64_t *shape, void **out);
ASDF_EXPORT asdf_ndarray_err_t asdf_ndarray_read_tile_2d(
    asdf_ndarray_t *ndarray, uint64_t x, uint64_t y, uint64_t width, uint64_t height,
    const uint64_t *plane_origin, void **out);

ASDF_END_DECLS

#endif /* ASDF_CORE_NDARRAY_H */
