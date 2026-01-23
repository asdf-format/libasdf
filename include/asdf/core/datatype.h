/**
 * .. _asdf/core/datatype.h:
 *
 * Implementation of the :ref:`stsci.edu/asdf/core/datatype-1.0.0` schema
 */

//

#ifndef ASDF_CORE_DATATYPE_H
#define ASDF_CORE_DATATYPE_H

#include <asdf/core/asdf.h>
#include <asdf/extension.h>

ASDF_BEGIN_DECLS

#define ASDF_CORE_DATATYPE_TAG ASDF_CORE_TAG_PREFIX "datatype-1.0.0"


/**
 * Enum for basic ndarray scalar datatypes
 *
 * The special datatype `ASDF_DATATYPE_STRUCTURED` is reserved for the case where
 * the datatype is a structured record (not yet supported beyond setting this
 * datatype).
 *
 * See `asdf_datatype_t` which represents a full datatype (including
 * compound/structured datatypes).
 *
 * This should not be confused with `asdf_value_t` which are the scalar value
 * types supported for YAML tree values.
 */
typedef enum {
    /** Reserved for invalid/unsupported datatypes */
    ASDF_DATATYPE_UNKNOWN = 0,
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
    /** ASCII text datatype */
    ASDF_DATATYPE_ASCII,
    /**
     * UCS4 Unicode datatype
     *
     * When using this datatype in `asdf_datatype_t` make sure to set the
     * ``.size`` field to 4 * the string field length in characters.
     */
    ASDF_DATATYPE_UCS4,
    /**
     * Indicates that a datatype is non-scalar / is a compound-type/structured array
     */
    ASDF_DATATYPE_STRUCTURED
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
 * or field in a structured datatype
 */
typedef enum {
    /** Sentinel value for an invalid byte order */
    ASDF_BYTEORDER_INVALID = -1,
    /**
     * Sentinel value for user-defined datatypes indicating that the byteorder
     * should not be explicitly written (just use the default)
     */
    ASDF_BYTEORDER_DEFAULT = 0,
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
    const uint64_t *shape;
    uint32_t nfields;
    const asdf_datatype_t *fields;
};


/**
 * Struct representing an ndarray datatype
 */
typedef struct asdf_datatype asdf_datatype_t;


ASDF_DECLARE_EXTENSION(datatype, asdf_datatype_t);


/**
 * Parse an ASDF scalar datatype and return the corresponding `asdf_scalar_datatype_t`
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
ASDF_EXPORT asdf_scalar_datatype_t asdf_scalar_datatype_from_string(const char *dtype);


/**
 * Convert an `asdf_scalar_datatype_t` to its string representation
 *
 * :param datatype: A member of `asdf_scalar_datatype_t`
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
ASDF_EXPORT const char *asdf_scalar_datatype_to_string(asdf_scalar_datatype_t datatype);


/**
 * Get the size of an `asdf_datatype_t` in bytes
 *
 * This is equivalent to looking up the public field ``asdf_datatype_t.size``.
 * However, the difference is that for user-defined datatypes it is not
 * required to set the size explicitly--in that case this computes, sets, and
 * returns ``asdf_datatype_t.size``.
 *
 * The exception is that for string datatypes (`ASDF_DATATYPE_ASCII` and
 * `ASDF_DATATYPE_UCS4`) the user *must* provide the correct size, and a size
 * of 0 is taken to mean "0-length string".
 *
 * :param datatype: An `asdf_datatype_t *`
 * :return: The size in bytes of the datatype
 */
ASDF_EXPORT uint64_t asdf_datatype_size(asdf_datatype_t *datatype);


/**
 * Get the size in bytes of a scalar (numeric) ndarray element for a given
 * `asdf_scalar_datatype_t`
 *
 * :param type: A member of `asdf_datatype_t`
 * :return: Size in bytes of a single element of that datatype, or ``-1`` for
 *   non-scalar datatypes (for the present purposes strings are not considered
 *   scalars, only numeric datatypes)
 */
static inline size_t asdf_scalar_datatype_size(asdf_scalar_datatype_t type) {
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
        // NOLINTNEXTLINE(readability-magic-numbers)
        return 8;
    case ASDF_DATATYPE_COMPLEX128:
        // NOLINTNEXTLINE(readability-magic-numbers)
        return 16;

    case ASDF_DATATYPE_ASCII:
    case ASDF_DATATYPE_UCS4:
    case ASDF_DATATYPE_STRUCTURED:
    case ASDF_DATATYPE_UNKNOWN:
        return 0;
    default:
        return 0;
    }
}

ASDF_END_DECLS

#endif /* ASDF_CORE_DATATYPE_H */
