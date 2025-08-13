#ifndef ASDF_UTIL_H
#define ASDF_UTIL_H

#if defined(__clang__) || (defined(__GNUC__) && __GNUC__ >= 4)
    #define ASDF_EXPORT __attribute__ ((visibility ("default")))
    #define ASDF_LOCAL __attribute__ ((visibility ("hidden")))
#else
    #define ASDF_EXPORT
    #define ASDF_LOCAL
#endif


#ifdef __cplusplus
    #define ASDF_BEGIN_DECLS extern "C" {
    #define ASDF_END_DECLS   }
    #define ASDF_STATIC_ASSERT(cond, msg) static_assert(cond, msg)
#else
    #define ASDF_BEGIN_DECLS
    #define ASDF_END_DECLS
    #define ASDF_STATIC_ASSERT(cond, msg) _Static_assert(cond, msg)
#endif


/* AFAIK this should be supported on virtually any target/compiler */
#define ASDF_CONSTRUCTOR __attribute__((constructor))
#define ASDF_DESTRUCTOR __attribute__((destructor))


#endif  /* ASDF_UTIL_H */
