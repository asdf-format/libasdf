/**
 * Internal data structures for the stream interfaces; do no export
 */
#pragma once

#include <stdint.h>
#include <stdio.h>


/* Should be more than enough for most ASDF files, though some extreme ones will need many more */
#define ASDF_FILE_STREAM_INITIAL_MMAPS 256


typedef struct {
    void *addr;
    size_t size;
    off_t offset;
} file_mmap_info_t;


typedef struct {
    FILE *file;
    const char *filename;
    bool should_close;
    uint8_t *buf;
    size_t buf_size;
    size_t buf_avail;
    size_t buf_pos;
    off_t file_pos;

    // Array of pointers to mmap'd memory regions
    // Realistically these are for (non-overlapping) block data, so we simply track an
    // Array of addresses and their sizes
    file_mmap_info_t *mmaps;
    size_t mmaps_size;
} file_userdata_t;


typedef struct {
    const uint8_t *buf;
    size_t size;
    size_t pos;
} mem_userdata_t;
