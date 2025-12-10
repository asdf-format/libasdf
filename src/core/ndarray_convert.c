/**
 * Implements data type conversion routines for copying ndarray data.
 *
 * This is moved out of the main ``src/ndarray.c`` file to reduce noise, as there are
 * many different combinations of conversion functions.
 */

#include <float.h>
#include <math.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "../util.h"

#include "ndarray.h"
#include "ndarray_convert.h"


static inline uint16_t bswap_uint16_t(uint16_t x) {
    return __builtin_bswap16(x);
}
static inline uint32_t bswap_uint32_t(uint32_t x) {
    return __builtin_bswap32(x);
}
static inline uint64_t bswap_uint64_t(uint64_t x) {
    return __builtin_bswap64(x);
}
static inline float bswap_float(float x) {
    /* Use memcpy to avoid strict aliasing issues */
    uint32_t v;
    memcpy(&v, &x, sizeof(v));
    v = bswap_uint32_t(v);
    memcpy(&x, &v, sizeof(x));
    return x;
}
static inline double bswap_double(double x) {
    /* Use memcpy to avoid strict aliasing issues */
    uint64_t v;
    memcpy(&v, &x, sizeof(v));
    v = bswap_uint64_t(v);
    memcpy(&x, &v, sizeof(x));
    return x;
}

#define bswap_int8_t(x) (x)
#define bswap_uint8_t(x) (x)
#define bswap_int16_t(x) ((int16_t)bswap_uint16_t((uint16_t)(x)))
#define bswap_int32_t(x) ((int32_t)bswap_uint32_t((uint32_t)(x)))
#define bswap_int64_t(x) ((int64_t)bswap_uint64_t((uint64_t)(x)))

#define _DO_BSWAP_0(src_t, val) val = (val) /* no-op */
#define _DO_BSWAP_1(src_t, val) val = bswap_##src_t(val)


// TODO: For all these conversion functions also pass in an argument specifying whether the src
// and dst are aligned; then we can use __builtin_assume_aligned here though need to
// have a clear routine for checking it first
typedef void (*asdf_ndarray_tile_convert_fn_t)(
    void *restrict dst, const void *restrict src, size_t n);


/**
 * Dispatch table for datatype conversion functions
 *
 * The third dimension is for whether or not to perform byteswap.
 */
static asdf_ndarray_convert_fn_t conversion_table[ASDF_DATATYPE_RECORD][ASDF_DATATYPE_RECORD][2] = {
    0};
static atomic_bool conversion_table_initialized = false;


/**
 * Defines a conversion function like convert_int16_to_int32_bswap
 * for converting int16_t to int32_t values and byteswapping.  Pass 1 to
 * add byteswap, and 0 when no byte order conversion is needed.
 */
#define _DEFINE_GENERIC_CONV_FN(src_t, dst_t, name, bswap) \
    static int convert_##name(void *dst, const void *src, size_t n, UNUSED(size_t elsize)) { \
        dst_t *_dst = (dst_t *)dst; \
        const src_t *_src = (const src_t *)src; \
        for (size_t idx = 0; idx < n; idx++) { \
            src_t val = _src[idx]; \
            _DO_BSWAP_##bswap(src_t, val); \
            _dst[idx] = (dst_t)val; \
        } \
        return 0; \
    }


/**
 * Defines a conversion function like convert_int32_to_int16_bswap
 * for converting int32_t to int16_t values, where values below INT16_MIN are
 * capped to INT16_MIN and similar for values above INT16_MAX, with optional
 * byteswapping.  Pass 1 to add byteswap, and 0 when no byte order conversion
 * is needed.
 */
#define _DEFINE_CLAMP_CONV_FN(src_t, dst_t, name, bswap, minval, maxval) \
    static int convert_##name(void *dst, const void *src, size_t n, UNUSED(size_t elsize)) { \
        dst_t *_dst = (dst_t *)dst; \
        const src_t *_src = (const src_t *)src; \
        int overflow = 0; \
        for (size_t idx = 0; idx < n; idx++) { \
            src_t val = _src[idx]; \
            _DO_BSWAP_##bswap(src_t, val); \
            if (val < (src_t)(minval)) { \
                _dst[idx] = minval; \
                overflow = 1; \
            } else if (val > (src_t)(maxval)) { \
                _dst[idx] = maxval; \
                overflow = 1; \
            } else { \
                _dst[idx] = (dst_t)val; \
            } \
        } \
        return overflow; \
    }


