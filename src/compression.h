/** Internal compressed block utilities */
#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <bzlib.h>
#include <zlib.h>

#include <asdf/file.h>


// Forward-declaration
typedef struct _asdf_block_comp_state_t asdf_block_comp_state_t;

#ifdef HAVE_USERFAULTFD
/** Additional state for lazy decompression with userfaultfd */
typedef struct {
    asdf_block_comp_state_t *comp_state;
    /** File descriptor for the UUFD handle */
    int uffd;
    pthread_t handler_thread;
    /** Signal the thread to stop */
    atomic_bool stop;
    /**
     * Internal work buffer for page fault handling; the main decompression
     * work buffer is se to this
     */
    uint8_t *work_buf;
    size_t work_buf_size;
} asdf_block_comp_userfaultfd_t;
#endif

/**
 * Stores state and info for block decompression
 */
typedef struct _asdf_block_comp_state_t {
    asdf_file_t *file;
    asdf_block_comp_t comp;
    int fd;
    bool own_fd;
    size_t produced;
    uint8_t *dest;
    size_t dest_size;

    /**
     * Decompression scratch buffer
     * In eager decompression it is the same as the main dest buffer but it
     * can also be used for incremental work e.g. in lazy decompression mode
     */
    uint8_t *work_buf;
    size_t work_buf_size;

    /**
     * Compression-lib-specific data, effectively something like
     *
     * z_stream and bz_stream are included since these are built in, but we
     * leave open the possibility for others in a void*
     */
    union {
        z_stream *z;
        bz_stream *bz;
        void *uz;
    };

    /** Additional state for lazy decompression, if any */
    union {
#ifdef HAVE_USERFAULTFD
        asdf_block_comp_userfaultfd_t *userfaultfd;
#endif
        void *_reserved;
    } lazy;
} asdf_block_comp_state_t;


// Forward-declaration
typedef struct asdf_block asdf_block_t;


ASDF_LOCAL asdf_block_comp_t asdf_block_comp_parse(asdf_file_t *file, const char *compression);
ASDF_LOCAL int asdf_block_comp_open(asdf_block_t *block);
ASDF_LOCAL void asdf_block_comp_close(asdf_block_t *block);
