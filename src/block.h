/**
 * ASDF block definitions
 */

#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

#include "stream.h"
#include "util.h"


#define ASDF_BLOCK_COMPRESSION_FIELD_SIZE 4
#define ASDF_BLOCK_CHECKSUM_FIELD_SIZE 16
// Currently always 48, but may be expanded on different versions of the standard
#define ASDF_BLOCK_HEADER_SIZE 48
// Size of the *full* block header including the block magic and header size fields.
#define ASDF_BLOCK_HEADER_FULL_SIZE (ASDF_BLOCK_HEADER_SIZE + ASDF_BLOCK_MAGIC_SIZE + 2)
#define ASDF_BLOCK_MAGIC_SIZE 4

// Offsets starting from after header_size
#define ASDF_BLOCK_FLAGS_OFFSET 0
#define ASDF_BLOCK_COMPRESSION_OFFSET 4
#define ASDF_BLOCK_ALLOCATED_SIZE_OFFSET 8
#define ASDF_BLOCK_USED_SIZE_OFFSET 16
#define ASDF_BLOCK_DATA_SIZE_OFFSET 24
#define ASDF_BLOCK_CHECKSUM_OFFSET 32

#define ASDF_BLOCK_INDEX_HEADER_SIZE 17


extern const unsigned char asdf_block_magic[];


extern const char asdf_block_index_header[];


typedef enum {
    ASDF_BLOCK_FLAG_STREAMED = 0x1,
} asdf_block_flag_t;


typedef struct asdf_block_header {
    //  4 MAGIC 4 char == "\323BLK"
    // char magic[4];
    // = {0xd3, "B", "L", "K"};

    //  2 HEADER SIZE 16 bit unsigned int
    //     doesn't include magic or header size (used to optionally align
    //     blocks with filesystem blocks)
    uint16_t header_size;

    //  4 FLAGS 32 bit unsigned int (only first bit used for stream block)
    uint32_t flags;

    //  4 COMPRESSION 4 char
    char compression[ASDF_BLOCK_COMPRESSION_FIELD_SIZE];

    //  8 ALLOCATED SIZE 64 bit unsigned int
    uint64_t allocated_size;

    //  8 USED SIZE 64 bit unsigned int
    uint64_t used_size;

    //  8 DATA SIZE 64 bit unsigned int
    uint64_t data_size;

    // 16 CHECKSUM 16 char MD5 checksum (optional)
    uint8_t checksum[ASDF_BLOCK_CHECKSUM_FIELD_SIZE];
} asdf_block_header_t;


typedef struct asdf_block_info {
    asdf_block_header_t header;
    off_t header_pos;
    off_t data_pos;
    size_t index;
} asdf_block_info_t;


/**
 * Returns `true` if the given buffer begins with the ASDF block magic
 */
static inline bool is_block_magic(const uint8_t *buf, size_t len) {
    if (len < ASDF_BLOCK_MAGIC_SIZE)
        return false;

    return memcmp(buf, asdf_block_magic, (size_t)ASDF_BLOCK_MAGIC_SIZE) == 0;
}


ASDF_LOCAL bool asdf_block_info_read(asdf_stream_t *stream, asdf_block_info_t *out_block);
