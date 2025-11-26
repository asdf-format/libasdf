/**
 * Internal utilities specifically for handling compressed blocks
 */
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <bzlib.h>
#include <zlib.h>

#include "config.h"
#include <asdf/file.h>
#include <asdf/log.h>

#ifdef HAVE_USERFAULTFD
#include <fcntl.h>
#include <linux/userfaultfd.h>
#include <pthread.h> // TODO: Should have configure-time check
#include <sys/ioctl.h>
#include <sys/syscall.h>
#endif


#include "../block.h"
#include "../error.h"
#include "../file.h"
#include "../log.h"
#include "../util.h"
#include "compression.h"


void asdf_block_comp_close(asdf_block_t *block) {
    assert(block);
    asdf_block_comp_state_t *cs = block->comp_state;

    if (!cs)
        return;

    switch (cs->comp) {
    case ASDF_BLOCK_COMP_UNKNOWN:
    case ASDF_BLOCK_COMP_NONE:
        /* Nothing to do */
        break;
    case ASDF_BLOCK_COMP_ZLIB:
        if (cs->z) {
            inflateEnd(cs->z);
            free(cs->z);
        }
        break;
    case ASDF_BLOCK_COMP_BZP2:
        if (cs->bz) {
            BZ2_bzDecompressEnd(cs->bz);
            free(cs->bz);
        }
        break;
    }

    if (cs->dest)
        munmap(cs->dest, cs->dest_size);

    if (cs->own_fd)
        close(cs->fd);

        // TODO: Maybe extract out any necessary cleanup for specific lazy
        // decompression implementations; currently there is only the one though
#ifdef HAVE_USERFAULTFD
    if (cs->lazy.userfaultfd) {
        atomic_store(&cs->lazy.userfaultfd->stop, true);
        close(cs->lazy.userfaultfd->uffd);
        pthread_join(cs->lazy.userfaultfd->handler_thread, NULL);
        free(cs->lazy.userfaultfd->work_buf);
        free(cs->lazy.userfaultfd);
    }
#endif

    ZERO_MEMORY(cs, sizeof(asdf_block_comp_state_t));
    free(cs);
}


asdf_block_comp_t asdf_block_comp_parse(asdf_file_t *file, const char *compression) {
    assert(compression);

    if (strncmp(compression, "zlib", ASDF_BLOCK_COMPRESSION_FIELD_SIZE) == 0)
        return ASDF_BLOCK_COMP_ZLIB;

    if (strncmp(compression, "bzp2", ASDF_BLOCK_COMPRESSION_FIELD_SIZE) == 0)
        return ASDF_BLOCK_COMP_BZP2;

    if (compression[0] == '\0')
        return ASDF_BLOCK_COMP_NONE;

    ASDF_LOG(
        file,
        ASDF_LOG_WARN,
        "unsupported block compression option %s; block data will simply be copied verbatim",
        compression);
    return ASDF_BLOCK_COMP_UNKNOWN;
}


static int asdf_create_temp_file(size_t data_size, const char *tmp_dir, int *out_fd) {
    char path[PATH_MAX];
    int fd;

    if (!tmp_dir) {
        const char *tmp = getenv("ASDF_TMPDIR");
        tmp = (tmp && tmp[0]) ? tmp : getenv("TMPDIR");
        tmp_dir = (tmp && tmp[0]) ? tmp : "/tmp";
    }

    snprintf(path, sizeof(path), "%s/libasdf-block-XXXXXX", tmp_dir);

    fd = mkstemp(path);

    if (fd < 0)
        return -1;

    // unlink so it deletes on close
    unlink(path);

    if (ftruncate(fd, data_size) != 0) {
        close(fd);
        return -1;
    }

    *out_fd = fd;
    return 0;
}