/** Special case for conversion between floats, to preserve infinities */
#define _DEFINE_CLAMP_FLOAT_CONV_FN(src_t, dst_t, name, bswap, minval, maxval) \
    static int convert_##name(void *dst, const void *src, size_t n, UNUSED(size_t elsize)) { \
        dst_t *_dst = (dst_t *)dst; \
        const src_t *_src = (const src_t *)src; \
        int overflow = 0; \
        for (size_t idx = 0; idx < n; idx++) { \
            src_t val = _src[idx]; \
            _DO_BSWAP_##bswap(src_t, val); \
            if (isinf(val)) { \
                _dst[idx] = (dst_t)val; \
            } else if (val < (src_t)(minval)) { \
                _dst[idx] = minval; \
                overflow = 1; \
            } else if (val > (src_t)(maxval)) { \
                _dst[idx] = maxval; \
                overflow = 1; \
            } else { \
                _dst[idx] = (dst_t)val; \
            } \
        } \
        return overflow; \
    }


#define _DEFINE_CLAMP_MAX_CONV_FN(src_t, dst_t, name, bswap, maxval) \
    static int convert_##name(void *dst, const void *src, size_t n, UNUSED(size_t elsize)) { \
        dst_t *_dst = (dst_t *)dst; \
        const src_t *_src = (const src_t *)src; \
        int overflow = 0; \
        for (size_t idx = 0; idx < n; idx++) { \
            src_t val = _src[idx]; \
            _DO_BSWAP_##bswap(src_t, val); \
            if (val > (src_t)(maxval)) { \
                _dst[idx] = maxval; \
                overflow = 1; \
            } else { \
                _dst[idx] = (dst_t)val; \
            } \
        } \
        return overflow; \
    }


/**
 * Like _DEFINE_CLAMP_CONV_FN but used only for converting signed integers to
 * unsigned integers of the same or greater width, truncating negative values to zero.
 */
#define _DEFINE_TRUNCATE_CONV_FN(src_t, dst_t, name, bswap) \
    static int convert_##name(void *dst, const void *src, size_t n, UNUSED(size_t elsize)) { \
        dst_t *_dst = (dst_t *)dst; \
        const src_t *_src = (const src_t *)src; \
        int overflow = 0; \
        for (size_t idx = 0; idx < n; idx++) { \
            src_t val = _src[idx]; \
            _DO_BSWAP_##bswap(src_t, val); \
            if (val < (src_t)(0)) { \
                val = (src_t)(0); \
                overflow = 1; \
            } \
            _dst[idx] = (dst_t)val; \
        } \
        return overflow; \
    }


