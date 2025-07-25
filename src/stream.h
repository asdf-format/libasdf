#pragma once

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>

#include <libfyaml.h>

#include "context.h"
#include "util.h"


// TODO: Document this once things shake out
typedef struct asdf_stream {
    asdf_base_t base;
    bool is_seekable;

    void *userdata;

    /* Optional stream capture buffer */
    uint8_t **capture_buf;
    size_t *capture_size;
    size_t capture_cap;

    const uint8_t *(*next)(struct asdf_stream *stream, size_t count, size_t *avail);
    void (*consume)(struct asdf_stream *stream, size_t count);
    const uint8_t *(*readline)(struct asdf_stream *stream, size_t *len);
    int (*scan)(
        struct asdf_stream *stream,
        const uint8_t **tokens,
        const size_t *token_lens,
        size_t n_tokens,
        size_t *match_offset,
        size_t *match_token_idx);
    int (*seek)(struct asdf_stream *stream, off_t offset, int whence);
    off_t (*tell)(struct asdf_stream *stream);
    void *(*open_mem)(struct asdf_stream *stream, off_t offset, size_t size, size_t *avail);
    int (*close_mem)(struct asdf_stream *stream, void *addr);
    void (*close)(struct asdf_stream *stream);
    int (*fy_parser_set_input)(struct asdf_stream *stream, struct fy_parser *fyp);

#if DEBUG
    // Some extra debugging helpers to catch stream->next() without stream->consume()
    size_t last_next_size;
    const uint8_t *last_next_ptr;
    int unconsumed_next_count;
#endif /* DEBUG */
} asdf_stream_t;


static inline const uint8_t *asdf_stream_next(asdf_stream_t *stream, size_t count, size_t *avail) {
#if DEBUG
    const uint8_t *r = stream->next(stream, count, avail);
    if (stream->last_next_ptr == r) {
        stream->unconsumed_next_count++;

        if (stream->unconsumed_next_count > 2) {
            // It's ok to call next() once (to peek) without consuming, but peeking multiple
            // times at the same position indicates a likely bug
            fprintf(
                stderr,
                "warning: calling stream->next() without consuming previous buffer"
                "(%zu bytes at %p)\n",
                stream->last_next_size,
                stream->last_next_ptr);
        }
    } else {
        stream->unconsumed_next_count = 1;
        stream->last_next_size = *avail;
        stream->last_next_ptr = r;
    }
    return r;
#else
    return stream->next(stream, count, avail);
#endif
}


static inline void asdf_stream_consume(asdf_stream_t *stream, size_t count) {
    stream->consume(stream, count);
#if DEBUG
    stream->unconsumed_next_count = 0;
    stream->last_next_ptr = NULL;
#endif
}


static inline const uint8_t *asdf_stream_readline(asdf_stream_t *stream, size_t *len) {
    return stream->readline(stream, len);
}


static inline int asdf_stream_scan(
    asdf_stream_t *stream,
    const uint8_t **tokens,
    const size_t *token_lens,
    size_t n_tokens,
    size_t *match_offset,
    size_t *match_token_idx) {
    return stream->scan(stream, tokens, token_lens, n_tokens, match_offset, match_token_idx);
}


static inline off_t asdf_stream_tell(asdf_stream_t *stream) {
    return stream->tell(stream);
}


static inline void asdf_stream_close(asdf_stream_t *stream) {
    return stream->close(stream);
}


ASDF_LOCAL int asdf_stream_seek(asdf_stream_t *stream, off_t offset, int whence);

ASDF_LOCAL asdf_stream_t *asdf_stream_from_file(asdf_context_t *ctx, const char *filename);
ASDF_LOCAL asdf_stream_t *asdf_stream_from_fp(
    asdf_context_t *ctx, FILE *file, const char *filename);
ASDF_LOCAL asdf_stream_t *asdf_stream_from_memory(
    asdf_context_t *ctx, const void *buf, size_t size);

ASDF_LOCAL void asdf_stream_set_capture(
    asdf_stream_t *stream, uint8_t **buf, size_t *size, size_t capacity);

ASDF_LOCAL int asdf_scan_tokens(
    const uint8_t *buf,
    size_t len,
    const uint8_t **tokens,
    const size_t *token_lens,
    size_t n_tokens,
    size_t *match_offset,
    size_t *match_token_idx);
