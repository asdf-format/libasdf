#include <stddef.h>

#include "config.h"

#ifdef HAVE_STATGRAB
#include <statgrab.h>
#endif

#include "util.h"


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
