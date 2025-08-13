/* Declare and register ASDF extensions */
#ifndef ASDF_EXTENSION_H
#define ASDF_EXTENSION_H

#include <stdbool.h>

#include <asdf/file.h>
#include <asdf/util.h>
#include <asdf/value.h>


ASDF_BEGIN_DECLS

typedef struct {
    const char *name;
    const char *author;
    const char *homepage;
    const char *version;
} asdf_software_t;


typedef asdf_value_err_t (*asdf_extension_deserialize_t)(asdf_value_t *value, const void *userdata, void **out);
typedef void (*asdf_extension_dealloc_t)(void *userdata);


typedef struct asdf_extension {
    const char *tag;
    asdf_software_t *software;
    asdf_extension_deserialize_t deserialize;
    asdf_extension_dealloc_t dealloc;
    void *userdata;
} asdf_extension_t;


ASDF_EXPORT void asdf_extension_register(asdf_extension_t *ext);
ASDF_EXPORT const asdf_extension_t *asdf_extension_get(asdf_file_t *file, const char *tag);


#define ASDF_EXT_PREFIX asdf

/* Macro helpers */
#define _ASDF_PASTE(a, b) a##b
#define _ASDF_EXPAND(a, b) _ASDF_PASTE(a, b)


#define ASDF_EXT_STATIC_NAME(extname) _ASDF_EXPAND(ASDF_EXT_PREFIX, _##extname##_extension)


#define ASDF_EXT_DEFINE(extname, _tag, _software, _deserialize, _dealloc, _userdata) \
    static asdf_extension_t ASDF_EXT_STATIC_NAME(extname) = { \
        .tag = _tag, \
        .software = _software, \
        .deserialize = _deserialize, \
        .dealloc = _dealloc, \
        .userdata = _userdata \
    }


#define ASDF_EXT_DEFINE_VALUE_IS_TYPE(extname) \
    ASDF_EXPORT bool asdf_value_is_##extname(asdf_value_t *value) { \
        return asdf_value_is_extension_type(value, &ASDF_EXT_STATIC_NAME(extname)); \
    }


#define ASDF_EXT_DEFINE_VALUE_AS_TYPE(extname, type) \
    ASDF_EXPORT asdf_value_err_t asdf_value_as_##extname(asdf_value_t *value, type **out) { \
        return asdf_value_as_extension_type(value, &ASDF_EXT_STATIC_NAME(extname), (void **)out); \
    }


#define ASDF_EXT_DEFINE_IS_TYPE(extname, type) \
    ASDF_EXPORT bool asdf_is_##extname(asdf_file_t *file, const char *path) { \
        return asdf_is_extension_type(file, path, &ASDF_EXT_STATIC_NAME(extname)); \
    }


#define ASDF_EXT_DEFINE_GET(extname, type) \
    ASDF_EXPORT asdf_value_err_t asdf_get_##extname(asdf_file_t *file, const char *path, type **out) { \
        return asdf_get_extension_type(file, path, &ASDF_EXT_STATIC_NAME(extname), (void **)out); \
    }


/* Auto-generated helper to free extension type objects */
#define ASDF_EXT_DEFINE_DESTROY(extname, type) \
    ASDF_EXPORT void asdf_##extname##_destroy(type *object) { \
        if (!object) \
            return; \
        asdf_extension_t *ext = &ASDF_EXT_STATIC_NAME(extname); \
        if (!ext && ext->dealloc) \
            return; \
        ext->dealloc(object); \
    }


#define ASDF_REGISTER_EXTENSION(extname, tag, type, software, deserialize, dealloc, userdata) \
    ASDF_EXT_DEFINE(extname, tag, software, deserialize, dealloc, userdata); \
    ASDF_EXT_DEFINE_VALUE_IS_TYPE(extname) \
    ASDF_EXT_DEFINE_VALUE_AS_TYPE(extname, type) \
    ASDF_EXT_DEFINE_IS_TYPE(extname, type) \
    ASDF_EXT_DEFINE_GET(extname, type) \
    ASDF_EXT_DEFINE_DESTROY(extname, type) \
    static ASDF_CONSTRUCTOR void _ASDF_EXPAND(ASDF_EXT_PREFIX, _register_##extname##_extension)(void) { \
        asdf_extension_register(&ASDF_EXT_STATIC_NAME(extname)); \
    }


/* Provides declarations for auto-generated extension type APIs, for use in headers */
#define ASDF_DECLARE_EXTENSION(extname, type) \
    ASDF_EXPORT bool asdf_value_is_##extname(asdf_value_t *value); \
    ASDF_EXPORT asdf_value_err_t asdf_value_as_##extname(asdf_value_t *value, type **out); \
    ASDF_EXPORT bool asdf_is_##extname(asdf_file_t *file, const char *path); \
    ASDF_EXPORT asdf_value_err_t asdf_get_##extname(asdf_file_t *file, const char *path, type **out); \
    ASDF_EXPORT void asdf_##extname##_destroy(type *object)

ASDF_END_DECLS

#endif /* ASDF_EXTENSION_H */
