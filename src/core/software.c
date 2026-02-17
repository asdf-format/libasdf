#include <stdlib.h>
#include <string.h>

#include "../error.h"
#include "../file.h"
#include "../log.h"
#include "../util.h"
#include "../value.h"


#define ASDF_CORE_SOFTWARE_TAG ASDF_CORE_TAG_PREFIX "software-1.0.0"


static asdf_value_t *asdf_software_serialize(
    asdf_file_t *file,
    // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
    const void *obj,
    UNUSED(const void *userdata)) {
    if (UNLIKELY(!file || !obj))
        return NULL;

    const asdf_software_t *software = obj;
    asdf_mapping_t *software_map = NULL;
    asdf_value_t *value = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_EMIT_FAILURE;

    if (!software->name) {
        ASDF_LOG(file, ASDF_LOG_WARN, ASDF_CORE_SOFTWARE_TAG " requires a name");
        goto cleanup;
    }

    if (!software->version) {
        ASDF_LOG(file, ASDF_LOG_WARN, ASDF_CORE_SOFTWARE_TAG " requires a version");
        goto cleanup;
    }


    software_map = asdf_mapping_create(file);

    if (!software_map)
        return NULL;

    err = asdf_mapping_set_string0(software_map, "name", software->name);

    if (err != ASDF_VALUE_OK)
        goto cleanup;

    err = asdf_mapping_set_string0(software_map, "version", software->version);

    if (err != ASDF_VALUE_OK)
        goto cleanup;

    if (software->author && strlen(software->author) > 0) {
        err = asdf_mapping_set_string0(software_map, "author", software->author);

        if (err != ASDF_VALUE_OK)
            goto cleanup;
    }

    if (software->homepage && strlen(software->homepage) > 0) {
        err = asdf_mapping_set_string0(software_map, "homepage", software->homepage);

        if (err != ASDF_VALUE_OK)
            goto cleanup;
    }

    value = asdf_value_of_mapping(software_map);
cleanup:
    if (err != ASDF_VALUE_OK)
        asdf_mapping_destroy(software_map);

    return value;
}


static asdf_value_err_t asdf_software_deserialize(
    asdf_value_t *value, UNUSED(const void *userdata), void **out) {
    const char *name = NULL;
    const char *version = NULL;
    const char *homepage = NULL;
    const char *author = NULL;
    asdf_mapping_t *software_map = NULL;
    asdf_value_t *prop = NULL;
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;

    if (asdf_value_as_mapping(value, &software_map) != ASDF_VALUE_OK)
        goto failure;

    /* name and version are required; if missing or the wrong type return parse failure */
    if (!(prop = asdf_mapping_get(software_map, "name")))
        goto failure;

    if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &name))
        goto failure;

    asdf_value_destroy(prop);

    if (!(prop = asdf_mapping_get(software_map, "version")))
        goto failure;

    if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &version))
        goto failure;

    asdf_value_destroy(prop);

    prop = asdf_mapping_get(software_map, "homepage");
    if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &homepage))
        ASDF_LOG(
            value->file, ASDF_LOG_WARN, "ignoring invalid value for for homepage in software tag");

    asdf_value_destroy(prop);

    prop = asdf_mapping_get(software_map, "author");
    if (ASDF_VALUE_OK != asdf_value_as_string0(prop, &author))
        ASDF_LOG(
            value->file, ASDF_LOG_WARN, "ignoring invalid value for for author in software tag");

    asdf_value_destroy(prop);

    asdf_software_t *software = calloc(1, sizeof(asdf_software_t));

    if (!software) {
        err = ASDF_VALUE_ERR_OOM;
        goto failure;
    }

    software->name = name ? strdup(name) : name;
    software->version = version ? strdup(version) : version;
    software->homepage = homepage ? strdup(homepage) : homepage;
    software->author = author ? strdup(author) : author;
    *out = software;
    return ASDF_VALUE_OK;
failure:
    asdf_value_destroy(prop);
    return err;
}


static void asdf_software_dealloc(void *value) {
    if (!value)
        return;

    asdf_software_t *software = value;
    free((void *)software->name);
    free((void *)software->version);
    free((void *)software->homepage);
    free((void *)software->author);
    free(value);
}


static void *asdf_software_copy(const void *value) {
    if (!value)
        return NULL;

    const asdf_software_t *software = value;
    asdf_software_t *copy = calloc(1, sizeof(asdf_software_t));

    if (!copy)
        goto failure;

    if (software->name) {
        copy->name = strdup(software->name);

        if (!copy->name)
            goto failure;
    }

    if (software->version) {
        copy->version = strdup(software->version);

        if (!copy->version)
            goto failure;
    }

    if (software->homepage) {
        copy->homepage = strdup(software->homepage);

        if (!copy->homepage)
            goto failure;
    }

    if (software->author) {
        copy->author = strdup(software->author);

        if (!copy->author)
            goto failure;
    }

    return copy;
failure:
    asdf_software_dealloc(copy);
    ASDF_ERROR_OOM(NULL);
    return NULL;
}


ASDF_REGISTER_EXTENSION(
    software,
    ASDF_CORE_SOFTWARE_TAG,
    asdf_software_t,
    &libasdf_software,
    asdf_software_serialize,
    asdf_software_deserialize,
    asdf_software_copy,
    asdf_software_dealloc,
    NULL);


/** Additional software-related methods */
void asdf_library_set(asdf_file_t *file, const asdf_software_t *software) {
    file->asdf_library = asdf_software_clone(software);
}


void asdf_library_set_version(asdf_file_t *file, const char *version) {
    asdf_software_t *software = asdf_software_clone(&libasdf_software);

    if (!software) {
        ASDF_ERROR_OOM(file);
        return;
    }

    if (software->version)
        free((void *)software->version);

    software->version = version ? strdup(version) : strdup("");
    file->asdf_library = software;
}
