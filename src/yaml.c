#include <libfyaml.h>

#include "event.h"
#include "file.h"
#include "yaml.h"


const char *asdf_yaml_directive_prefix = ASDF_YAML_DIRECTIVE_PREFIX;
const char *asdf_yaml_directive = ASDF_YAML_DIRECTIVE;
const char *asdf_yaml_document_end_marker = ASDF_YAML_DOCUMENT_END_MARKER;
const char *asdf_yaml_empty_document = ASDF_YAML_DIRECTIVE ASDF_YAML_DOCUMENT_BEGIN_MARKER
    ASDF_YAML_DOCUMENT_END_MARKER;


static const asdf_yaml_event_type_t fyet_to_asdf_event[] = {
    [FYET_STREAM_START] = ASDF_YAML_STREAM_START_EVENT,
    [FYET_STREAM_END] = ASDF_YAML_STREAM_END_EVENT,
    [FYET_DOCUMENT_START] = ASDF_YAML_DOCUMENT_START_EVENT,
    [FYET_DOCUMENT_END] = ASDF_YAML_DOCUMENT_END_EVENT,
    [FYET_MAPPING_START] = ASDF_YAML_MAPPING_START_EVENT,
    [FYET_MAPPING_END] = ASDF_YAML_MAPPING_END_EVENT,
    [FYET_SEQUENCE_START] = ASDF_YAML_SEQUENCE_START_EVENT,
    [FYET_SEQUENCE_END] = ASDF_YAML_SEQUENCE_END_EVENT,
    [FYET_SCALAR] = ASDF_YAML_SCALAR_EVENT,
    [FYET_ALIAS] = ASDF_YAML_ALIAS_EVENT,
};


#define ASDF_IS_YAML_EVENT(event) \
    ((event) && (event)->type == ASDF_YAML_EVENT && (event)->payload.yaml)


asdf_yaml_event_type_t asdf_yaml_event_type(const asdf_event_t *event) {
    if (!ASDF_IS_YAML_EVENT(event))
        return ASDF_YAML_NONE_EVENT;

    enum fy_event_type type = event->payload.yaml->type;
    if (type < 0 || type >= (int)(sizeof(fyet_to_asdf_event) / sizeof(fyet_to_asdf_event[0]))) {
        abort();
    }
    return fyet_to_asdf_event[type];
}

/**
 * Return a text representation of a YAML event type
 */
const char *asdf_yaml_event_type_text(const asdf_event_t *event) {
    if (!ASDF_IS_YAML_EVENT(event))
        return "";

    return fy_event_type_get_text(event->payload.yaml->type);
}


/**
 * Return unparsed YAML scalar value associated with an event, if any
 *
 * Returns NULL if the event is not a YAML scalar event
 */
const char *asdf_yaml_event_scalar_value(const asdf_event_t *event, size_t *lenp) {
    if (!ASDF_IS_YAML_EVENT(event))
        return NULL;

    if (event->payload.yaml->type != FYET_SCALAR) {
        *lenp = 0;
        return NULL;
    }

    struct fy_token *token = event->payload.yaml->scalar.value;
    // Is safe to call if there is no token, just returns empty string/0
    return fy_token_get_text(token, lenp);
}


/**
 * Return the YAML tag associated with an event, if any
 *
 * Returns NULL if the event is not a YAML event or if there is no tag
 */
const char *asdf_yaml_event_tag(const asdf_event_t *event, size_t *lenp) {
    if (!ASDF_IS_YAML_EVENT(event))
        return NULL;

    struct fy_token *token = fy_event_get_tag_token(event->payload.yaml);
    // Is safe to call if there is no token, just returns empty string/0
    return fy_token_get_text(token, lenp);
}


// TODO: Could maybe cache the default empty document and use fy_document_clone on it
// but I don't think this is a very expensive operation to begin with.
struct fy_document *asdf_yaml_create_empty_document(asdf_config_t *config) {
    bool has_default_tag_handle = false;
    struct fy_document *doc = fy_document_build_from_string(NULL, asdf_yaml_empty_document, FY_NT);

    if (!doc)
        return NULL;

    if (config && config->emitter.tag_handles) {
        // One loop over to see if we actually have the ! handle defined, if not
        // set it to the default
        asdf_yaml_tag_handle_t *handle = config->emitter.tag_handles;
        while (handle && handle->handle) {
            if (strcmp(handle->handle, ASDF_YAML_DEFAULT_TAG_HANDLE) == 0) {
                has_default_tag_handle = true;
                break;
            }
            handle++;
        }

        if (!has_default_tag_handle) {
            if (fy_document_tag_directive_lookup(doc, ASDF_YAML_DEFAULT_TAG_HANDLE) != NULL) {
                if (fy_document_tag_directive_remove(doc, ASDF_YAML_DEFAULT_TAG_HANDLE) != 0)
                    goto error;
            }

            if (fy_document_tag_directive_add(
                    doc, ASDF_YAML_DEFAULT_TAG_HANDLE, ASDF_STANDARD_TAG_PREFIX) != 0)
                goto error;
        }

        handle = config->emitter.tag_handles;
        while (handle && handle->handle) {
            if (fy_document_tag_directive_lookup(doc, handle->handle) != NULL) {
                if (fy_document_tag_directive_remove(doc, handle->handle) != 0)
                    goto error;
            }
            if (fy_document_tag_directive_add(doc, handle->handle, handle->prefix) != 0)
                goto error;
            handle++;
        }
    }

    return doc;
error:
    fy_document_destroy(doc);
    return NULL;
}
