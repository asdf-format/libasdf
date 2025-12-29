#include <assert.h>
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


static bool asdf_yaml_document_add_tag_handles(
    struct fy_document *doc, const asdf_yaml_tag_handle_t *handles) {
    assert(doc);
    assert(handles);
    const asdf_yaml_tag_handle_t *handle = handles;
    bool has_default_tag_handle = false;

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
                return false;
        }

        if (fy_document_tag_directive_add(
                doc, ASDF_YAML_DEFAULT_TAG_HANDLE, ASDF_STANDARD_TAG_PREFIX) != 0)
            return false;
    }

    handle = handles;
    while (handle && handle->handle) {
        if (fy_document_tag_directive_lookup(doc, handle->handle) != NULL) {
            if (fy_document_tag_directive_remove(doc, handle->handle) != 0)
                return false;
        }
        if (fy_document_tag_directive_add(doc, handle->handle, handle->prefix) != 0)
            return false;
        handle++;
    }

    return true;
}

// TODO: Could maybe cache the default empty document and use fy_document_clone on it
// but I don't think this is a very expensive operation to begin with.
struct fy_document *asdf_yaml_create_empty_document(asdf_config_t *config) {
    struct fy_document *doc = fy_document_build_from_string(NULL, asdf_yaml_empty_document, FY_NT);

    if (!doc)
        return NULL;

    if (config && config->emitter.tag_handles) {
        if (!asdf_yaml_document_add_tag_handles(doc, config->emitter.tag_handles))
            goto error;
    }

    return doc;
error:
    fy_document_destroy(doc);
    return NULL;
}


/** Utilities for path parser */


#define SKIP_WHITESPACE(p) \
    do { \
        while (*(p) && isspace(*(p))) \
            (p)++; \
    } while (0)


#define ASDF_YAML_PATH_MAX_INDEX_LEN 10


/**
 * Given a character ``brac`` determine if it is a known bracket character and
 * return the expected path target type for a key/index in that bracket
 *
 * The expected matching bracket is returned in ``closing_brac``.
 *
 * If ``[`` then a sequence index is expected; if ``'`` or ``"`` a mapping key
 * is expected; otherwise ambiguous.
 *
 * If the input character is not a supported bracket; ``closing_brac`` is set
 * to -1.
 */
static inline asdf_yaml_pc_target_t target_for_bracket(char brac, char *closing_brac) {
    assert(closing_brac);
    switch (brac) {
    case '[':
        // target had better be a sequence index
        *closing_brac = ']';
        return ASDF_YAML_PC_TARGET_SEQ;
    case '\'':
    case '\"':
        // target had better be a mapping key
        *closing_brac = brac;
        return ASDF_YAML_PC_TARGET_MAP;
    default:
        *closing_brac = -1;
        return ASDF_YAML_PC_TARGET_ANY;
    }
}


/** Return a copy of the path component or NULL if not valid */
static inline char *find_path_component_any(const char *start, const char *end, size_t *len) {
    assert(len);
    // In the unknown/ambiguous case, just scan until the next /
    // A '/' is not allowed in a key unless it's in a quoted key
    SKIP_WHITESPACE(start);
    const char *p = start;
    while (p < end && *p != '/') {
        // Invalid bracket encountered in a non-bracketed path
        // component
        if (strchr("'\"[]", *p))
            return NULL;

        p++;
    }
    *len = p - start;
    return strndup(start, *len);
}


static inline char *find_path_component_map(
    const char *start, const char *end, char closing_brac, size_t *len) {
    assert(start);
    assert(end);
    assert(len);
    const char *p = start;

    if (p == end)
        return NULL;

    while (p < end) {
        if (*p == '\\') {
            // Encountered an escape sequence; next character must be
            // one of the escaped characters
            if (!strchr("/*&.{}[]\\", *(++p)))
                return NULL;

            continue;
        }

        if (*p == closing_brac && (*(p + 1) == '\0' || *(p + 1) == '/')) {
            break;
        }

        p++;
    }

    // We never reached the end quote; invalid path
    if (p == end)
        return NULL;

    size_t key_len = p - start;
    // Advance past closing bracket
    *len = key_len + 1;
    return strndup(start, key_len);
}