#define DEFINE_CONVERSION(src_name, src_t, dst_name, dst_t) \
    _DEFINE_GENERIC_CONV_FN(src_t, dst_t, src_name##_to_##dst_name, 0) \
    _DEFINE_GENERIC_CONV_FN(src_t, dst_t, src_name##_to_##dst_name##_bswap, 1)


#define DEFINE_CLAMP_CONVERSION(src_name, src_t, dst_name, dst_t, minval, maxval) \
    _DEFINE_CLAMP_CONV_FN(src_t, dst_t, src_name##_to_##dst_name, 0, minval, maxval) \
    _DEFINE_CLAMP_CONV_FN(src_t, dst_t, src_name##_to_##dst_name##_bswap, 1, minval, maxval)


#define DEFINE_CLAMP_FLOAT_CONVERSION(src_name, src_t, dst_name, dst_t, minval, maxval) \
    _DEFINE_CLAMP_FLOAT_CONV_FN(src_t, dst_t, src_name##_to_##dst_name, 0, minval, maxval) \
    _DEFINE_CLAMP_FLOAT_CONV_FN(src_t, dst_t, src_name##_to_##dst_name##_bswap, 1, minval, maxval)


#define DEFINE_CLAMP_MAX_CONVERSION(src_name, src_t, dst_name, dst_t, maxval) \
    _DEFINE_CLAMP_MAX_CONV_FN(src_t, dst_t, src_name##_to_##dst_name, 0, maxval) \
    _DEFINE_CLAMP_MAX_CONV_FN(src_t, dst_t, src_name##_to_##dst_name##_bswap, 1, maxval)


#define DEFINE_TRUNCATE_CONVERSION(src_name, src_t, dst_name, dst_t) \
    _DEFINE_TRUNCATE_CONV_FN(src_t, dst_t, src_name##_to_##dst_name, 0) \
    _DEFINE_TRUNCATE_CONV_FN(src_t, dst_t, src_name##_to_##dst_name##_bswap, 1)


#define DEFINE_IDENTITY_CONVERSION(src_name, src_t) \
    static int convert_##src_name##_to_##src_name( \
        void *dst, const void *src, size_t n, size_t elsize) { \
        memcpy(dst, src, n *elsize); \
        return 0; \
    } \
    _DEFINE_GENERIC_CONV_FN(src_t, src_t, src_name##_to_##src_name##_bswap, 1)


/**
 * Now we manually define which conversion functions should be used for each to/from type
 * pair.
 *
 * Would be nice if we could do this automatically but it requires some hand-written logic
 * to ensure which type of conversion function is needed.
 */

/** Conversions from int8 */
DEFINE_IDENTITY_CONVERSION(int8, int8_t)
DEFINE_CONVERSION(int8, int8_t, int16, int16_t)
DEFINE_CONVERSION(int8, int8_t, int32, int32_t)
DEFINE_CONVERSION(int8, int8_t, int64, int64_t)
DEFINE_CONVERSION(int8, int8_t, float32, float)
DEFINE_CONVERSION(int8, int8_t, float64, double)
DEFINE_TRUNCATE_CONVERSION(int8, int8_t, uint8, uint8_t)
DEFINE_TRUNCATE_CONVERSION(int8, int8_t, uint16, uint16_t)
DEFINE_TRUNCATE_CONVERSION(int8, int8_t, uint32, uint32_t)
DEFINE_TRUNCATE_CONVERSION(int8, int8_t, uint64, uint64_t)

/** Conversions from uint8 */
DEFINE_IDENTITY_CONVERSION(uint8, uint8_t)
DEFINE_CONVERSION(uint8, uint8_t, int16, int16_t)
DEFINE_CONVERSION(uint8, uint8_t, uint16, uint16_t)
DEFINE_CONVERSION(uint8, uint8_t, int32, int32_t)
DEFINE_CONVERSION(uint8, uint8_t, uint32, uint32_t)
DEFINE_CONVERSION(uint8, uint8_t, int64, int64_t)
DEFINE_CONVERSION(uint8, uint8_t, uint64, uint64_t)
DEFINE_CONVERSION(uint8, uint8_t, float32, float)
DEFINE_CONVERSION(uint8, uint8_t, float64, double)
DEFINE_CLAMP_MAX_CONVERSION(uint8, uint8_t, int8, int8_t, INT8_MAX)

/** Conversions from int16 */
DEFINE_IDENTITY_CONVERSION(int16, int16_t)
DEFINE_CONVERSION(int16, int16_t, int32, int32_t)
DEFINE_CONVERSION(int16, int16_t, int64, int64_t)
DEFINE_CONVERSION(int16, int16_t, float32, float)
DEFINE_CONVERSION(int16, int16_t, float64, double)
DEFINE_CLAMP_CONVERSION(int16, int16_t, int8, int8_t, INT8_MIN, INT8_MAX)
DEFINE_CLAMP_CONVERSION(int16, int16_t, uint8, uint8_t, 0, UINT8_MAX)
DEFINE_TRUNCATE_CONVERSION(int16, int16_t, uint16, uint16_t)
DEFINE_TRUNCATE_CONVERSION(int16, int16_t, uint32, uint32_t)
DEFINE_TRUNCATE_CONVERSION(int16, int16_t, uint64, uint64_t)

/** Conversions from uint16 */
DEFINE_IDENTITY_CONVERSION(uint16, uint16_t)
DEFINE_CONVERSION(uint16, uint16_t, int32, int32_t)
DEFINE_CONVERSION(uint16, uint16_t, uint32, uint32_t)
DEFINE_CONVERSION(uint16, uint16_t, int64, int64_t)
DEFINE_CONVERSION(uint16, uint16_t, uint64, uint64_t)
DEFINE_CONVERSION(uint16, uint16_t, float32, float)
DEFINE_CONVERSION(uint16, uint16_t, float64, double)
DEFINE_CLAMP_MAX_CONVERSION(uint16, uint16_t, int8, int8_t, INT8_MAX)
DEFINE_CLAMP_MAX_CONVERSION(uint16, uint16_t, uint8, uint8_t, UINT8_MAX)
DEFINE_CLAMP_MAX_CONVERSION(uint16, uint16_t, int16, int16_t, INT16_MAX)

/** Conversions from int32 */
DEFINE_IDENTITY_CONVERSION(int32, int32_t)
DEFINE_CONVERSION(int32, int32_t, int64, int64_t)
DEFINE_CONVERSION(int32, int32_t, float32, float)
DEFINE_CONVERSION(int32, int32_t, float64, double)
DEFINE_CLAMP_CONVERSION(int32, int32_t, int8, int8_t, INT8_MIN, INT8_MAX)
DEFINE_CLAMP_CONVERSION(int32, int32_t, uint8, uint8_t, 0, UINT8_MAX)
DEFINE_CLAMP_CONVERSION(int32, int32_t, int16, int16_t, INT16_MIN, INT16_MAX)
DEFINE_CLAMP_CONVERSION(int32, int32_t, uint16, uint16_t, 0, UINT16_MAX)
DEFINE_TRUNCATE_CONVERSION(int32, int32_t, uint32, uint32_t)
DEFINE_TRUNCATE_CONVERSION(int32, int32_t, uint64, uint64_t)

/** Conversions from uint32 */
DEFINE_IDENTITY_CONVERSION(uint32, uint32_t)
DEFINE_CONVERSION(uint32, uint32_t, int64, int64_t)
DEFINE_CONVERSION(uint32, uint32_t, uint64, uint64_t)
DEFINE_CONVERSION(uint32, uint32_t, float32, float)
DEFINE_CONVERSION(uint32, uint32_t, float64, double)
DEFINE_CLAMP_MAX_CONVERSION(uint32, uint32_t, int8, int8_t, INT8_MAX)
DEFINE_CLAMP_MAX_CONVERSION(uint32, uint32_t, uint8, uint8_t, UINT8_MAX)
DEFINE_CLAMP_MAX_CONVERSION(uint32, uint32_t, int16, int16_t, INT16_MAX)
DEFINE_CLAMP_MAX_CONVERSION(uint32, uint32_t, uint16, uint16_t, UINT16_MAX)
DEFINE_CLAMP_MAX_CONVERSION(uint32, uint32_t, int32, int32_t, INT32_MAX)

/** Conversions from int64 */
DEFINE_IDENTITY_CONVERSION(int64, int64_t)
DEFINE_CONVERSION(int64, int64_t, float32, float)
DEFINE_CONVERSION(int64, int64_t, float64, double)
DEFINE_CLAMP_CONVERSION(int64, int64_t, int8, int8_t, INT8_MIN, INT8_MAX)
DEFINE_CLAMP_CONVERSION(int64, int64_t, uint8, uint8_t, 0, UINT8_MAX)
DEFINE_CLAMP_CONVERSION(int64, int64_t, int16, int16_t, INT16_MIN, INT16_MAX)
DEFINE_CLAMP_CONVERSION(int64, int64_t, uint16, uint16_t, 0, UINT16_MAX)
DEFINE_CLAMP_CONVERSION(int64, int64_t, int32, int32_t, INT32_MIN, INT32_MAX)
DEFINE_CLAMP_CONVERSION(int64, int64_t, uint32, uint32_t, 0, UINT32_MAX)
DEFINE_TRUNCATE_CONVERSION(int64, int64_t, uint64, uint64_t)

/** Conversions from uint64 */
DEFINE_IDENTITY_CONVERSION(uint64, uint64_t)
DEFINE_CONVERSION(uint64, uint64_t, float32, float)
DEFINE_CONVERSION(uint64, uint64_t, float64, double)
DEFINE_CLAMP_MAX_CONVERSION(uint64, uint64_t, int8, int8_t, INT8_MAX)
DEFINE_CLAMP_MAX_CONVERSION(uint64, uint64_t, uint8, uint8_t, UINT8_MAX)
DEFINE_CLAMP_MAX_CONVERSION(uint64, uint64_t, int16, int16_t, INT16_MAX)
DEFINE_CLAMP_MAX_CONVERSION(uint64, uint64_t, uint16, uint16_t, UINT16_MAX)
DEFINE_CLAMP_MAX_CONVERSION(uint64, uint64_t, int32, int32_t, INT32_MAX)
DEFINE_CLAMP_MAX_CONVERSION(uint64, uint64_t, uint32, uint32_t, UINT32_MAX)
DEFINE_CLAMP_MAX_CONVERSION(uint64, uint64_t, int64, int64_t, INT64_MAX)

/** Conversions from float32 */
DEFINE_IDENTITY_CONVERSION(float32, float)
DEFINE_CONVERSION(float32, float, float64, double)
DEFINE_CLAMP_CONVERSION(float32, float, int8, int8_t, INT8_MIN, INT8_MAX)
DEFINE_CLAMP_CONVERSION(float32, float, uint8, uint8_t, 0, UINT8_MAX)
DEFINE_CLAMP_CONVERSION(float32, float, int16, int16_t, INT16_MIN, INT16_MAX)
DEFINE_CLAMP_CONVERSION(float32, float, uint16, uint16_t, 0, UINT16_MAX)
DEFINE_CLAMP_CONVERSION(float32, float, int32, int32_t, INT32_MIN, INT32_MAX)
DEFINE_CLAMP_CONVERSION(float32, float, uint32, uint32_t, 0, UINT32_MAX)
DEFINE_CLAMP_CONVERSION(float32, float, int64, int64_t, INT64_MIN, INT64_MAX)
DEFINE_CLAMP_CONVERSION(float32, float, uint64, uint64_t, 0, UINT64_MAX)

/** Conversions from float64 */
DEFINE_IDENTITY_CONVERSION(float64, double)
DEFINE_CLAMP_FLOAT_CONVERSION(float64, double, float32, float, -FLT_MAX, FLT_MAX)
DEFINE_CLAMP_CONVERSION(float64, double, int8, int8_t, INT8_MIN, INT8_MAX)
DEFINE_CLAMP_CONVERSION(float64, double, uint8, uint8_t, 0, UINT8_MAX)
DEFINE_CLAMP_CONVERSION(float64, double, int16, int16_t, INT16_MIN, INT16_MAX)
DEFINE_CLAMP_CONVERSION(float64, double, uint16, uint16_t, 0, UINT16_MAX)
DEFINE_CLAMP_CONVERSION(float64, double, int32, int32_t, INT32_MIN, INT32_MAX)
DEFINE_CLAMP_CONVERSION(float64, double, uint32, uint32_t, 0, UINT32_MAX)
DEFINE_CLAMP_CONVERSION(float64, double, int64, int64_t, INT64_MIN, INT64_MAX)
DEFINE_CLAMP_CONVERSION(float64, double, uint64, uint64_t, 0, UINT64_MAX)


/** I am very sorry in advance to anyone who has to read this */
#define FOR_NUMERIC_TYPES(X) \
    X(ASDF_DATATYPE_INT8, int8) \
    X(ASDF_DATATYPE_UINT8, uint8) \
    X(ASDF_DATATYPE_INT16, int16) \
    X(ASDF_DATATYPE_UINT16, uint16) \
    X(ASDF_DATATYPE_INT32, int32) \
    X(ASDF_DATATYPE_UINT32, uint32) \
    X(ASDF_DATATYPE_INT64, int64) \
    X(ASDF_DATATYPE_UINT64, uint64) \
    X(ASDF_DATATYPE_FLOAT32, float32) \
    X(ASDF_DATATYPE_FLOAT64, float64)


#define FOR_NUMERIC_TYPES_EXPAND(X, src_enum, src_name) \
    X(src_enum, src_name, ASDF_DATATYPE_INT8, int8) \
    X(src_enum, src_name, ASDF_DATATYPE_UINT8, uint8) \
    X(src_enum, src_name, ASDF_DATATYPE_INT16, int16) \
    X(src_enum, src_name, ASDF_DATATYPE_UINT16, uint16) \
    X(src_enum, src_name, ASDF_DATATYPE_INT32, int32) \
    X(src_enum, src_name, ASDF_DATATYPE_UINT32, uint32) \
    X(src_enum, src_name, ASDF_DATATYPE_INT64, int64) \
    X(src_enum, src_name, ASDF_DATATYPE_UINT64, uint64) \
    X(src_enum, src_name, ASDF_DATATYPE_FLOAT32, float32) \
    X(src_enum, src_name, ASDF_DATATYPE_FLOAT64, float64)


#define REGISTER_CONVERSION_FOR_PAIR(src_enum, src_name, dst_enum, dst_name) \
    conversion_table[src_enum][dst_enum][false] = convert_##src_name##_to_##dst_name; \
    conversion_table[src_enum][dst_enum][true] = convert_##src_name##_to_##dst_name##_bswap;


#define REGISTER_CONVERSION_FOR_SRC(src_enum, src_name) \
    FOR_NUMERIC_TYPES_EXPAND(REGISTER_CONVERSION_FOR_PAIR, src_enum, src_name)


ASDF_CONSTRUCTOR static void asdf_conversion_table_init() {
    if (atomic_load_explicit(&conversion_table_initialized, memory_order_acquire))
        return;

    FOR_NUMERIC_TYPES(REGISTER_CONVERSION_FOR_SRC);

    atomic_store_explicit(&conversion_table_initialized, true, memory_order_release);
}


asdf_ndarray_convert_fn_t asdf_ndarray_get_convert_fn(
    asdf_scalar_datatype_t src_t, asdf_scalar_datatype_t dst_t, bool byteswap) {
    if (src_t < ASDF_DATATYPE_INT8 || src_t > ASDF_DATATYPE_RECORD || dst_t < ASDF_DATATYPE_INT8 ||
        dst_t > ASDF_DATATYPE_RECORD)
        return NULL;

    return conversion_table[src_t][dst_t][byteswap];
}
