#include <ctype.h>
#include <stdbool.h>
#include <string.h>

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


#define SKIP_WHITESPACE(p) \
    do { \
        while (*(p) && isspace(*(p))) \
            (p)++; \
    } while (0)


/**
 * Path parser
 *
 * This is inspired by the code in libfyaml for parsing its YAML Pointer paths.
 *
 * The main difference here is the libfyaml code actually walks through real
 * nodes in the document while parsing the path, whereas here we
 */
bool asdf_yaml_path_parse(const char *path, asdf_yaml_path_t *out_path) {
    if (!out_path)
        return false;

    const char *p = path;
    size_t len = 0;

    if (p) {
        // Skip any leading / or whitespace
        while (*p && (*p == '/' || isspace(*p)))
            p++;

        len = strlen(p);
    }

    if (!p || len == 0) {
        // Special case--if the path is null or an empty string, it always
        // refers to the root.  We represent that with parent = NULL, and
        // key = ""
        asdf_yaml_path_push(
            out_path,
            (asdf_yaml_path_component_t){.target = ASDF_YAML_PC_TARGET_MAP, .key = strdup("")});
        return true;
    }

    // First count the number of '/' in the path to get an upper bound on the
    // number of components in the path.  There may be more '/' than there are
    // components if some mapping keys contain an embedded '/' but this is rare
    isize n_comp = 1;
    for (const char *q = p; *q; q++)
        n_comp += (*q == '/');

    if (!asdf_yaml_path_reserve(out_path, n_comp))
        return false;

    const char *end = p + len;
    const char *cur = NULL;
    const char *cur_end = NULL;
    char brac = -1;
    asdf_yaml_pc_target_t target = ASDF_YAML_PC_TARGET_ANY;

    while (p != end) {
        cur_end = NULL;
        brac = *p;

        // Here we check if the path component begins with one of the
        // recognized opening brackets: [, ", or '
        // If [ then it *must* be a sequence index, and if a quote it must
        // be a mapping key.  Otherwise we determine later
        switch (brac) {
        case '[':
            // target had better be a sequence index
            target = ASDF_YAML_PC_TARGET_SEQ;
            p++;
            break;
        case '\'':
        case '\"':
            // target had better be a mapping key
            target = ASDF_YAML_PC_TARGET_MAP;
            p++;
            break;
        default:
            target = ASDF_YAML_PC_TARGET_ANY;
            brac = -1;
            break;
        }

        switch (target) {
        case ASDF_YAML_PC_TARGET_ANY: {
            // In the unknown/ambiguous case, just scan until the next /
            // A '/' is not allowed in a key unless it's in a quoted key
            SKIP_WHITESPACE(p);
            cur = p;
            while (p < end && *p != '/') {
                // Invalid bracket encountered in a non-bracketed path
                // component
                if (strchr("'\"[]", *p))
                    goto invalid;

                p++;
            }
            cur_end = p;
            break;
        }
        case ASDF_YAML_PC_TARGET_MAP:
            // Definitely a mapping key because we are inside quotes
            // scan until the matching quote and also check for valid escape
            // sequences
            if (p == end)
                goto invalid;

            cur = p;
            while (p < end) {
                if (*p == '\\') {
                    // Encountered an escape sequence; next character must be
                    // one of the escaped characters
                    if (!strchr("/*&.{}[]\\", *(++p)))
                        goto invalid;

                    continue;
                }

                if (*p == brac && (*(p + 1) == '\0' || *(p + 1) == '/')) {
                    cur_end = p;
                    p++;
                    break;
                }

                p++;
            }

            // We never reached the end quote; invalid path
            if (!cur_end)
                goto invalid;

            break;
        case ASDF_YAML_PC_TARGET_SEQ:
            // Must be a sequence index (opened with [)
            // We allow leading and trailing whitespace inside the brackets
            // like [ 0 ]; this seems to be consistent with libfyaml
            SKIP_WHITESPACE(p);

            if (p == end)
                goto invalid;

            cur = p;
            // Leading sign, OK
            if (*p == '-')
                p++;

            while (p < end) {
                if (!isdigit(*p)) {
                    // must be followed by either whitespace or the closing
                    // bracket
                    SKIP_WHITESPACE(p);
                    if (*p != ']') {
                        goto invalid;
                    } else {
                        cur_end = p;
                        p++;
                        SKIP_WHITESPACE(p);
                        break;
                    }
                } else {
                    p++;
                }
            }

            if (!cur_end)
                goto invalid;

            break;
        }

        ssize_t index = 0;
        char *key = strndup(cur, (cur_end - cur));

        if (!key)
            goto invalid;

        // Determine if the key was a pure integer
        // If the target is ANY (ambiguous) we store both the string key and the
        // integer value on the path component.  Otherwise we determine here
        // that if the path component was not an integer, in which case it should
        // be treated as a mapping key
        if (target == ASDF_YAML_PC_TARGET_ANY || target == ASDF_YAML_PC_TARGET_SEQ) {
            char *end_idx = NULL;
            index = strtoll(key, &end_idx, 10);

            // If it was not an integer it's invalid if we were in an explicit
            // sequence (in [] brackets); else it can still be a mapping key
            if (end_idx != NULL && *end_idx) {
                if (target == ASDF_YAML_PC_TARGET_SEQ)
                    goto invalid;
                else
                    target = ASDF_YAML_PC_TARGET_MAP;
            }
        }

        // For sequence components we don't store the string key at all
        if (target == ASDF_YAML_PC_TARGET_SEQ) {
            free(key);
            key = NULL;
        }

        asdf_yaml_path_push(
            out_path, (asdf_yaml_path_component_t){.target = target, .key = key, .index = index});

        // Advance past the trailing / if any
        if (p != end)
            p++;
    }

    return true;
invalid:
    asdf_yaml_path_clear(out_path);
    return false;
}