static int asdf_block_decomp_next(asdf_block_comp_state_t *cs) {
    assert(cs);
    assert(cs->work_buf);
    size_t new_produced = cs->dest_size;

    // TODO: Probably offload this as well as per-compression-type
    // initialization to a separate, extensible interface
    switch (cs->comp) {
    case ASDF_BLOCK_COMP_UNKNOWN:
    case ASDF_BLOCK_COMP_NONE:
        return -1;
    case ASDF_BLOCK_COMP_ZLIB: {
        cs->z->next_out = cs->work_buf;
        cs->z->avail_out = cs->work_buf_size;

        int ret = inflate(cs->z, Z_NO_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END)
            return ret;

        new_produced -= cs->z->avail_out;
        break;
    }
    case ASDF_BLOCK_COMP_BZP2: {
        cs->bz->next_out = (char *)cs->work_buf;
        cs->bz->avail_out = cs->work_buf_size;

        int ret = BZ2_bzDecompress(cs->bz);
        if (ret != BZ_OK && ret != BZ_STREAM_END)
            return ret;

        new_produced -= cs->bz->avail_out;
        break;
    }
    }

    cs->produced = new_produced;
    return 0;
}


static int asdf_block_decomp_eager(asdf_block_comp_state_t *cs) {
    cs->work_buf = cs->dest;
    cs->work_buf_size = cs->dest_size;
    // Decompress the full range
    int ret = asdf_block_decomp_next(cs);
    // After decompression set PROT_READ for now (later this should depend on the mode flag the
    // file was opened with)
    mprotect(cs->dest, cs->dest_size, PROT_READ);
    return ret;
}

/** Optional lazy decompression implementation(s) -- maybe move to another file ?*/

#if !(defined(HAVE_USERFAULTFD) && defined(HAVE_DECL_SYS_USERFAULTFD))
static int asdf_block_decomp_lazy(asdf_block_comp_state_t *state) {
    ASDF_LOG(
        state->file,
        ASDF_LOG_ERROR,
        "lazy decompression is not available on this system, and this code path "
        "should not have been reached");
    return -1;
}


static bool asdf_block_decomp_lazy_available(
    UNUSED(asdf_block_comp_state_t *cs), UNUSED(bool use_file_backing)) {
    return false;
}
#else
static void *asdf_block_comp_userfaultfd_handler(void *arg) {
    asdf_block_comp_userfaultfd_t *uffd = arg;
    asdf_block_comp_state_t *cs = uffd->comp_state;
    struct uffd_msg msg;

    while (!atomic_load(&uffd->stop)) {
        if (cs->produced >= cs->dest_size)
            break;

        ssize_t n = read(uffd->uffd, &msg, sizeof(msg));

        if (n <= 0) {
            if (errno == EINTR)
                continue;

            break;
        }

        if (msg.event != UFFD_EVENT_PAGEFAULT)
            continue;

        size_t fault_addr = msg.arg.pagefault.address;
        size_t chunk_size = cs->work_buf_size;

        // Align to page offset
        size_t offset = (fault_addr - (uintptr_t)cs->dest) & ~(chunk_size - 1);
        memset(cs->work_buf, 0, chunk_size);
        int ret = asdf_block_decomp_next(cs);

        if (ret != 0) {
            // TODO: Error handling
            break;
        }

        // TODO: Allow PROT_WRITE if we are running in updatable mode
        mprotect(cs->dest + offset, chunk_size, PROT_READ);

        struct uffdio_copy uffd_copy = {
            // Copy back to the (page-aligned) destination in the user's mmap
            .dst = fault_addr & ~(chunk_size - 1),
            // From our lazy decompression buffer
            .src = (uint64_t)uffd->work_buf,
            .len = chunk_size,
            .mode = 0};
        ioctl(uffd->uffd, UFFDIO_COPY, &uffd_copy);
    }

    return NULL;
}


#define FEATURE_IS_SET(bits, bit) (((bits) & (bit)) == (bit))

