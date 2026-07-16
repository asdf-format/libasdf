/* Declare and register ASDF extensions */
#ifndef ASDF_EXTENSION_H
#define ASDF_EXTENSION_H

#include <assert.h>
#include <stdbool.h>

#include <asdf/error.h>
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


/**
 * Serialize a native object into an `asdf_value_t`
 *
 * :param file: The `asdf_file_t *` the value is created for
 * :param obj: The native object to serialize
 * :param userdata: The extension's ``userdata``
 * :return: The new `asdf_value_t *`, or ``NULL`` on failure
 */
typedef asdf_value_t *(*asdf_extension_serialize_t)(
    asdf_file_t *file, const void *obj, const void *userdata);


/**
 * Deserialize an `asdf_value_t` into a native object
 *
 * :param value: The raw `asdf_value_t *` read from the file
 * :param userdata: The extension's ``userdata``
 * :param out: Set to the newly allocated native object on success
 * :return: ``ASDF_VALUE_OK`` on success, otherwise an error code
 */
typedef asdf_value_err_t (*asdf_extension_deserialize_t)(
    asdf_value_t *value, const void *userdata, void **out);


/**
 * Deep-copy a native object into caller-provided storage
 *
 * The generated ``asdf_<ext>_copy``/``asdf_<ext>_copy_into`` wrappers zero
 * ``dst`` before this is called, and on failure they call the extension's
 * ``deinit`` method on ``dst`` to unwind any partial work.  An implementation
 * therefore only needs to populate ``dst`` and, on failure, return ``false``
 * (the sole assumed failure mode being out-of-memory).
 *
 * :param file: A handle to the file to which the object belongs
 * :param src: The native object to copy
 * :param dst: Pre-zeroed storage to copy ``src`` into
 * :return: ``true`` on success, ``false`` on failure
 */
typedef bool (*asdf_extension_copy_t)(asdf_file_t *file, const void *src, void *dst);


/**
 * De-initialize a native object produced by an `asdf_extension_deserialize_t`
 *
 * This frees any resources owned by ``obj``'s fields but *must not* free ``obj``
 * itself--its storage may be embedded, an array element, or static.  Freeing
 * the object's own storage is done by the generated ``asdf_<ext>_destroy``.
 * It must be safe to call on a zero-initialized object and on a
 * partially-initialized object left behind by a failed `asdf_extension_copy_t`.
 *
 * :param obj: The native object whose fields to de-initialize
 */
typedef void (*asdf_extension_deinit_t)(void *obj);


/**
 * Generic method-pointer type for reserved `asdf_extension_vtab_t` slots
 */
typedef void (*asdf_extension_method_t)(void);


/** Total number of method slots in `asdf_extension_vtab_t` (used + reserved) */
#define ASDF_EXTENSION_VTAB_MAX_METHODS 8

/**
 * Number of currently-defined `asdf_extension_vtab_t` methods
 *
 * Bump this when adding new methods to the vtab.
 */
#define ASDF_EXTENSION_VTAB_METHODS 4


/**
 * Table of the methods implementing an extension's behavior
 *
 * Extension authors define one of these statically and pass a pointer to it to
 * `ASDF_REGISTER_EXTENSION`.
 */
typedef struct {
    /** Serializer for the extension type, or ``NULL`` if it cannot be written */
    asdf_extension_serialize_t serialize;
    /** Deserializer for the extension type */
    asdf_extension_deserialize_t deserialize;
    /** Deep-copy method, or ``NULL`` for a shallow copy */
    asdf_extension_copy_t copy;
    /** Method to de-initialize objects produced by ``deserialize`` */
    asdf_extension_deinit_t deinit;
    /** Reserved for future methods; keeps the ABI stable as methods are added */
    asdf_extension_method_t
        _reserved[ASDF_EXTENSION_VTAB_MAX_METHODS - ASDF_EXTENSION_VTAB_METHODS];
} asdf_extension_vtab_t;


