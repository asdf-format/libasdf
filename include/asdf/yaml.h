#ifndef ASDF_YAML_H
#define ASDF_YAML_H

#include <asdf/event.h>
#include <asdf/util.h>

ASDF_BEGIN_DECLS

typedef enum {
    // Not a YAML event
    ASDF_YAML_NONE_EVENT = 0,
    ASDF_YAML_STREAM_START_EVENT,
    ASDF_YAML_STREAM_END_EVENT,
    ASDF_YAML_DOCUMENT_START_EVENT,
    ASDF_YAML_DOCUMENT_END_EVENT,
    ASDF_YAML_MAPPING_START_EVENT,
    ASDF_YAML_MAPPING_END_EVENT,
    ASDF_YAML_SEQUENCE_START_EVENT,
    ASDF_YAML_SEQUENCE_END_EVENT,
    ASDF_YAML_SCALAR_EVENT,
    ASDF_YAML_ALIAS_EVENT
} asdf_yaml_event_type_t;


ASDF_EXPORT const char *asdf_yaml_event_scalar_value(const asdf_event_t *event, size_t *lenp);
ASDF_EXPORT const char *asdf_yaml_event_tag(const asdf_event_t *event, size_t *lenp);
ASDF_EXPORT asdf_yaml_event_type_t asdf_yaml_event_type(const asdf_event_t *event);
ASDF_EXPORT const char *asdf_yaml_event_type_text(const asdf_event_t *event);

ASDF_END_DECLS

#endif  /* ASDF_YAML_H */
