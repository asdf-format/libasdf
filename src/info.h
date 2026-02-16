#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>

#include "file.h"
#include "util.h"


/**
 * Optional configuration options for the `asdf_info` function
 */
typedef struct {
    const char *filename;
    bool print_tree;
    bool print_blocks;
} asdf_info_cfg_t;


ASDF_EXPORT int asdf_info(asdf_file_t *file, FILE *out, const asdf_info_cfg_t *cfg);
