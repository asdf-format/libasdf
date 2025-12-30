/**
 * Internal registration of compressor extensions
 *
 * Works similarly to the tag extension registry but on the "compression" string from a block
 * header.
 *
 * For now this interface is not publicly exposed but it could be later especially if the ASDF
 * standard formally grows extensible compression support.
 */
#include <stdatomic.h>

#include "../block.h"
#include "../context.h"
#include "../file.h"
#include "../log.h"

#include "asdf_compressor_map.h"

static asdf_compressor_map_t compressor_map = {0};
static atomic_bool compressor_map_initialized = false;


const asdf_compressor_t *asdf_compressor_get(asdf_file_t *file, const char *compression) {
    const asdf_compressor_map_value *ext = NULL;

    ext = asdf_compressor_map_get(&compressor_map, compression);

    if (!ext) {
        ASDF_LOG(file, ASDF_LOG_WARN, "no compressor registered for %s compression", compression);
        return NULL;
    }

    return (const asdf_compressor_t *)ext->second;
}


/**
 * TODO: These initializers/destructors for global mappings are repeated in a few places; could
 * easily generalize...
 */
ASDF_CONSTRUCTOR static void asdf_compressor_map_create() {
    if (atomic_load_explicit(&compressor_map_initialized, memory_order_acquire))
        return;

    compressor_map = asdf_compressor_map_init();
    atomic_store_explicit(&compressor_map_initialized, true, memory_order_release);
}


ASDF_DESTRUCTOR static void asdf_compressor_map_destroy(void) {
    if (atomic_load_explicit(&compressor_map_initialized, memory_order_acquire)) {
        asdf_compressor_map_drop(&compressor_map);
        atomic_store_explicit(&compressor_map_initialized, false, memory_order_release);
    }
}


void asdf_compressor_register(asdf_compressor_t *comp) {
    /* TODO: Handle overlaps on registration */
    // Ensure compressor map initialized
    asdf_compressor_map_create();
    const char *compression = comp->compression;

#ifdef ASDF_LOG_ENABLED
    asdf_global_context_t *ctx = asdf_global_context_get();
#endif

    size_t compression_len = strnlen(compression, ASDF_BLOCK_COMPRESSION_FIELD_SIZE);

    if (compression_len == 0) {
#ifdef ASDF_LOG_ENABLED
        ASDF_LOG(
            ctx, ASDF_LOG_ERROR, "empty compression name in compressor extension at 0x%px", comp);
#endif
        return;
    }

    cstr compression_cstr = cstr_with_n(compression, ASDF_BLOCK_COMPRESSION_FIELD_SIZE);

    asdf_compressor_map_result res = asdf_compressor_map_insert(
        &compressor_map, compression_cstr, comp);

#ifdef ASDF_LOG_ENABLED
    /* TODO: Improve extension registration logging; more details about each extension */
    if (res.inserted)
        ASDF_LOG(
            ctx,
            ASDF_LOG_DEBUG,
            "registered compressor extension for %s",
            cstr_str(&compression_cstr));
    else
        ASDF_LOG(
            ctx,
            ASDF_LOG_WARN,
            "failed to register compressor extension for %s",
            cstr_str(&compression_cstr));
#endif

    cstr_drop((cstr *)&compression_cstr);
}
