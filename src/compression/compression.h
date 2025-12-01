/** Internal compressed block utilities */
#pragma once

#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef HAVE_USERFAULTFD
#include <linux/userfaultfd.h>
#endif

#include <asdf/file.h>


typedef enum {
    ASDF_COMPRESSOR_UNINITIALIZED,
    ASDF_COMPRESSOR_INITIALIZED,
    ASDF_COMPRESSOR_IN_PROGRESS,
    ASDF_COMPRESSOR_DONE
} asdf_compressor_status_t;


typedef struct {
    asdf_compressor_status_t status;
    size_t optimal_chunk_size;
} asdf_compressor_info_t;


typedef void asdf_compressor_userdata_t;

typedef asdf_compressor_userdata_t *(*asdf_compressor_init_fn)(
    asdf_block_t *block, const void *dest, size_t dest_size);
typedef const asdf_compressor_info_t *(*asdf_compressor_info_fn)(
    asdf_compressor_userdata_t *userdata);
typedef int (*asdf_compressor_decomp_fn)(
    asdf_compressor_userdata_t *userdata,
    uint8_t *buf,
    size_t buf_size,
    size_t offset_hint,
    size_t *offset_out);
typedef void (*asdf_compressor_destroy_fn)(asdf_compressor_userdata_t *userdata);


typedef struct {
    /** Compression string from the block header */
    const char *compression;
    asdf_compressor_init_fn init;
    asdf_compressor_info_fn info;
    asdf_compressor_decomp_fn decomp;
    asdf_compressor_destroy_fn destroy;
} asdf_compressor_t;


// Forward-declaration
typedef struct _asdf_block_comp_state_t asdf_block_comp_state_t;

#ifdef HAVE_USERFAULTFD
/** Additional state for lazy decompression with userfaultfd */
typedef struct {
    asdf_block_comp_state_t *comp_state;
    /** File descriptor for the UUFD handle */
    int uffd;
    /** File descriptor for passing other events to the lazy decompression handler */
    int evtfd;
    /** Keep track of the range on which the UFFD was registered */
    struct uffdio_range range;
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
    asdf_block_decomp_mode_t mode;
    int fd;
    bool own_fd;
    uint8_t *dest;
    size_t dest_size;

    /**
     * Decompression scratch buffer
     * In eager decompression it is the same as the main dest buffer but it
     * can also be used for incremental work e.g. in lazy decompression mode
     */
    uint8_t *work_buf;
    size_t work_buf_size;

    const asdf_compressor_t *compressor;
    // Compressor-specific userdata
    asdf_compressor_userdata_t *userdata;

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


ASDF_LOCAL int asdf_block_comp_open(asdf_block_t *block);
ASDF_LOCAL void asdf_block_comp_close(asdf_block_t *block);