static inline char *find_path_component_seq(
    const char *start, const char *end, char closing_brac, size_t *len) {
    // Must be a sequence index (opened with [)
    // We allow leading and trailing whitespace inside the brackets
    // like [ 0 ]; this seems to be consistent with libfyaml
    SKIP_WHITESPACE(start);
    size_t index_len = 0;
    const char *index_start = start;
    const char *p = start;

    if (p == end)
        return NULL;

    // Leading sign, OK
    if (*p == '-')
        p++;

    while (p < end) {
        if (!isdigit(*p)) {
            index_len = p - index_start;
            // must be followed by either whitespace or the closing
            // bracket
            SKIP_WHITESPACE(p);
            if (*p != closing_brac)
                return NULL;

            p++;
            SKIP_WHITESPACE(p);
            break;
        }

        p++;
    }

    if (index_len == 0)
        return NULL;

    *len = p - start;
    return strndup(index_start, index_len);
}


/**
 * Find the next path component given the expected target type, start position of the path, end
 * of the path
 *
 * Return the length of the path parsed into ``len`` (which may not be the length of the returned
 * path component since it includes any skipped whitespace, etc.).
 */
static inline char *find_path_component(
    asdf_yaml_pc_target_t target,
    const char *start,
    const char *end,
    char closing_brac,
    size_t *len) {
    switch (target) {
    case ASDF_YAML_PC_TARGET_ANY:
        return find_path_component_any(start, end, len);
    case ASDF_YAML_PC_TARGET_MAP:
        return find_path_component_map(start, end, closing_brac, len);
    case ASDF_YAML_PC_TARGET_SEQ:
        return find_path_component_seq(start, end, closing_brac, len);
    default:
        UNREACHABLE();
    }
}


static inline size_t parse_single_path_component(
    const char *start, const char *end, asdf_yaml_path_t *out_path) {
    assert(start);
    assert(end);
    assert(out_path);

    const char *p = start;
    char *key = NULL;
    char closing_brac = -1;
    size_t comp_len = 0;
    ssize_t index = 0;

    asdf_yaml_pc_target_t target = target_for_bracket(*p, &closing_brac);

    if (closing_brac > 0)
        p++;

    key = find_path_component(target, p, end, closing_brac, &comp_len);

    if (key == NULL)
        return 0;

    p += comp_len;

    // Determine if the key was a pure integer
    // If the target is ANY (ambiguous) we store both the string key and the
    // integer value on the path component.  Otherwise we determine here
    // that if the path component was not an integer, in which case it should
    // be treated as a mapping key
    if (target == ASDF_YAML_PC_TARGET_ANY || target == ASDF_YAML_PC_TARGET_SEQ) {
        char *end_idx = NULL;
        index = strtoll(key, &end_idx, ASDF_YAML_PATH_MAX_INDEX_LEN);

        // If it was not an integer it's invalid if we were in an explicit
        // sequence (in [] brackets); else it can still be a mapping key
        if (end_idx != NULL && *end_idx) {
            if (target == ASDF_YAML_PC_TARGET_SEQ)
                return 0;

            target = ASDF_YAML_PC_TARGET_MAP;
        }
    }

    asdf_yaml_path_push(
        out_path, (asdf_yaml_path_component_t){.target = target, .key = key, .index = index});

    // Advance past the trailing / if any
    if (p != end)
        p++;

    return p - start;
}


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
    size_t comp_len = 0;

    while (p != end) {
        comp_len = parse_single_path_component(p, end, out_path);

        if (comp_len == 0)
            goto invalid;

        p += comp_len;
    }

    return true;
invalid:
    asdf_yaml_path_clear(out_path);
    return false;
}
