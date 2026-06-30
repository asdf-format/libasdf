/* Declare and register ASDF extensions */
#ifndef ASDF_EXTENSION_H
#define ASDF_EXTENSION_H

#include <stdbool.h>

#include <asdf/file.h>
#include <asdf/util.h>
#include <asdf/value.h>
#include <asdf/version.h>


ASDF_BEGIN_DECLS

typedef struct {
    const char *name;
    const asdf_version_t *version;
} asdf_tag_t;


/**
 * Metadata describing a piece of software involved in producing an ASDF file
 *
 * This corresponds to the ``core/software`` schema and is used to record any
 * software that produced or contributed to a file -- including libasdf itself,
 * which records its own `asdf_software_t` in the file's ``asdf_library``
 * metadata.  It is most relevant to extension authors, who pass a pointer to
 * one of these to `ASDF_REGISTER_EXTENSION` to document the software that
 * implements their extension.  See :ref:`extensions`.
 */
typedef struct {
    /** Name of the software */
    const char *name;
    /** Version of the software; see ``asdf_version_t`` */
    const asdf_version_t *version;
    /** Optional author of the software */
    const char *author;
    /** Optional homepage URL for the software */
    const char *homepage;
} asdf_software_t;


typedef asdf_value_t *(*asdf_extension_serialize_t)(
    asdf_file_t *file, const void *obj, const void *userdata);


typedef asdf_value_err_t (*asdf_extension_deserialize_t)(
    asdf_value_t *value, const void *userdata, void **out);


typedef void *(*asdf_extension_copy_t)(const void *obj);


typedef void (*asdf_extension_dealloc_t)(void *obj);


struct _asdf_extension {
    const char *tag;
    asdf_software_t *software;
    asdf_extension_serialize_t serialize;
    asdf_extension_deserialize_t deserialize;
    asdf_extension_copy_t copy;
    asdf_extension_dealloc_t dealloc;
    void *userdata;
};


/**
 * Struct representing a registered libasdf extension
 */
typedef struct _asdf_extension asdf_extension_t;


/**
 * Register an extension with the library
 *
 * This is normally called automatically by the constructor that
 * `ASDF_REGISTER_EXTENSION` generates, and rarely needs to be called directly.
 *
 * :param ext: The `asdf_extension_t *` to register
 */
ASDF_EXPORT void asdf_extension_register(asdf_extension_t *ext);

/**
 * Look up a registered extension by its tag
 *
 * :param file: The `asdf_file_t *` for the file
 * :param tag: The tag string the extension was registered under
 * :return: The matching `asdf_extension_t *`, or ``NULL`` if no extension is
 *   registered for ``tag``
 */
ASDF_EXPORT const asdf_extension_t *asdf_extension_get(asdf_file_t *file, const char *tag);

/**
 * Parse a tag string of the form ``"name"`` or ``"name-version"`` into an
 * ``asdf_tag_t``
 *
 * :param tag: The tag string to parse
 * :return: A newly allocated ``asdf_tag_t *`` owned by the caller (free it with
 *   `asdf_tag_destroy`), or ``NULL`` on failure
 */
ASDF_EXPORT asdf_tag_t *asdf_tag_parse(const char *tag);

/**
 * Free a tag returned by `asdf_tag_parse`
 *
 * :param tag: The ``asdf_tag_t *`` to free
 */
ASDF_EXPORT void asdf_tag_destroy(asdf_tag_t *tag);


#define ASDF_EXT_PREFIX asdf

/* Macro helpers */
#define ASDF_PASTE(a, b) a##b
#define ASDF_EXPAND(a, b) ASDF_PASTE(a, b)


#define ASDF_EXT_STATIC_NAME(extname) ASDF_EXPAND(ASDF_EXT_PREFIX, _##extname##_extension)


#define ASDF_EXT_DEFINE( \
    extname, _tag, _software, _serialize, _deserialize, _copy, _dealloc, _userdata) \
    static asdf_extension_t ASDF_EXT_STATIC_NAME(extname) = { \
        .tag = (_tag), \
        .software = (_software), \
        .serialize = (_serialize), \
        .deserialize = (_deserialize), \
        .copy = (_copy), \
        .dealloc = (_dealloc), \
        .userdata = (_userdata)}


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


#define ASDF_EXT_DEFINE_VALUE_OF_TYPE(extname, type) \
    ASDF_EXPORT asdf_value_t *asdf_value_of_##extname(asdf_file_t *file, const type *obj) { \
        return asdf_value_of_extension_type(file, obj, &ASDF_EXT_STATIC_NAME(extname)); \
    }


#define ASDF_EXT_DEFINE_GET(extname, type) \
    ASDF_EXPORT asdf_value_err_t asdf_get_##extname( \
        asdf_file_t *file, const char *path, type **out) { \
        return asdf_get_extension_type(file, path, &ASDF_EXT_STATIC_NAME(extname), (void **)out); \
    }


#define ASDF_EXT_DEFINE_SET(extname, type) \
    ASDF_EXPORT asdf_value_err_t asdf_set_##extname( \
        asdf_file_t *file, const char *path, const type *obj) { \
        return asdf_set_extension_type( \
            file, path, (const void *)obj, &ASDF_EXT_STATIC_NAME(extname)); \
    }


/*
 * Auto-generated helper to clone extension types
 *
 * Extension types may optionally not implement the copy method, in which case a shallow
 * copy is performed.  This may result in undesired effects (double-frees, etc.) so do make
 * sure to implement this if the extension object contains nested data structures.
 */
