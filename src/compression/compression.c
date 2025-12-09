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

#include "config.h"
#include <asdf/file.h>
#include <asdf/log.h>

#ifdef HAVE_USERFAULTFD
#include <fcntl.h>
#include <linux/userfaultfd.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/syscall.h>

#ifndef UFFD_USER_MODE_ONLY
#pragma message "warning: UFFD_USER_MODE_ONLY missing"
#define UFFD_USER_MODE_ONLY 0
#endif
#endif


#include "../block.h"
#include "../error.h"
#include "../file.h"
#include "../log.h"
#include "../util.h"
#include "compression.h"
#include "compressor_registry.h"


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


static int asdf_block_decomp_offset(
    asdf_block_comp_state_t *cs, size_t offset_hint, size_t *offset_out) {
    assert(cs);
    assert(cs->work_buf);
    assert(cs->compressor);
    assert(cs->userdata);
    return cs->compressor->decomp(
        cs->userdata, cs->work_buf, cs->work_buf_size, offset_hint, offset_out);
}


static int asdf_block_decomp_eager(asdf_block_comp_state_t *cs) {
    cs->work_buf = cs->dest;
    cs->work_buf_size = cs->dest_size;
    // Decompress the full range
    size_t offset = 0;
    int ret = asdf_block_decomp_offset(cs, 0, &offset);
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


static void asdf_block_decomp_lazy_shutdown(UNUSED(asdf_block_comp_state_t *cs)) {
}
#else
static void *asdf_block_comp_userfaultfd_handler(void *arg) {
    asdf_block_comp_userfaultfd_t *uffd = arg;
    asdf_block_comp_state_t *cs = uffd->comp_state;
    const asdf_compressor_info_t *info = NULL;
    struct uffd_msg msg;
    size_t page_size = sysconf(_SC_PAGE_SIZE);
    size_t page_mask = page_size - 1;

    struct pollfd fds[2] = {0};
    fds[0].fd = uffd->uffd;
    fds[0].events = POLLIN;
    fds[1].fd = uffd->evtfd;
    fds[1].events = POLLIN;

    struct pollfd *poll_uffd = &fds[0];
    struct pollfd *poll_evtfd = &fds[1];

    while (!atomic_load(&uffd->stop)) {
        info = cs->compressor->info(cs->userdata);
        if (!info || info->status == ASDF_COMPRESSOR_DONE)
            break;

        int nready = poll(fds, 2, -1);

        if (nready == -1) {
            if (errno == EINTR)
                continue;

            ASDF_ERROR_ERRNO(cs->file, errno);
            break;
        }

        if ((poll_evtfd->revents & POLLIN) != 0) {
            uint64_t val = 0;
            ssize_t n = read(uffd->evtfd, &val, sizeof(val));

            if (n <= 0) {
                if (errno == EINTR)
                    continue;
            }
            break;
        }

        if ((poll_uffd->revents & POLLIN) != 0) {
            ssize_t n = read(uffd->uffd, &msg, sizeof(msg));

            if (n <= 0) {
                if (errno == EINTR)
                    continue;

                break;
            }

            if (msg.event != UFFD_EVENT_PAGEFAULT)
                continue;
        } else {
            continue;
        }

        size_t fault_addr = msg.arg.pagefault.address & ~page_mask;
        size_t chunk_size = cs->work_buf_size;
        // Align to page offset
        size_t offset = (fault_addr - (uintptr_t)cs->dest) & ~page_mask;
        int ret = 0;

        while (!atomic_load(&uffd->stop)) {
            size_t got_offset = 0;
            memset(cs->work_buf, 0, chunk_size);
            ret = asdf_block_decomp_offset(cs, offset, &got_offset);

            // TODO: Better handling or at least logging of error in decompressor
            if (ret != 0)
                break;

            size_t dst = (uintptr_t)cs->dest + got_offset;
            size_t src = (size_t)uffd->work_buf;
            size_t len =
                ((got_offset + chunk_size) > cs->dest_size ? (cs->dest_size - got_offset)
                                                           : chunk_size);
            len = (len + page_size - 1) & ~page_mask;
            uint64_t mode = (got_offset == offset) ? 0 : UFFDIO_COPY_MODE_DONTWAKE;

            struct uffdio_copy uffd_copy = {
                // Copy back to the (page-aligned) destination in the user's mmap
                .dst = dst,
                // From our lazy decompression buffer
                .src = src,
                .len = len,
                .mode = mode};

            // TODO: Allow PROT_WRITE if we are running in updatable mode
            mprotect(cs->dest + got_offset, len, PROT_READ);

            ret = ioctl(uffd->uffd, UFFDIO_COPY, &uffd_copy);
            if (ret != 0) {
                // TODO: Error handling
                break;
            }

            if (got_offset == offset) {
                break;
            }
        }

        if (ret != 0) {
            break;
        }
    }

    // If the thread exits for any reason make sure to deregister UFFD for
    // for the range handled; of course if the thread crashes we're in deep
    // water (main thread may hang)
    if (ioctl(uffd->uffd, UFFDIO_UNREGISTER, &uffd->range) == -1) {
        ASDF_ERROR_ERRNO(cs->file, errno);
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

    if (!FEATURE_IS_SET(uffd_api.ioctls, _UFFDIO_REGISTER)) {
        ASDF_LOG(
            cs->file,
            ASDF_LOG_DEBUG,
            "UFFDIO_REGISTER ioctl is not supported: lazy decompression with userfaultfd not "
            "available");
        close(uffd);
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

    close(uffd);
    return true;
}


#define ASDF_BLOCK_DECOMP_DEFAULT_PAGES_PER_CHUNK 1024


static int asdf_block_decomp_lazy(asdf_block_comp_state_t *cs) {
    asdf_block_comp_userfaultfd_t *uffd = calloc(1, sizeof(asdf_block_comp_userfaultfd_t));

    if (!uffd) {
        ASDF_ERROR_OOM(cs->file);
        return -1;
    }

    uffd->comp_state = cs;
    cs->lazy.userfaultfd = uffd;

    // Determine the chunk size--if not specified in the settings set to _SC_PAGESIZE,
    // but otherwise align to a multiple of page size
    size_t page_size = sysconf(_SC_PAGESIZE);
    size_t chunk_size = page_size * ASDF_BLOCK_DECOMP_DEFAULT_PAGES_PER_CHUNK;

    // Check the compressor info in case it reports an optimal chunk size preferred by
    // the compressor
    const asdf_compressor_info_t *info = cs->compressor->info(cs->userdata);

    if (info->optimal_chunk_size > 0)
        chunk_size = info->optimal_chunk_size;

    if (cs->file->config->decomp.chunk_size > 0)
        chunk_size = cs->file->config->decomp.chunk_size;

    chunk_size = (chunk_size + page_size - 1) & ~(page_size - 1);
    ASDF_LOG(cs->file, ASDF_LOG_DEBUG, "lazy decompression chunk size: %ld", chunk_size);

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
    uffd->range.start = (uintptr_t)cs->dest;
    uffd->range.len = range_len;
    struct uffdio_register uffd_reg = {.range = uffd->range, .mode = UFFDIO_REGISTER_MODE_MISSING};

    if (ioctl(uffd->uffd, UFFDIO_REGISTER, &uffd_reg) == -1) {
        ASDF_LOG(
            cs->file, ASDF_LOG_ERROR, "failed registering memory range for userfaultfd handling");
        ASDF_ERROR_ERRNO(cs->file, errno);
        return -1;
    }

    // Create the eventfd for signalling
    uffd->evtfd = eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);

    if (uffd->evtfd < 0) {
        ASDF_ERROR_ERRNO(cs->file, errno);
        return -1;
    }

    // Spawn handler thread
    pthread_create(&uffd->handler_thread, NULL, asdf_block_comp_userfaultfd_handler, uffd);

    return 0;
}


static void asdf_block_decomp_lazy_shutdown(asdf_block_comp_state_t *cs) {
    if (!cs)
        return;

    asdf_block_comp_userfaultfd_t *uffd = cs->lazy.userfaultfd;

    if (!uffd)
        return;

    // Set the stop flag then signal the thread to stop
    atomic_store(&uffd->stop, true);
    uint64_t one = 1;
    ssize_t n = write(uffd->evtfd, &one, sizeof(one));

    if (n < 0) {
        ASDF_LOG(
            cs->file,
            ASDF_LOG_ERROR,
            "failed to write the shutdown event to the lazy decompression handler: %s",
            strerror(errno));
        ASDF_ERROR_ERRNO(cs->file, errno);
    }

    pthread_join(uffd->handler_thread, NULL);
    close(uffd->uffd);
    close(uffd->evtfd);
    free(uffd->work_buf);
    free(uffd);
}
#endif /* HAVE_USERFAULTFD */


void asdf_block_comp_close(asdf_block_t *block) {
    assert(block);
    asdf_block_comp_state_t *cs = block->comp_state;

    if (!cs)
        return;

    if (cs->mode == ASDF_BLOCK_DECOMP_MODE_LAZY)
        asdf_block_decomp_lazy_shutdown(cs);

    if (cs->compressor)
        cs->compressor->destroy(cs->userdata);

    if (cs->dest)
        munmap(cs->dest, cs->dest_size);

    if (cs->own_fd)
        close(cs->fd);

    ZERO_MEMORY(cs, sizeof(asdf_block_comp_state_t));
    free(cs);
}


/**
 * Opens a memory handle to contain decompressed block data
 *
 * TODO: Improve error handling here
 * TODO: Clean up cognitive complexity of this function
 */
int asdf_block_comp_open(asdf_block_t *block) {
    assert(block);

    int ret = -1;

    const char *compression = asdf_block_compression(block);

    if (strlen(compression) == 0) {
        // Actually nothing to do, just return 0
        return 0;
    }

    const asdf_compressor_t *comp = asdf_compressor_get(block->file, compression);

    if (!comp) {
        ASDF_LOG(
            block->file,
            ASDF_LOG_ERROR,
            "no compressor extension found for %s compression",
            compression);
        return ret;
    }

    asdf_block_comp_state_t *state = calloc(1, sizeof(asdf_block_comp_state_t));

    if (!state) {
        ASDF_ERROR_OOM(block->file);
        goto failure;
    }

    state->file = block->file;
    state->compressor = comp;


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
        } else if (!use_lazy_mode && mode == ASDF_BLOCK_DECOMP_MODE_LAZY) {
            ASDF_LOG(
                block->file,
                ASDF_LOG_WARN,
                "lazy decompression mode requested, but the runtime check for kernel "
                "support failed");
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
    state->userdata = state->compressor->init(block, state->dest, state->dest_size);
    state->mode = mode; // Store the effective mode we're running under

    if (!state->userdata) {
        ASDF_LOG(
            block->file,
            ASDF_LOG_ERROR,
            "failed to initialize compressor for %s compression",
            compression);
        goto failure;
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