static_assert(
    sizeof(asdf_extension_vtab_t) ==
        ASDF_EXTENSION_VTAB_MAX_METHODS * sizeof(asdf_extension_method_t),
    "asdf_extension_vtab_t must stay ASDF_EXTENSION_VTAB_MAX_METHODS methods wide");


struct asdf_extension {
    /**
     * ``NULL``-terminated array of full YAML tags this extension handles
     *
     * ``tags[0]`` is written when serializing a newly created or replaced
     * object of this type; any of the listed tags is recognized when reading.
     */
    const char *const *tags;
    asdf_software_t *software;
    const asdf_extension_vtab_t *vtab;
    /**
     * Size of the extension's objects
     *
     * Useful for dynamic allocations in situations where the extension
     * object's type is not known.
     */
    size_t size;
    void *userdata;
};


/**
 * Struct representing a registered libasdf extension
 */
typedef struct asdf_extension asdf_extension_t;


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


#define ASDF_EXT_TAGS_NAME(extname) ASDF_EXPAND(ASDF_EXT_PREFIX, _##extname##_extension_tags)


#define ASDF_EXT_DEFINE(extname, type, _software, _vtab, _userdata, ...) \
    static const char *const ASDF_EXT_TAGS_NAME(extname)[] = {__VA_ARGS__, NULL}; \
    static asdf_extension_t ASDF_EXT_STATIC_NAME(extname) = { \
        .tags = ASDF_EXT_TAGS_NAME(extname), \
        .software = (_software), \
        .vtab = (_vtab), \
        .size = sizeof(type), \
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
 * Auto-generated helper to de-initialize an extension type object in place
 *
 * Frees resources owned by the object's fields but does not free the object's
 * own storage; use for embedded, array-element, or static objects.
 */
#define ASDF_EXT_DEFINE_DEINIT(extname, type) \
    ASDF_EXPORT void asdf_##extname##_deinit(type *object) { \
        if (!object) \
            return; \
        asdf_extension_t *ext = &ASDF_EXT_STATIC_NAME(extname); \
        if (ext->vtab && ext->vtab->deinit) \
            ext->vtab->deinit(object); \
    }


/* Auto-generated helper to de-initialize and free an extension type object */
#define ASDF_EXT_DEFINE_DESTROY(extname, type) \
    ASDF_EXPORT void asdf_##extname##_destroy(type *object) { \
        if (!object) \
            return; \
        asdf_##extname##_deinit(object); \
        free(object); \
    }


/*
 * Auto-generated helper to copy an extension type into caller-provided storage
 *
 * ``dst`` is zeroed before the extension's copy method runs; on failure the
 * partial ``dst`` is de-initialized via ``asdf_<ext>_deinit``.  Extension types
 * may optionally not implement the copy method, in which case a shallow copy is
 * performed.  This may result in undesired effects (double-frees, etc.) so do
 * make sure to implement it if the extension object contains nested data.
 */
#define ASDF_EXT_DEFINE_COPY_INTO(extname, type) \
    ASDF_EXPORT bool asdf_##extname##_copy_into(asdf_file_t *file, const type *src, type *dst) { \
        if (!src || !dst) \
            return false; \
        asdf_extension_t *ext = &ASDF_EXT_STATIC_NAME(extname); \
        memset(dst, 0, sizeof(type)); \
        if (!ext->vtab || !ext->vtab->copy) { \
            memcpy(dst, src, sizeof(type)); \
            return true; \
        } \
        if (!ext->vtab->copy(file, src, dst)) { \
            asdf_##extname##_deinit(dst); \
            ASDF_ERROR_OOM(file); \
            return false; \
        } \
        return true; \
    }


/*
 * Auto-generated helper to copy an extension type into freshly allocated storage
 */
