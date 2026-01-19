#include <stdatomic.h>
#include <stdlib.h>

#include "context.h"
#include "error.h"
#include "file.h"
#include "log.h"
#include "types/asdf_extension_map.h"
#include "yaml.h"


static asdf_extension_map_t extension_map = {0};
static atomic_bool extension_map_initialized = false;


const asdf_extension_t *asdf_extension_get(asdf_file_t *file, const char *tag) {
    const asdf_extension_map_value *ext = NULL;
    char *full_tag = asdf_yaml_tag_canonicalize(tag);

    if (!full_tag) {
        ASDF_ERROR_OOM(file);
        return NULL;
    }

    ext = asdf_extension_map_get(&extension_map, full_tag);

    if (!ext) {
        ASDF_LOG(file, ASDF_LOG_TRACE, "no extension registered for tag %s", full_tag);
        return NULL;
    }

    free(full_tag);
    return (const asdf_extension_t *)ext->second;
}


ASDF_CONSTRUCTOR static void asdf_extension_map_create() {
    if (atomic_load_explicit(&extension_map_initialized, memory_order_acquire))
        return;

    extension_map = asdf_extension_map_init();
    atomic_store_explicit(&extension_map_initialized, true, memory_order_release);
}


ASDF_DESTRUCTOR static void asdf_extension_map_destroy(void) {
    if (atomic_load_explicit(&extension_map_initialized, memory_order_acquire)) {
        asdf_extension_map_drop(&extension_map);
        atomic_store_explicit(&extension_map_initialized, false, memory_order_release);
    }
}


void asdf_extension_register(asdf_extension_t *ext) {
    /* TODO: Handle tag overlaps on registration */
    // Ensure extension map initialized
    asdf_extension_map_create();
    const char *tag = ext->tag;

#ifdef ASDF_LOG_ENABLED
    asdf_global_context_t *ctx = asdf_global_context_get();
#endif

    char *full_tag = asdf_yaml_tag_canonicalize(tag);
    if (!full_tag) {
#ifdef ASDF_LOG_ENABLED
        ASDF_LOG(ctx, ASDF_LOG_FATAL, "failed to allocate memory for extension tag %s", tag);
#endif
        return;
    }
    asdf_extension_map_result res = asdf_extension_map_emplace(&extension_map, full_tag, ext);

#ifdef ASDF_LOG_ENABLED
    /* TODO: Improve extension registration logging; more details about each extension */
    if (res.inserted)
        ASDF_LOG(ctx, ASDF_LOG_DEBUG, "registered extension for tag %s", full_tag);
    else
        ASDF_LOG(ctx, ASDF_LOG_WARN, "failed to register extension for tag %s", full_tag);
#endif

    free(full_tag);
}
