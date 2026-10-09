#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <float.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "event.h"
#include "file.h"
#include "util.h"
#include "yaml.h"


const char *asdf_yaml_directive_prefix = ASDF_YAML_DIRECTIVE_PREFIX;
const char *asdf_yaml_directive = ASDF_YAML_DIRECTIVE;
const char *asdf_yaml_document_end_marker = ASDF_YAML_DOCUMENT_END_MARKER;
const char *asdf_yaml_empty_document = ASDF_YAML_DIRECTIVE ASDF_YAML_DOCUMENT_BEGIN_MARKER
    ASDF_YAML_DOCUMENT_END_MARKER;
const char *asdf_yaml_tag_prefix = "tag:";


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
    if (type < 0 || type >= (int)ARRAY_SIZE(fyet_to_asdf_event)) {
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


/**
 * Prefix a tag string with tag: if not already prefixed
 *
 * Memory is always allocated for the new string even if unmodified
 */
char *asdf_yaml_tag_canonicalize(const char *tag) {
    char *full_tag = NULL;
    if (0 != strncmp(tag, "tag:", ASDF_YAML_TAG_PREFIX_SIZE)) {
        size_t taglen = strlen(tag);
        full_tag = malloc(ASDF_YAML_TAG_PREFIX_SIZE + taglen + 1);

        if (!full_tag)
            return NULL;

        memcpy(full_tag, asdf_yaml_tag_prefix, ASDF_YAML_TAG_PREFIX_SIZE);
        memcpy(full_tag + ASDF_YAML_TAG_PREFIX_SIZE, tag, taglen + 1);
    } else {
        full_tag = strdup(tag);
    }

    return full_tag;
}


static asdf_yaml_tag_handle_t asdf_yaml_default_tag_handle = {
    .handle = ASDF_YAML_DEFAULT_TAG_HANDLE, .prefix = ASDF_STANDARD_TAG_PREFIX};


/**
 * Normalize tags to their shortened form taking into account tag handles to
 * be used in the document
 *
 * The libfyaml function fy_node_set_tag, nor its document emitter code has
 * any intelligence (that I've found so far) to shorten tags added to new
 * nodes based on what tag handles are available in the document, and just
 * takes the tag to write verbatim (as long as it correctly matches YAML
 * syntax for tags--must start with '!')
 *
 * This looks at all the available tag handles to be written to the document
 * (typically set in the emitter config) and chooses the handle with the
 * longest prefix matching the tag to normalize.  It also takes into
 * account the default behavior of using the '!' handle for asdf/core, if it
 * was not already explicitly included in the array of tag handles.
 *
 * .. todo::
 *
 *   This is a bit slow and cumbersome if we're writing a lot of tagged values,
 *   so might be useful to cache the handles to use for each tag written.  This
 *   should be done on the level of the asdf_file_t, maybe.
 */
char *asdf_yaml_tag_normalize(const char *tag, const asdf_yaml_tag_handle_t *handles) {
    char *normal_tag = NULL;
    bool has_default_tag_handle = false;
    size_t prefix_len = 0;
    size_t longest_prefix = 0;
    const asdf_yaml_tag_handle_t *handle = NULL;
    const asdf_yaml_tag_handle_t *tmp_handle = handles;
    char *canonical_tag = asdf_yaml_tag_canonicalize(tag);

    if (!canonical_tag)
        return NULL;

    while (tmp_handle && tmp_handle->prefix && tmp_handle->handle) {
        prefix_len = strlen(tmp_handle->prefix);
        if (strncmp(canonical_tag, tmp_handle->prefix, prefix_len) == 0) {
            if (prefix_len > longest_prefix) {
                handle = tmp_handle;
                longest_prefix = prefix_len;
            }
        }

        if (strcmp(tmp_handle->handle, ASDF_YAML_DEFAULT_TAG_HANDLE) == 0)
            has_default_tag_handle = true;

        tmp_handle++;
    }

    // Check if it matches the standard asdf/core tag prefix
    if (!handle && !has_default_tag_handle) {
        prefix_len = strlen(ASDF_STANDARD_TAG_PREFIX);
        if (strncmp(canonical_tag, ASDF_STANDARD_TAG_PREFIX, prefix_len) == 0)
            handle = &asdf_yaml_default_tag_handle;
    }

    int ret = -1;

    if (!handle)
        // Does not match any tag handle, so write the tag in full verbatim
        // format
        ret = asprintf(&normal_tag, "!<%s>", canonical_tag);
    else
        ret = asprintf(&normal_tag, "%s%s", handle->handle, canonical_tag + prefix_len);

    free(canonical_tag);

    if (ret <= 0)
        return NULL;

    return normal_tag;
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


struct fy_node *asdf_yaml_node_set_style(
    struct fy_document *doc, struct fy_node *node, asdf_yaml_node_style_t style) {
    // We might have some other interesting options here to control wrapping,
    // etc. that can be later determined by the global emitter configuration for
    // the file
    enum fy_emitter_cfg_flags flags = style == ASDF_YAML_NODE_STYLE_BLOCK ? FYECF_MODE_BLOCK
                                                                          : FYECF_MODE_FLOW;
    char *out = fy_emit_node_to_string(node, flags);

    if (!out)
        return NULL;

    return fy_node_build_from_malloc_string(doc, out, FY_NT);
}


/*
 * Implicit scalar resolution
 *
 * Plain (untagged, unquoted) scalars are resolved using the YAML 1.1 types
 * (https://yaml.org/type/) mandated by the ASDF Standard, consistent with the
 * resolver used by PyYAML (and hence Python asdf):
 *
 * - null: ``~``, ``null``, ``Null``, ``NULL``, or the empty scalar
 * - bool: ``true``/``false``, ``yes``/``no``, ``on``/``off`` in lowercase,
 *   Titlecase, or UPPERCASE
 * - int: decimal, ``0b`` binary, ``0`` octal, ``0x`` hexadecimal, and base 60
 *   (``1:30``), all allowing ``_`` digit separators
 * - float: decimal with a ``.`` and an optional signed exponent, base 60
 *   (``1:30.5``), ``[-+]?.inf``, and ``.nan``
 *
 * As a leniency, decimal floats with an exponent but no ``.`` (``1e-08``), or
 * with an unsigned exponent (``1.0e8``) are also read as floats.  Strictly
 * these are strings in YAML 1.1, but they are floats in YAML 1.2, and older
 * versions of libasdf wrote floats in that form.  libasdf never writes them.
 */
static inline bool scalar_equals(const char *scalar, size_t len, const char *str) {
    return (bool)((strlen(str) == len) && (0 == strncmp(scalar, str, len)));
}


bool asdf_yaml_scalar_is_null(const char *scalar, size_t len) {
    return (bool)(!scalar || len == 0 || scalar_equals(scalar, len, "null") ||
                  scalar_equals(scalar, len, "Null") || scalar_equals(scalar, len, "NULL") ||
                  scalar_equals(scalar, len, "~"));
}


static const char *const yaml_true_strs[] = {
    "true", "True", "TRUE", "yes", "Yes", "YES", "on", "On", "ON", NULL};


static const char *const yaml_false_strs[] = {
    "false", "False", "FALSE", "no", "No", "NO", "off", "Off", "OFF", NULL};


bool asdf_yaml_scalar_is_bool(const char *scalar, size_t len, bool *value) {
    if (!scalar)
        return false;

    /* Allow 0 and 1 tagged as bool */
    if (len == 1) {
        if (scalar[0] == '0') {
            *value = false;
            return true;
        }

        if (scalar[0] == '1') {
            *value = true;
            return true;
        }
    }

    for (const char *const *str = yaml_true_strs; *str; str++) {
        if (scalar_equals(scalar, len, *str)) {
            *value = true;
            return true;
        }
    }

    for (const char *const *str = yaml_false_strs; *str; str++) {
        if (scalar_equals(scalar, len, *str)) {
            *value = false;
            return true;
        }
    }

    return false;
}


/* Radixes of YAML 1.1 ints and floats */
enum {
    RADIX_BIN = 2,
    RADIX_OCT = 8,
    RADIX_DEC = 10,
    RADIX_HEX = 16,
    RADIX_SEXAGESIMAL = 60,
};


// NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
static bool is_digit_in_base(char chr, int base) {
    switch (base) {
    case RADIX_BIN:
        return (bool)(chr == '0' || chr == '1');
    case RADIX_OCT:
        return (bool)(chr >= '0' && chr <= '7');
    case RADIX_HEX:
        return isxdigit((unsigned char)chr);
    default:
        return isdigit((unsigned char)chr);
    }
}


/** Return the length of the run of ``_`` and base ``base`` digits at ``ptr`` */
static size_t span_digits(const char *ptr, const char *end, int base) {
    const char *start = ptr;

    while (ptr < end && (*ptr == '_' || is_digit_in_base(*ptr, base)))
        ptr++;

    return ptr - start;
}


/** Return the length of the match for ``(:[0-5]?[0-9])*`` at ``ptr`` */
static size_t span_base60(const char *ptr, const char *end) {
    const char *start = ptr;

    while (ptr + 1 < end && ptr[0] == ':' && isdigit((unsigned char)ptr[1])) {
        if (ptr + 2 < end && isdigit((unsigned char)ptr[2])) {
            if (ptr[1] > '5')
                break;

            ptr += 3;
        } else {
            ptr += 2;
        }
    }

    return ptr - start;
}


/**
 * Copy ``[start, end)`` to a new string with ``_`` digit separators removed
 *
 * If ``decimal_point`` is not ``NULL``, any ``.`` is replaced with it, so that
 * the result can be parsed by `strtod` regardless of the current locale.
 */
static char *copy_digits(const char *start, const char *end, const char *decimal_point) {
    size_t decimal_len = decimal_point ? strlen(decimal_point) : 1;
    char *buf = malloc(((end - start) * decimal_len) + 1);

    if (!buf)
        return NULL;

    char *out = buf;

    for (; start < end; start++) {
        if (*start == '_')
            continue;

        if (*start == '.' && decimal_point) {
            memcpy(out, decimal_point, decimal_len);
            out += decimal_len;
        } else {
            *out++ = *start;
        }
    }

    *out = '\0';
    return buf;
}


/** Parse the digits in ``[start, end)``, ignoring ``_``, as an unsigned int */
static asdf_value_err_t parse_uint_digits(
    const char *start, const char *end, int base, uint64_t *value) {
    char *digits = copy_digits(start, end, NULL);

    if (!digits)
        return ASDF_VALUE_ERR_OOM;

    if (!*digits) {
        free(digits);
        return ASDF_VALUE_ERR_PARSE_FAILURE;
    }

    errno = 0;
    *value = strtoull(digits, NULL, base);
    free(digits);
    return errno == ERANGE ? ASDF_VALUE_ERR_OVERFLOW : ASDF_VALUE_OK;
}


/**
 * Check whether a bare scalare is an integer, including support for
 * non-base-10 integers, thousands place separators, etc.
 */
asdf_value_err_t asdf_yaml_scalar_is_int(
    const char *scalar, size_t len, bool *negative, uint64_t *magnitude) {
    if (!scalar)
        return ASDF_VALUE_ERR_UNKNOWN;

    const char *ptr = scalar;
    const char *end = scalar + len;
    const char *digits = NULL;
    int base = RADIX_DEC;
    size_t span = 0;
    asdf_value_err_t err = ASDF_VALUE_OK;
    uint64_t component = 0;

    *negative = false;

    if (ptr < end && (*ptr == '-' || *ptr == '+')) {
        *negative = *ptr == '-';
        ptr++;
    }

    if (ptr >= end || !isdigit((unsigned char)*ptr))
        return ASDF_VALUE_ERR_PARSE_FAILURE;

    // Handle non-base-10 integer scalars
    if (*ptr == '0' && ptr + 1 < end) {
        /* 0b binary, 0x hex, or 0 octal; the leading 0 is kept as one of the
         * octal digits so that e.g. 0_ is still 0 */
        switch (ptr[1]) {
        case 'b':
            base = RADIX_BIN;
            digits = ptr + 2;
            break;
        case 'x':
            base = RADIX_HEX;
            digits = ptr + 2;
            break;
        default:
            base = RADIX_OCT;
            digits = ptr;
            break;
        }

        span = span_digits(digits, end, base);

        if (digits + span != end)
            return ASDF_VALUE_ERR_PARSE_FAILURE;

        return parse_uint_digits(digits, end, base, magnitude);
    }

    /* Decimal 0 or [1-9][0-9_]*, optionally followed by base 60 components */
    digits = ptr;
    ptr += span_digits(ptr, end, base);

    if (ptr == end)
        return parse_uint_digits(digits, end, base, magnitude);

    if (*digits == '0' || ptr + span_base60(ptr, end) != end)
        return ASDF_VALUE_ERR_PARSE_FAILURE;

    err = parse_uint_digits(digits, ptr, RADIX_DEC, magnitude);

    if (err != ASDF_VALUE_OK)
        return err;

    while (ptr < end) {
        /* Skip the colon; each component is one or two decimal digits */
        ptr++;
        component = (uint64_t)(*ptr++ - '0');

        if (ptr < end && *ptr != ':')
            component = (component * RADIX_DEC) + (uint64_t)(*ptr++ - '0');

        if (*magnitude > (UINT64_MAX - component) / RADIX_SEXAGESIMAL)
            return ASDF_VALUE_ERR_OVERFLOW;

        *magnitude = (*magnitude * RADIX_SEXAGESIMAL) + component;
    }

    return ASDF_VALUE_OK;
}


/**
 * Parse the YAML 1.1 special float values ``[-+]?.inf`` and ``.nan``
 *
 * Only the lowercase, titlecase, and uppercase spellings are allowed, and
 * NaN may not be signed.
 */
static bool is_yaml_special_float(const char *scalar, size_t len, double *value) {
    char sign = '\0';

    if (len == 5 && (scalar[0] == '-' || scalar[0] == '+')) {
        sign = scalar[0];
        scalar++;
        len--;
    }

    if (len != 4)
        return false;

    if ((0 == strncmp(scalar, ".inf", len)) || (0 == strncmp(scalar, ".Inf", len)) ||
        (0 == strncmp(scalar, ".INF", len))) {
        *value = (sign == '-') ? -INFINITY : INFINITY;
        return true;
    }

    if (sign)
        return false;

    if ((0 == strncmp(scalar, ".nan", len)) || (0 == strncmp(scalar, ".NaN", len)) ||
        (0 == strncmp(scalar, ".NAN", len))) {
        *value = NAN;
        return true;
    }

    return false;
}


/**
 * Convert an already-validated decimal float in ``[start, end)`` to a double
 *
 * Returns ``ASDF_VALUE_ERR_OVERFLOW`` (with ``*value`` set to +/-``HUGE_VAL``)
 * if its magnitude is too large for a double.  Underflow is not an error:
 * the result is the nearest subnormal or zero, as for Python's `float`.
 */
static asdf_value_err_t parse_float_digits(const char *start, const char *end, double *value) {
    /* strtod honors LC_NUMERIC; translate . to the locale's decimal point */
    char *digits = copy_digits(start, end, localeconv()->decimal_point);

    if (!digits)
        return ASDF_VALUE_ERR_OOM;

    errno = 0;
    *value = strtod(digits, NULL);
    free(digits);

    if (errno == ERANGE && isinf(*value))
        return ASDF_VALUE_ERR_OVERFLOW;

    return ASDF_VALUE_OK;
}


/** Return ``ptr`` advanced past an optional leading ``-`` or ``+`` */
static const char *skip_sign(const char *ptr, const char *end) {
    return (ptr < end && (*ptr == '-' || *ptr == '+')) ? ptr + 1 : ptr;
}


/** Return the length of the match for ``[0-9][0-9_]*`` at ``ptr`` */
static size_t span_decimal_int(const char *ptr, const char *end) {
    if (ptr >= end || !isdigit((unsigned char)*ptr))
        return 0;

    return span_digits(ptr, end, RADIX_DEC);
}


/** Return the length of the match for ``[eE][-+]?[0-9]+`` at ``ptr`` */
static size_t span_exponent(const char *ptr, const char *end) {
    const char *start = ptr;
    const char *digits = NULL;

    if (ptr >= end || (*ptr != 'e' && *ptr != 'E'))
        return 0;

    digits = skip_sign(ptr + 1, end);
    ptr = digits;

    while (ptr < end && isdigit((unsigned char)*ptr))
        ptr++;

    return ptr == digits ? 0 : ptr - start;
}


/**
 * Match a decimal float
 *
 * That is ``[-+]?`` followed by a mantissa of either ``[0-9][0-9_]*`` with
 * an optional ``\.[0-9_]*``, or ``\.[0-9][0-9_]*``, followed by an optional
 * exponent ``[eE][-+]?[0-9]+``.  Either the ``.`` or the exponent is
 * required, unless ``allow_int``.
 *
 * This is slightly more lenient than YAML 1.1, which always requires the
 * ``.`` and that the exponent is always signed ``[-+]``.
 */
static bool is_decimal_float(const char *scalar, size_t len, bool allow_int) {
    const char *end = scalar + len;
    const char *ptr = skip_sign(scalar, end);
    size_t int_len = span_decimal_int(ptr, end);
    size_t exp_len = 0;
    bool has_decimal = false;

    ptr += int_len;

    if (ptr < end && *ptr == '.') {
        has_decimal = true;
        ptr++;

        /* .5 is a float but . and ._ are not */
        if (int_len == 0 && span_decimal_int(ptr, end) == 0)
            return false;

        ptr += span_digits(ptr, end, RADIX_DEC);
    } else if (int_len == 0) {
        return false;
    }

    exp_len = span_exponent(ptr, end);
    ptr += exp_len;
    return (bool)(ptr == end && (has_decimal || exp_len > 0 || allow_int));
}


/** Match a base 60 float ``[-+]?[0-9][0-9_]*(:[0-5]?[0-9])+\.[0-9_]*`` */
static bool is_base60_float(const char *scalar, size_t len) {
    const char *end = scalar + len;
    const char *ptr = skip_sign(scalar, end);
    size_t int_len = span_decimal_int(ptr, end);
    size_t base60_len = 0;

    if (int_len == 0)
        return false;

    ptr += int_len;
    base60_len = span_base60(ptr, end);

    if (base60_len == 0)
        return false;

    ptr += base60_len;

    if (ptr >= end || *ptr != '.')
        return false;

    ptr++;
    return ptr + span_digits(ptr, end, RADIX_DEC) == end;
}


/**
 * Parse a base 60 float already matched by `is_base60_float`
 *
 * As in PyYAML, each ``:``-separated component is parsed as a float, so the
 * last component carries the fractional part.
 */
static asdf_value_err_t parse_base60_float(const char *scalar, size_t len, double *value) {
    const char *end = scalar + len;
    const char *ptr = skip_sign(scalar, end);
    const char *comp_end = NULL;
    double component = 0.0;
    asdf_value_err_t err = ASDF_VALUE_OK;

    *value = 0.0;

    while (ptr < end) {
        comp_end = memchr(ptr, ':', end - ptr);

        if (!comp_end)
            comp_end = end;

        err = parse_float_digits(ptr, comp_end, &component);

        if (err != ASDF_VALUE_OK)
            return err;

        *value = (*value * RADIX_SEXAGESIMAL) + component;
        ptr = comp_end == end ? end : comp_end + 1;
    }

    if (*scalar == '-')
        *value = -*value;

    return isinf(*value) ? ASDF_VALUE_ERR_OVERFLOW : ASDF_VALUE_OK;
}


asdf_value_err_t asdf_yaml_scalar_is_float(
    const char *scalar, size_t len, bool allow_int, double *value) {
    asdf_value_err_t err = ASDF_VALUE_ERR_PARSE_FAILURE;

    if (!scalar)
        return ASDF_VALUE_ERR_UNKNOWN;

    /* Most common forms first */
    if (is_decimal_float(scalar, len, allow_int))
        err = parse_float_digits(scalar, scalar + len, value);
    else if (is_yaml_special_float(scalar, len, value))
        err = ASDF_VALUE_OK;
    else if (is_base60_float(scalar, len))
        err = parse_base60_float(scalar, len, value);

    return err;
}


/* Number of digits in the year that begins a YAML 1.1 timestamp */
enum { YEAR_DIGITS = 4 };


bool asdf_yaml_string_is_ambiguous(const char *str, size_t len) {
    bool b_val = false;
    bool negative = false;
    uint64_t u_val = 0;
    double d_val = 0.0;
    const char *ptr = str;
    const char *end = str + len;

    if (asdf_yaml_scalar_is_null(str, len) ||
        (len > 1 && asdf_yaml_scalar_is_bool(str, len, &b_val)))
        return true;

    if (scalar_equals(str, len, "<<") || scalar_equals(str, len, "="))
        return true;

    /* The YAML 1.1 spec (though not PyYAML or libasdf) also resolves
     * y/Y/n/N as bools */
    if (len == 1 && strchr("yYnN", *str))
        return true;

    /* Leading sign, digit, or . are required for any int, float, or timestamp
     * so skip the more expensive checks otherwise */
    if (len == 0 || !(isdigit((unsigned char)*str) || *str == '-' || *str == '+' || *str == '.'))
        return false;

    if (asdf_yaml_scalar_is_int(str, len, &negative, &u_val) != ASDF_VALUE_ERR_PARSE_FAILURE)
        return true;

    if (asdf_yaml_scalar_is_float(str, len, true, &d_val) != ASDF_VALUE_ERR_PARSE_FAILURE)
        return true;

    /* YAML 1.2 octal 0o[0-7]+ */
    ptr = skip_sign(ptr, end);

    if (end - ptr > 2 && ptr[0] == '0' && ptr[1] == 'o' &&
        span_digits(ptr + 2, end, RADIX_OCT) == (size_t)(end - ptr - 2))
        return true;

    size_t digits = span_digits(str, str + YEAR_DIGITS, RADIX_DEC);

    /* YAML 1.1 timestamps all start with a 4 digit year followed by - */
    return (bool)(len > YEAR_DIGITS && digits == YEAR_DIGITS && str[YEAR_DIGITS] == '-');
}


bool asdf_yaml_scalar_is_number(const char *str, size_t len) {
    bool negative = false;
    uint64_t u_val = 0;
    double d_val = 0.0;

    if (!str || len == 0)
        return false;

    asdf_value_err_t is_int = asdf_yaml_scalar_is_int(str, len, &negative, &u_val);
    asdf_value_err_t is_float = asdf_yaml_scalar_is_float(str, len, false, &d_val);

    return (bool)(is_int != ASDF_VALUE_ERR_PARSE_FAILURE ||
                  is_float != ASDF_VALUE_ERR_PARSE_FAILURE);
}


/**
 * Range of decimal exponents for which floats are formatted in fixed rather
 * than scientific notation, as by Python's `repr`
 */
enum { REPR_MIN_FIXED_EXPONENT = -4, REPR_MAX_FIXED_EXPONENT = 15 };


/**
 * Float formatting
 *
 * The goal is the shortest string that reads back as exactly the same value,
 * spelled so that a YAML 1.1 resolver reads it as a float.  The obvious
 * ``printf`` formats, which we used to use for simplicity's sake each fail
 * one of these: ``%.17g`` always round-trips but is noisy
 * (``0.10000000000000001``), while ``%g`` at any precision can drop the ``.``
 * (``1e-08``, ``100``), and without a ``.`` a strict YAML 1.1 resolver reads
 * the scalar as an int or a string, not a float.
 *
 * This is good enough to correctly write YAML floats without including a
 * full ``dtoa`` library such as Ryu as a dependency.
 *
 * Non-finite values are handled up front (``.nan``, ``.inf``, ``-.inf``).
 * Finite values take four steps:
 *
 * 1. Find the shortest precision that round-trips.  Format with ``%.*e`` at
 *    1, 2, ... significant digits and stop at the first that parses back to
 *    the same value.  ``DBL_DECIMAL_DIG`` (17) digits always round-trip a
 *    double, so the search is bounded.  This is a brute-force stand-in for
 *    the dedicated shortest-digits algorithms (e.g. Ryu) behind Python's
 *    ``repr``, costing at most 17 ``snprintf``/``strtod`` pairs per value.
 *
 *    For ``single`` (precision) the round-trip is checked with ``strtof``
 *    against the value narrowed back to float, bounded by ``FLT_DECIMAL_DIG``
 *    (9).  This gives float32 values their own shortest form: 0.1f is written
 *    ``0.1``, not ``0.10000000149011612`` as it would be if treated as a
 *    double.
 *
 * 2. Choose fixed or scientific notation, like ``repr``: fixed for decimal
 *    exponents in [-4, 15], else scientific.  The exponent is read from the
 *    ``%e`` output rather than computed with ``log10``, because rounding can
 *    carry into it (9.96 at 2 digits is ``1.0e+01``).  For fixed notation the
 *    value is reformatted with ``%f`` and ``prec - 1 - exponent`` fractional
 *    digits.  That rounds at the same digit position as the ``%e`` form, so
 *    it gives the same significant digits without the exponent (``1e+02``
 *    becomes ``100``, ``1.5e-04`` becomes ``0.00015``).  In scientific
 *    notation, ``%e`` always signs the exponent (``e-08``, ``e+16``), as
 *    YAML 1.1 requires.
 *
 * 3. Delocalize.  ``printf`` writes the locale's decimal point (e.g. ``,``
 *    under fr_FR), so it is replaced with ``.``.  This has to come after
 *    step 1, since ``strtod`` also expects the locale's decimal point.  The
 *    locale's decimal point may be more than one byte, so the rest of the
 *    string is shifted down over the extra bytes.
 *
 * 4. Make sure there is a ``.``.  A one-digit mantissa or an integral fixed
 *    value has none, so ``.0`` is inserted before the ``e``, or appended:
 *    ``1e-08`` becomes ``1.0e-08`` and ``100`` becomes ``100.0``.
 */
void asdf_yaml_format_float(char *buf, size_t size, double val, bool single) {
    int max_prec = single ? FLT_DECIMAL_DIG : DBL_DECIMAL_DIG;
    int prec = 1;
    int exponent = 0;
    char *dot = NULL;
    char *exp = NULL;
    const char *decimal_point = localeconv()->decimal_point;
    size_t decimal_len = strlen(decimal_point);
    size_t len = 0;

    if (isnan(val)) {
        snprintf(buf, size, ".nan");
        return;
    }

    if (isinf(val)) {
        snprintf(buf, size, "%s", signbit(val) ? "-.inf" : ".inf");
        return;
    }

    /* 1. Shortest round-trip precision */
    for (; prec < max_prec; prec++) {
        snprintf(buf, size, "%.*e", prec - 1, val);

        if (single ? strtof(buf, NULL) == (float)val : strtod(buf, NULL) == val)
            break;
    }

    if (prec == max_prec)
        snprintf(buf, size, "%.*e", prec - 1, val);

    /* 2. Fixed or scientific notation */
    exp = strchr(buf, 'e');
    assert(exp);
    exponent = atoi(exp + 1);

    if (exponent >= REPR_MIN_FIXED_EXPONENT && exponent <= REPR_MAX_FIXED_EXPONENT) {
        int frac_digits = prec - 1 - exponent;
        snprintf(buf, size, "%.*f", frac_digits > 0 ? frac_digits : 0, val);
    }

    /* 3. Replace the locale's decimal point, if not ., with . */
    dot = strstr(buf, decimal_point);

    if (dot && 0 != strcmp(decimal_point, ".")) {
        *dot = '.';
        memmove(dot + 1, dot + decimal_len, strlen(dot + decimal_len) + 1);
    }

    if (strchr(buf, '.'))
        return;

    /* 4. No . so insert .0 before the exponent if any, else append it
     *
     * Some YAML parsers will strictly reject any scalar not containing a
     * a decimal as a non-floating-point value.  This includes AST, so that
     * . better get in there somewhere.
     */
    len = strlen(buf);
    exp = strchr(buf, 'e');

    if (len + 2 >= size)
        return;

    if (exp) {
        memmove(exp + 2, exp, strlen(exp) + 1);
        exp[0] = '.';
        exp[1] = '0';
    } else {
        buf[len] = '.';
        buf[len + 1] = '0';
        buf[len + 2] = '\0';
    }
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
            if (target == ASDF_YAML_PC_TARGET_SEQ) {
                free(key);
                return 0;
            }

            target = ASDF_YAML_PC_TARGET_MAP;
        }
    }

    asdf_yaml_path_component_t comp = {.target = target, .key = key, .index = index};
    asdf_yaml_path_push(out_path, comp);

    // Advance past the trailing / if any
    if (p != end)
        p++;

    // NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
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
        char *key = strdup("");

        if (!key)
            return false;

        asdf_yaml_path_component_t comp = {.target = ASDF_YAML_PC_TARGET_MAP, .key = key};
        asdf_yaml_path_push(out_path, comp);
        // NOLINTNEXTLINE(clang-analyzer-unix.Malloc)
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
