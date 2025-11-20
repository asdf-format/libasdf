#pragma once

#include <assert.h>
#include <stddef.h>
#include <string.h>

#include <asdf/util.h>


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


/* Cast memset to volatile to prevent opt-away */
#define ZERO_MEMORY(ptr, size) \
    do { \
        void *volatile _volatile_ptr = (ptr); \
        memset(_volatile_ptr, 0, (size)); \
    } while (0)


ASDF_LOCAL size_t asdf_util_get_total_memory(void);
