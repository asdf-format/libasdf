#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

#ifdef HAVE_STATGRAB
#include <statgrab.h>
#endif

#include "util.h"


void asdf_free(void *ptr) {
    free(ptr);
}


size_t asdf_util_get_total_memory(void) {
#ifndef HAVE_STATGRAB
    return 0;
#else
    sg_init(1); // TODO: Maybe move this to somewhere else like during library init
    size_t entries = 0;
    sg_mem_stats *mem_stats = sg_get_mem_stats(&entries);
    sg_shutdown();

    if (!mem_stats || entries < 1)
        return 0;

    return mem_stats->total;
#endif
}


void **asdf_array_concat(void **dst, const void **src) {
    size_t dst_len = 0;
    size_t src_len = 0;
    void *new_dst = NULL;

    for (const void **p = src; *p; ++p)
        src_len++;

    if (!dst) {
        new_dst = malloc((src_len + 1) * sizeof(*src));

        if (!new_dst)
            return NULL;
    } else {
        for (void **p = dst; *p; ++p)
            dst_len++;

        new_dst = realloc((void *)dst, (dst_len + src_len + 1) * sizeof(*dst));

        if (!new_dst)
            return NULL;
    }

    memcpy(new_dst + (dst_len * sizeof(*dst)), (const void *)src, (src_len + 1) * sizeof(*src));
    return (void **)new_dst;
}