#define ASDF_EXT_DEFINE_CLONE(extname, type) \
    ASDF_EXPORT type *asdf_##extname##_clone(const type *object) { \
        if (!object) \
            return NULL; \
        asdf_extension_t *ext = &ASDF_EXT_STATIC_NAME(extname); \
        if (!ext) \
            return NULL; \
        if (!ext->copy) { \
            void *clone = malloc(sizeof(type)); \
            if (!clone) \
                return NULL; \
            memcpy(clone, object, sizeof(type)); \
            return clone; \
        } \
        return (type *)ext->copy(object); \
    }


/*
 * Helper to clone a NULL-terminated array of pointers to an extension object
 *
 * For example, clones an `asdf_history_entry_t **` array.
 */
#define ASDF_EXT_DEFINE_ARRAY_CLONE(extname, type) \
    ASDF_EXPORT type **asdf_##extname##_array_clone(const type **src) { \
        size_t nelem = 0; \
        while (src[nelem]) \
            nelem++; \
        type **dst = (type **)malloc((nelem + 1) * sizeof(type *)); \
        if (!dst) \
            return NULL; \
        for (size_t idx = 0; idx < nelem; idx++) { \
            dst[idx] = asdf_##extname##_clone(src[idx]); \
            if (!dst[idx]) { \
                for (size_t jdx = 0; jdx < idx; jdx++) \
                    asdf_##extname##_destroy(dst[jdx]); \
                free((void *)dst); \
                return NULL; \
            } \
        } \
        dst[nelem] = NULL; \
        return dst; \
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


/**
 * Define and register an extension type
 *
 * Generates the full family of public accessors for an extension type and a
 * constructor that registers the extension with libasdf automatically at load
 * time.  Place it in a single ``.c`` file; use `ASDF_DECLARE_EXTENSION` in a
 * header to expose the generated functions to other translation units.  See
 * :ref:`extensions` for a complete walkthrough.
 *
 * The generated functions are named after ``extname``: ``asdf_get_<extname>``,
 * ``asdf_set_<extname>``, ``asdf_is_<extname>``, ``asdf_value_is_<extname>``,
 * ``asdf_value_as_<extname>``, ``asdf_value_of_<extname>``,
 * ``asdf_<extname>_clone``, ``asdf_<extname>_array_clone``, and
 * ``asdf_<extname>_destroy``.
 *
 * :param extname: Base name for the generated functions (need not match the
 *   C ``type``)
 * :param tag: The YAML tag string the extension is registered for
 * :param type: The C type the extension deserializes to (e.g. ``asdf_foo_t``)
 * :param software: Pointer to an `asdf_software_t` describing the software that
 *   implements the extension
 * :param serialize: A ``asdf_extension_serialize_t`` callback, or ``NULL``
 * :param deserialize: A ``asdf_extension_deserialize_t`` callback
 * :param copy: A ``asdf_extension_copy_t`` deep-copy callback, or ``NULL`` for
 *   a shallow copy
 * :param dealloc: A ``asdf_extension_dealloc_t`` callback to free the type
 * :param userdata: Optional ``void *`` passed through to the callbacks, or
 *   ``NULL``
 */
#define ASDF_REGISTER_EXTENSION( \
    extname, tag, type, software, serialize, deserialize, copy, dealloc, userdata) \
    ASDF_EXT_DEFINE(extname, tag, software, serialize, deserialize, copy, dealloc, userdata); \
    ASDF_EXT_DEFINE_VALUE_AS_TYPE(extname, type) \
    ASDF_EXT_DEFINE_VALUE_IS_TYPE(extname) \
    ASDF_EXT_DEFINE_VALUE_OF_TYPE(extname, type) \
    ASDF_EXT_DEFINE_IS_TYPE(extname, type) \
    ASDF_EXT_DEFINE_GET(extname, type) \
    ASDF_EXT_DEFINE_SET(extname, type) \
    ASDF_EXT_DEFINE_DESTROY(extname, type) \
    ASDF_EXT_DEFINE_CLONE(extname, type) \
    ASDF_EXT_DEFINE_ARRAY_CLONE(extname, type) \
    static ASDF_CONSTRUCTOR void ASDF_EXPAND( \
        ASDF_EXT_PREFIX, _register_##extname##_extension)(void) { \
        asdf_extension_register(&ASDF_EXT_STATIC_NAME(extname)); \
    }


/**
 * Declare the public API generated by `ASDF_REGISTER_EXTENSION`
 *
 * Place this in a header to expose an extension's generated functions (the
 * ``asdf_get_<extname>``/``asdf_set_<extname>``/etc. family listed under
 * `ASDF_REGISTER_EXTENSION`) to other translation units.  ``extname`` and
 * ``type`` must match those passed to `ASDF_REGISTER_EXTENSION`.
 *
 * :param extname: Base name used for the generated functions
 * :param type: The C type the extension deserializes to
 */
#define ASDF_DECLARE_EXTENSION(extname, type) \
    ASDF_EXPORT asdf_value_err_t asdf_value_as_##extname(asdf_value_t *value, type **out); \
    ASDF_EXPORT bool asdf_value_is_##extname(asdf_value_t *value); \
    ASDF_EXPORT asdf_value_t *asdf_value_of_##extname(asdf_file_t *file, const type *obj); \
    ASDF_EXPORT bool asdf_is_##extname(asdf_file_t *file, const char *path); \
    ASDF_EXPORT asdf_value_err_t asdf_get_##extname( \
        asdf_file_t *file, const char *path, type **out); \
    ASDF_EXPORT asdf_value_err_t asdf_set_##extname( \
        asdf_file_t *file, const char *path, const type *obj); \
    ASDF_EXPORT type *asdf_##extname##_clone(const type *object); \
    ASDF_EXPORT type **asdf_##extname##_array_clone(const type **src); \
    ASDF_EXPORT void asdf_##extname##_destroy(type *object)

ASDF_END_DECLS

#endif /* ASDF_EXTENSION_H */