#define ASDF_EXT_DEFINE_COPY(extname, type) \
    ASDF_EXPORT type *asdf_##extname##_copy(asdf_file_t *file, const type *src) { \
        if (!src) \
            return NULL; \
        type *copy = (type *)calloc(1, sizeof(type)); \
        if (!copy) \
            return NULL; \
        if (!asdf_##extname##_copy_into(file, src, copy)) { \
            free(copy); \
            return NULL; \
        } \
        return copy; \
    }


/*
 * Helper to copy a NULL-terminated array of pointers to an extension object
 *
 * For example, copies an `asdf_history_entry_t **` array.
 */
#define ASDF_EXT_DEFINE_ARRAY_COPY(extname, type) \
    ASDF_EXPORT type **asdf_##extname##_array_copy(asdf_file_t *file, const type **src) { \
        size_t nelem = 0; \
        while (src[nelem]) \
            nelem++; \
        type **dst = (type **)malloc((nelem + 1) * sizeof(type *)); \
        if (!dst) \
            return NULL; \
        for (size_t idx = 0; idx < nelem; idx++) { \
            dst[idx] = asdf_##extname##_copy(file, src[idx]); \
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
 * ``asdf_<extname>_copy``, ``asdf_<extname>_copy_into``,
 * ``asdf_<extname>_array_copy``, ``asdf_<extname>_deinit``, and
 * ``asdf_<extname>_destroy``.
 *
 * One or more YAML tags are passed as trailing arguments; the extension is
 * registered for each of them, so a single extension can handle several
 * versions of the same schema.  At least one tag is required.  When an object
 * of this type is serialized it is written with the *first* tag listed, so put
 * the preferred tag first.  Values read from a file and left unmodified keep
 * the tag they were read with.
 *
 * :param extname: Base name for the generated functions (need not match the
 *   C ``type``)
 * :param type: The C type the extension deserializes to (e.g. ``asdf_foo_t``)
 * :param software: Pointer to an `asdf_software_t` describing the software that
 *   implements the extension
 * :param vtab: Pointer to an `asdf_extension_vtab_t` holding the extension's
 *   callbacks
 * :param userdata: Optional ``void *`` passed through to the callbacks, or
 *   ``NULL``
 * :param ...: One or more YAML tag strings the extension is registered for
 */
#define ASDF_REGISTER_EXTENSION(extname, type, software, vtab, userdata, ...) \
    ASDF_EXT_DEFINE(extname, type, software, vtab, userdata, __VA_ARGS__); \
    ASDF_EXT_DEFINE_VALUE_AS_TYPE(extname, type) \
    ASDF_EXT_DEFINE_VALUE_IS_TYPE(extname) \
    ASDF_EXT_DEFINE_VALUE_OF_TYPE(extname, type) \
    ASDF_EXT_DEFINE_IS_TYPE(extname, type) \
    ASDF_EXT_DEFINE_GET(extname, type) \
    ASDF_EXT_DEFINE_SET(extname, type) \
    ASDF_EXT_DEFINE_DEINIT(extname, type) \
    ASDF_EXT_DEFINE_DESTROY(extname, type) \
    ASDF_EXT_DEFINE_COPY_INTO(extname, type) \
    ASDF_EXT_DEFINE_COPY(extname, type) \
    ASDF_EXT_DEFINE_ARRAY_COPY(extname, type) \
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
    ASDF_EXPORT type *asdf_##extname##_copy(asdf_file_t *file, const type *src); \
    ASDF_EXPORT bool asdf_##extname##_copy_into(asdf_file_t *file, const type *src, type *dst); \
    ASDF_EXPORT type **asdf_##extname##_array_copy(asdf_file_t *file, const type **src); \
    ASDF_EXPORT void asdf_##extname##_deinit(type *object); \
    ASDF_EXPORT void asdf_##extname##_destroy(type *object)

ASDF_END_DECLS

#endif /* ASDF_EXTENSION_H */