/**
 * Test whether lazy decompression is actually possible.
 *
 * In practice this probably wastes a lot of time since we repeat many of the
 * same operations then to set up lazy decompression.  We can probably do this
 * just once per library initialization.
 */
static bool asdf_block_decomp_lazy_available(asdf_block_comp_state_t *cs, bool use_file_backing) {
    // Probe UFFD features
    // TODO: I Think we only need to do this once, we need to make runtime checks for what's
    // actually supported anyways before enabling this feature
    int uffd = syscall(SYS_userfaultfd, O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY);

    if (uffd < 0) {
        ASDF_LOG(
            cs->file,
            ASDF_LOG_DEBUG,
            "userfaultfd syscall failed: %s; lazy decompression with userfaultfd not "
            "available",
            strerror(errno));
        return false;
    }

    struct uffdio_api uffd_api = {.api = UFFD_API};
    if (ioctl(uffd, UFFDIO_API, &uffd_api) == -1) {
        ASDF_LOG(
            cs->file,
            ASDF_LOG_DEBUG,
            "UFFDIO_API ioctl failed: %s; lazy decompression with userfaultfd not "
            "available",
            strerror(errno));
        close(uffd);
        return false;
    }

    close(uffd);

    if (!FEATURE_IS_SET(uffd_api.ioctls, _UFFDIO_REGISTER)) {
        ASDF_LOG(
            cs->file,
            ASDF_LOG_DEBUG,
            "UFFDIO_REGISTER ioctl is not supported: lazy decompression with userfaultfd not "
            "available");
        return false;
    }

    if (use_file_backing) {
        /* This is really only possible if
         *
         * - the kernel has UFFD_FEATURE_MISSING_SHMEM
         * - the location on the filesystem we're writing to is actually
         *   shared memory and not a real disk-backed file, an unfortunate
         *   limitation that somewhat makes file-backing less useful, but the
         *   only the only way to test that is to try to make a temp file, map
         *   it, and register it
         */
        size_t page_size = sysconf(_SC_PAGESIZE);
        asdf_config_t *config = cs->file->config;
        int fd = -1;
        if (asdf_create_temp_file(page_size, config->decomp.tmp_dir, &fd) != 0) {
            // Could not even create temp file so I guess false
            ASDF_ERROR_ERRNO(cs->file, errno);
            return false;
        }

        void *map = mmap(NULL, page_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

        // Test the range ioctl on the mapping
        struct uffdio_register uffd_reg = {
            .range = {.start = (uintptr_t)map, .len = page_size},
            .mode = UFFDIO_REGISTER_MODE_MISSING};
        if (ioctl(uffd, UFFDIO_REGISTER, &uffd_reg) == -1) {
            ASDF_LOG(
                cs->file,
                ASDF_LOG_WARN,
                "failed registering memory range for userfaultfd handling; file-backed lazy "
                "decompression not possible");
            munmap(map, page_size);
            return false;
        }
        munmap(map, page_size);
    }

    return true;
}


static int asdf_block_decomp_lazy(asdf_block_comp_state_t *cs) {
    asdf_block_comp_userfaultfd_t *uffd = calloc(1, sizeof(asdf_block_comp_userfaultfd_t));

    if (!uffd) {
        ASDF_ERROR_OOM(cs->file);
        return -1;
    }

    uffd->comp_state = cs;

    // Determine the chunk size--if not specified in the settings set to _SC_PAGESIZE,
    // but otherwise align to a multiple of page size
    size_t page_size = sysconf(_SC_PAGESIZE);
    size_t chunk_size = cs->file->config->decomp.chunk_size;

    if (chunk_size != 0)
        chunk_size = (chunk_size + page_size - 1) & ~(page_size - 1);
    else
        chunk_size = page_size;

    uffd->work_buf = aligned_alloc(page_size, chunk_size);

    if (!uffd->work_buf) {
        if (errno == ENOMEM)
            ASDF_ERROR_OOM(cs->file);
        else
            ASDF_ERROR_ERRNO(cs->file, errno);
        return -1;
    }

    uffd->work_buf_size = chunk_size;
    cs->work_buf = uffd->work_buf;
    cs->work_buf_size = uffd->work_buf_size;


    // Create userfaultfd
    uffd->uffd = syscall(SYS_userfaultfd, O_CLOEXEC | O_NONBLOCK | UFFD_USER_MODE_ONLY);
    if (uffd->uffd < 0) {
        ASDF_ERROR_ERRNO(cs->file, errno);
        return -1;
    }

    struct uffdio_api uffd_api = {
        .api = UFFD_API,
    };
    if (ioctl(uffd->uffd, UFFDIO_API, &uffd_api) == -1) {
        ASDF_ERROR_ERRNO(cs->file, errno);
        return -1;
    }

    // Register the memory range
    // Per careful reading of the man page, the length of the range must also be page-aligned
    size_t range_len = (cs->dest_size + page_size - 1) & ~(page_size - 1);
    struct uffdio_register uffd_reg = {
        .range = {.start = (uintptr_t)cs->dest, .len = range_len},
        .mode = UFFDIO_REGISTER_MODE_MISSING};
    if (ioctl(uffd->uffd, UFFDIO_REGISTER, &uffd_reg) == -1) {
        ASDF_LOG(
            cs->file, ASDF_LOG_ERROR, "failed registering memory range for userfaultfd handling");
        ASDF_ERROR_ERRNO(cs->file, errno);
        return -1;
    }

    // Spawn handler thread
    pthread_create(&uffd->handler_thread, NULL, asdf_block_comp_userfaultfd_handler, uffd);

    return 0;
}
#endif /* HAVE_USERFAULTFD */


#define ASDF_ZLIB_FORMAT 15
#define ASDF_ZLIB_AUTODETECT 32


/**
 * Opens a memory handle to contain decompressed block data
 *
 * TODO: Improve error handling here
 * TODO: Clean up cognitive complexity of this function
 */
int asdf_block_comp_open(asdf_block_t *block) {
    assert(block);

    int ret = -1;

    if (block->comp == ASDF_BLOCK_COMP_UNKNOWN || block->comp == ASDF_BLOCK_COMP_NONE) {
        // Actually nothing to do, just return 0
        return 0;
    }

    asdf_block_comp_state_t *state = calloc(1, sizeof(asdf_block_comp_state_t));

    if (!state) {
        ASDF_ERROR_OOM(block->file);
        goto failure;
    }

    state->file = block->file;
    state->comp = block->comp;

    asdf_config_t *config = block->file->config;
    asdf_block_header_t *header = &block->info.header;
    asdf_block_decomp_mode_t mode = config->decomp.mode;
    // Decide whether to use temp file or anonymous mmap
    size_t max_memory_bytes = config->decomp.max_memory_bytes;
    double max_memory_threshold = config->decomp.max_memory_threshold;

    size_t max_memory = SIZE_MAX;

    if (max_memory_threshold > 0.0) {
        size_t total_memory = asdf_util_get_total_memory();

        if (total_memory > 0)
            max_memory = (size_t)(total_memory * max_memory_threshold);
    }

    if (max_memory_bytes > 0)
        max_memory = (max_memory_bytes < max_memory) ? max_memory_bytes : max_memory;

    size_t dest_size = header->data_size;

    bool use_file_backing = dest_size > max_memory;

    if (use_file_backing)
        ASDF_LOG(
            block->file,
            ASDF_LOG_DEBUG,
            "compressed data in block %d is %d bytes, exceeding the memory threshold %d; "
            "data will be decompressed to a temp file",
            block->info.index,
            dest_size,
            max_memory);


    // Determine if we can use lazy decompression mode
    bool use_lazy_mode = false;
    if (mode == ASDF_BLOCK_DECOMP_MODE_AUTO || mode == ASDF_BLOCK_DECOMP_MODE_LAZY) {
        use_lazy_mode = asdf_block_decomp_lazy_available(state, use_file_backing);

        if (use_lazy_mode && use_file_backing) {
            if (mode == ASDF_BLOCK_DECOMP_MODE_AUTO) {
                ASDF_LOG(
                    block->file,
                    ASDF_LOG_DEBUG,
                    "using eager decompression mode, since lazy mode is not possible when "
                    "decompressing to a temp file on disk");
                use_lazy_mode = false;
                mode = ASDF_BLOCK_DECOMP_MODE_EAGER;
            } else {
                // If the user explicitly requested lazy mode, disable file backing instead
                ASDF_LOG(
                    block->file,
                    ASDF_LOG_WARN,
                    "using eager decompression mode, since lazy mode is not possible when "
                    "decompressing to a temp file on disk");
                use_file_backing = false;
                mode = ASDF_BLOCK_DECOMP_MODE_LAZY;
            }
        } else if (use_lazy_mode && mode == ASDF_BLOCK_DECOMP_MODE_LAZY) {
            ASDF_LOG(
                block->file,
                ASDF_LOG_WARN,
                "lazy decompression mode requested, but the runtime check for kernel "
                "support failed");
            use_lazy_mode = false;
            mode = ASDF_BLOCK_DECOMP_MODE_EAGER;
        }
    }

    if (use_file_backing && !use_lazy_mode) {
        if (asdf_create_temp_file(dest_size, config->decomp.tmp_dir, &state->fd) != 0) {
            goto failure;
        }
        // Read-only for now
        state->dest = mmap(NULL, dest_size, PROT_READ | PROT_WRITE, MAP_SHARED, state->fd, 0);
        if (state->dest == MAP_FAILED) {
            close(state->fd);
            goto failure;
        } else {
            state->own_fd = true;
        }
    } else {
        // anonymous mmap
        // Read-only for now
        state->dest = mmap(
            NULL, dest_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

        if (state->dest == MAP_FAILED) {
            goto failure;
        }

        state->fd = -1;
        state->own_fd = false;
    }

    state->dest_size = dest_size;

    // Initialize compression lib-specific structures
    switch (state->comp) {
    case ASDF_BLOCK_COMP_UNKNOWN:
    case ASDF_BLOCK_COMP_NONE:
        /* shouldn't even be here */
        UNREACHABLE();
    case ASDF_BLOCK_COMP_ZLIB: {
        z_stream *z = calloc(1, sizeof(z_stream));
        if (!z) {
            ASDF_ERROR_OOM(block->file);
            goto failure;
        }
        z->next_in = (Bytef *)block->data;
        z->avail_in = block->avail_size;
        z->next_out = (Bytef *)state->dest;
        z->avail_out = state->dest_size;
        state->z = z;

        ret = inflateInit2(z, ASDF_ZLIB_FORMAT + ASDF_ZLIB_AUTODETECT);

        if (ret != Z_OK)
            goto failure;

        break;
    }
    case ASDF_BLOCK_COMP_BZP2: {
        bz_stream *bz = calloc(1, sizeof(bz_stream));

        if (!bz) {
            ASDF_ERROR_OOM(block->file);
            goto failure;
        }

        bz->next_in = (char *)block->data;
        bz->avail_in = block->avail_size;
        bz->next_out = (char *)state->dest;
        bz->avail_out = state->dest_size;
        state->bz = bz;

        ret = BZ2_bzDecompressInit(bz, 0, 0);
        if (ret != BZ_OK)
            goto failure;

        break;
    }
    }

    if (use_lazy_mode)
        ret = asdf_block_decomp_lazy(state);
    else
        ret = asdf_block_decomp_eager(state);

    if (ret != 0)
        goto failure;

    block->comp_state = state;
    return 0;
failure:
    asdf_block_comp_close(block);
    return ret;
}
