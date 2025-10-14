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

#define ASDF_CORE_NDARRAY_TAG ASDF_CORE_TAG_PREFIX "ndarray-1.1.0"

/*
 * Enum for basic ndarray datatypes
 *
 * The special datatype `ASDF_DATATYPE_RECORD` is reserved for the case where the datatype is a
 * structured record (not yet supported beyond setting this datatype).
 *
 * See `asdf_datatype_t` which represents a full datatype (including compound/record datatypes).
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
} asdf_scalar_datatype_t;


/**
 * Alias for `ASDF_DATATYPE_UNKNOWN`
 *
 * This is used primarily in the `asdf_ndarray_read_tile_ndim` family of functions indicating
 * that the destination data type is the same as the source datatype.  This alias is clearer
 * in intent than `ASDF_DATATYPE_UNKNOWN` in this context.
 */
#define ASDF_DATATYPE_SOURCE ASDF_DATATYPE_UNKNOWN


/**
 * Struct representing the byte order/endianness of elements in an ndarray
 * or field in a record datatype
 */
typedef enum {
    /** Litle-endian **/
    ASDF_BYTEORDER_BIG = '>',
    /** Big-endian **/
    ASDF_BYTEORDER_LITTLE = '<'
} asdf_byteorder_t;


// Forward-declaration of asdf_datatype_t;
typedef struct asdf_datatype asdf_datatype_t;


struct asdf_datatype {
    asdf_scalar_datatype_t type;
    uint64_t size;
    const char *name;
    asdf_byteorder_t byteorder;
    uint32_t ndim;
    const size_t *shape;
    uint32_t nfields;
    const asdf_datatype_t *fields;
};


/**
 * Struct representing an ndarray datatype
 */
typedef struct asdf_datatype asdf_datatype_t;


/* Error codes for reading ndarray data */
typedef enum {
    ASDF_NDARRAY_OK = 0,
    ASDF_NDARRAY_ERR_OUT_OF_BOUNDS,
    ASDF_NDARRAY_ERR_OOM,
    ASDF_NDARRAY_ERR_INVAL,
    ASDF_NDARRAY_ERR_OVERFLOW,
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


/** ndarray methods */
ASDF_EXPORT void *asdf_ndarray_data_raw(asdf_ndarray_t *ndarray, size_t *size);

/**
 * Return the total number of elements (not bytes) in the ndarray
 *
 * :param ndarray: An `asdf_ndarray_t *`
 * :return: Total number of elements in the array (just the product of its
 *   shape)
 */
ASDF_EXPORT size_t asdf_ndarray_size(asdf_ndarray_t *ndarray);


/**
 * Read the full ndarray, copying into the provided buffer (or allocating a
 * destination buffer if ``dst = NULL``)
 *
 * This is like `asdf_ndarray_read_tile_ndim` but with a default "tile" size
 * of the full array.  Like `asdf_ndarray_read_tile_ndim` it will also convert
 * the data to the host native byte order if necessary, and can convert it to a
 * different numeric type than the source array.
 *
 * :param ndarray: The `asdf_ndarray_t *` handle to the ndarray
 * :param dst_t: An `asdf_scalar_datatype_t` to convert to, or
 *   `ASDF_DATATYPE_SOURCE` to keep the original source datatype
 * :param dst: Pointer to a destination `void *` already allocated to receive
 *   the exact number of bytes in the source ndarray, or `NULL` to indicate
 *   that a buffer should be allocated.  In the latter case the caller is
 *   responsible for freeing the allocated buffer.
 */
ASDF_EXPORT asdf_ndarray_err_t
asdf_ndarray_read_all(asdf_ndarray_t *ndarray, asdf_scalar_datatype_t dst_t, void **dst);

ASDF_EXPORT asdf_ndarray_err_t asdf_ndarray_read_tile_ndim(
    asdf_ndarray_t *ndarray,
    const uint64_t *origin,
    const uint64_t *shape,
    asdf_scalar_datatype_t dst_t,
    void **dst);

ASDF_EXPORT asdf_ndarray_err_t asdf_ndarray_read_tile_2d(
    asdf_ndarray_t *ndarray,
    uint64_t x,
    uint64_t y,
    uint64_t width,
    uint64_t height,
    const uint64_t *plane_origin,
    asdf_scalar_datatype_t dst_t,
    void **dst);


/**
 * Parse an ASDF ndarray scalar datatype and return the corresponding `asdf_scalar_datatype_t`
 *
 * :param s: Null-terminated string
 * :return: The corresponding `asdf_scalar_datatype_t` or `ASDF_DATATYPE_UNKNOWN`
 *
 * .. note::
 *
 *   Resists the urge to name this ``asdf_ndarray_serialize_datatype`` as in the long term
 *   this will be used to serialize a datatype back to YAML, and will need to also support
 *   compound datatypes.
 *
 *   This just provides the string representations for the common scalar datatypes.
 */
ASDF_EXPORT asdf_scalar_datatype_t asdf_ndarray_datatype_from_string(const char *s);


/**
 * Convert an `asdf_scalar_datatype_t` to its string representation
 *
 * :param datatype: A member of `ASDF_NDARRAY_DATATYPE`
 * :return: The string representation of the scalar datatype
 *
 * .. note::
 *
 *   Resists the urge to name this ``asdf_ndarray_serialize_datatype`` as in the long term
 *   this will be used to serialize a datatype back to YAML, and will need to also support
 *   compound datatypes.
 *
 *   This just provides the string representations for the common scalar datatypes.
 */
ASDF_EXPORT const char *asdf_ndarray_datatype_to_string(asdf_scalar_datatype_t datatype);


/**
 * Get the size in bytes of a scalar (numeric) ndarray element for a given
 * `asdf_scalar_datatype_t`
 *
 * :param type: A member of `asdf_datatype_t`
 * :return: Size in bytes of a single element of that datatype, or ``-1`` for
 *   non-scalar datatypes (for the present purposes strings are not considered
 *   scalars, only numeric datatypes)
 */
static inline size_t asdf_ndarray_scalar_datatype_size(asdf_scalar_datatype_t type) {
    switch (type) {
    case ASDF_DATATYPE_INT8:
    case ASDF_DATATYPE_UINT8:
    case ASDF_DATATYPE_BOOL8:
        return 1;
    case ASDF_DATATYPE_INT16:
    case ASDF_DATATYPE_UINT16:
    case ASDF_DATATYPE_FLOAT16:
        return 2;
    case ASDF_DATATYPE_INT32:
    case ASDF_DATATYPE_UINT32:
    case ASDF_DATATYPE_FLOAT32:
        return 4;
    case ASDF_DATATYPE_INT64:
    case ASDF_DATATYPE_UINT64:
    case ASDF_DATATYPE_FLOAT64:
    case ASDF_DATATYPE_COMPLEX64:
        return 8;
    case ASDF_DATATYPE_COMPLEX128:
        return 16;

    case ASDF_DATATYPE_ASCII:
    case ASDF_DATATYPE_UCS4:
    case ASDF_DATATYPE_RECORD:
    case ASDF_DATATYPE_UNKNOWN:
        return 0;
    default:
        return 0;
    }
}

ASDF_END_DECLS

#endif /* ASDF_CORE_NDARRAY_H */
