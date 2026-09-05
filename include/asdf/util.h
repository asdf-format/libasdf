/**
 * .. _asdf/util.h:
 *
 * Common macros and utilities included by every other public header
 */

//

#ifndef ASDF_UTIL_H
#define ASDF_UTIL_H

#include <asdf/config.h> // IWYU pragma: export

#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 4)
#define ASDF_EXPORT __attribute__((visibility("default")))
#define ASDF_LOCAL __attribute__((visibility("hidden")))
#else
#define ASDF_EXPORT
#define ASDF_LOCAL
#endif


#ifdef __cplusplus
#define ASDF_BEGIN_DECLS extern "C" {
#define ASDF_END_DECLS }
#define ASDF_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
#define ASDF_BEGIN_DECLS
#define ASDF_END_DECLS
#define ASDF_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif


/* AFAIK this should be supported on virtually any target/compiler */
#define ASDF_CONSTRUCTOR __attribute__((constructor))
#define ASDF_DESTRUCTOR __attribute__((destructor))


ASDF_BEGIN_DECLS

/**
 * Free a buffer that libasdf allocated on the caller's behalf
 *
 * Use this for buffers returned by `asdf_write_to_mem`,
 * `asdf_ndarray_read_all`, `asdf_ndarray_read_tile_ndim` and
 * `asdf_ndarray_read_tile_2d` when they were asked to allocate the
 * destination buffer.  Other pointers returned by libasdf have their own
 * destructors and must not be passed here.
 *
 * :param ptr: The buffer to free; passing `NULL` is a no-op
 */
ASDF_EXPORT void asdf_free(void *ptr);

ASDF_END_DECLS


#endif /* ASDF_UTIL_H */
