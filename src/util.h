#pragma once

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

// Some versions of libfyaml.h started exporting its own internal
// ARRAY_SIZE macro, which it probably shouldn't do...
#include <libfyaml.h>
#ifdef ARRAY_SIZE
#undef ARRAY_SIZE
#endif

#include "asdf/util.h" // IWYU pragma: export


#if defined(__GNUC__) || defined(__clang__)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif


#if defined(__GNUC__) || defined(__clang__)
#define UNUSED(x) x __attribute__((unused))
#else
#define UNUSED(x) (void)(x)
#endif


#if defined(__GNUC__) || defined(__clang__)
#define UNREACHABLE() \
    assert(false && "unreachable"); \
    __builtin_unreachable()
#else
#define UNREACHABLE() assert(false && "unreachable")
#endif


#define FIELD_SIZEOF(t, f) (sizeof(((t *)0)->f))


/* Number of elements in a fixed-size array */
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))


/* Cast memset to volatile to prevent opt-away */
#define ZERO_MEMORY(ptr, size) \
    do { \
        void *volatile _volatile_ptr = (ptr); \
        memset(_volatile_ptr, 0, (size)); \
    } while (0)


/*
 * Read sizeof(dst) bytes from src into dst without assuming src is aligned
 *
 * Block data is mapped at whatever offset it occupies in the file, so pointers
 * into it carry that offset's alignment.  Compilers lower a fixed-size memcpy
 * to the same load a typed dereference would use, so this costs nothing.
 */
#define LOAD_UNALIGNED(dst, src) memcpy(&(dst), (src), sizeof(dst))


/*
 * Saturating uint64_t arithmetic: return UINT64_MAX instead of wrapping
 *
 * Used for sizes computed from untrusted shapes, so that a size that does not
 * fit in 64 bits fails any check against a real data size rather than wrapping
 * to a small value that passes it.  Since a saturated operand stays saturated
 * (except when multiplied by zero), a chain of these saturates if any step
 * overflows.
 */
static inline uint64_t asdf_uint64_mul_sat(uint64_t lhs, uint64_t rhs) {
    if (rhs != 0 && lhs > UINT64_MAX / rhs)
        return UINT64_MAX;

    return lhs * rhs;
}


static inline uint64_t asdf_uint64_add_sat(uint64_t lhs, uint64_t rhs) {
    if (lhs > UINT64_MAX - rhs)
        return UINT64_MAX;

    return lhs + rhs;
}


ASDF_LOCAL size_t asdf_util_get_total_memory(void);


/**
 * Concatenate two NULL-terminated arrays returning a new array.
 *
 * Elements from ``src`` are appended to ``dst`` and the new address
 * of the destination array is returned (via ``realloc``).  ``dst``
 * may be NULL, in which case a fresh array containing a copy of
 * ``src`` is allocated.  Returns NULL on OOM.
 */
ASDF_LOCAL void **asdf_array_concat(void **dst, const void **src);


// Portable-enough way to get the maximum off_t on the system
// Very weird that POSIX does not just define this
#define ASDF_OFF_MAX (size_t)(((uintmax_t)1 << (sizeof(off_t) * CHAR_BIT - 1)) - 1)
