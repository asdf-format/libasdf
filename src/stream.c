#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "context.h"
#include "error.h"
#include "stream.h"
#include "stream_intern.h"
#include "util.h"


/**
 * Helper for token scan methods
 *
 * Worth exposing publicly if we later want to make the stream interface available
 * to users to implement their own.
 *
 * NOTE: This is dog**** performance though; spend more time optimizing later, could use SIMD
 */
int asdf_scan_tokens(
    const uint8_t *buf,
    size_t len,
    const uint8_t **tokens,
    const size_t *token_lens,
    size_t n_tokens,
    size_t *match_offset,
    size_t *match_token_idx) {

    if (!buf || len == 0 || n_tokens == 0)
        return 1;

    size_t max_token_len = 0;

    for (size_t idx = 0; idx < n_tokens; idx++) {
        if (token_lens[idx] > max_token_len)
            max_token_len = token_lens[idx];
    }

    for (size_t idx = 0; idx < len; idx++) {
        for (size_t tdx = 0; tdx < n_tokens; tdx++) {
            size_t tok_len = token_lens[tdx];
            if (tok_len <= len - idx && memcmp(buf + idx, tokens[tdx], tok_len) == 0) {
                if (match_offset)
                    *match_offset = idx;

                if (match_token_idx)
                    *match_token_idx = tdx;

                return 0;
            }
        }
    }

    return 1;
}


static void stream_capture(asdf_stream_t *stream, const uint8_t *buf, size_t size) {
    if (LIKELY(!stream->capture_buf))
        return;

    uint8_t *capture_buf = *stream->capture_buf;

    if (!capture_buf)
        return;

    size_t needed = *stream->capture_size + size;

    if (needed > stream->capture_cap) {
        size_t new_cap = stream->capture_cap * 2;
        uint8_t *new_buf = realloc(capture_buf, new_cap);

        if (!new_buf) {
            ASDF_ERROR_OOM(stream);
            return;
        }

        capture_buf = new_buf;
        *stream->capture_buf = new_buf;
        stream->capture_cap = new_cap;
    }

    memcpy(capture_buf + *stream->capture_size, buf, size);
    *stream->capture_size += size;
}


void asdf_stream_set_capture(asdf_stream_t *stream, uint8_t **buf, size_t *size, size_t capacity) {
    if (!stream)
        return;

    stream->capture_buf = buf;
    stream->capture_cap = capacity;
    stream->capture_size = size;
}


/**
 * Allows seeking forward unseekable files but only with ``SEEK_CUR`` with a positive offset,
 * otherwise returns an error.
 *
 * For random access files it simply calls ``stream->seek``.
 */
int asdf_stream_seek(asdf_stream_t *stream, off_t offset, int whence) {
    if (UNLIKELY(!stream->is_seekable && offset < 0 && whence != SEEK_CUR)) {
        ASDF_ERROR_ERRNO(stream, EINVAL);
        return -1;
    }

    if (stream->is_seekable)
        return stream->seek(stream, offset, whence);

    /* Case for non-seekable streams; just read and consume up to offset bytes */
    /* Cast to size_t is safe since offset should never be non-zero in this branch */
    assert(offset >= 0);
    size_t to_consume = (size_t)offset;
    size_t avail = 0;

    while (to_consume > 0) {
        stream->next(stream, 1, &avail);

        if (ASDF_ERROR_GET(stream))
            return -1;

        if (avail == 0)
            break;

        size_t count = avail >= to_consume ? to_consume : avail;
        stream->consume(stream, count);
        to_consume -= count;
    }

    return 0;
}


/**
 * File-backed read handling
 */
static const uint8_t *file_next(asdf_stream_t *stream, size_t count, size_t *avail) {
    assert(stream);
    assert(avail);
    file_userdata_t *data = stream->userdata;
    size_t buf_remain = data->buf_avail - data->buf_pos;

    // Resize buffer if necessary; note if pass a large count request this can
    // resize effectively unbounded so use with caution
    if (count > data->buf_size) {
        uint8_t *new_buf = realloc(data->buf, count);
        if (!new_buf) {
            *avail = 0;
            ASDF_ERROR_OOM(stream);
            return NULL;
        }
        data->buf = new_buf;
        data->buf_size = count;
    }

    // If not enough data available, shift remaining data to start
    if (buf_remain < count) {
        memmove(data->buf, data->buf + data->buf_pos, buf_remain);
        data->buf_avail = buf_remain;
        data->buf_pos = 0;
    }

    if (buf_remain < count || data->buf_pos >= data->buf_avail) {
        size_t n = fread(data->buf + buf_remain, 1, data->buf_size - buf_remain, data->file);
        data->buf_avail += n;
        buf_remain = data->buf_avail - data->buf_pos;
    }

    *avail = buf_remain;
    return (buf_remain > 0) ? data->buf + data->buf_pos : NULL;
}


static void file_consume(asdf_stream_t *stream, size_t count) {
    assert(stream);
    file_userdata_t *data = stream->userdata;

    stream_capture(stream, data->buf + data->buf_pos, count);

    data->buf_pos += count;
    data->file_pos += count;

    if (data->buf_pos > data->buf_avail) {
        data->buf_pos = data->buf_avail;
    }
}


// The file-based readline only returns lines up to the file buffer size
// Anything longer than that is truncated, though it still advances the file
// to the end of the line (or EOF)
const uint8_t *file_readline(asdf_stream_t *stream, size_t *len) {
    file_userdata_t *data = stream->userdata;

    size_t avail = 0;
    const uint8_t *buf = file_next(stream, 0, &avail);

    if (!buf || avail == 0) {
        *len = 0;
        return NULL;
    }

    for (size_t idx = 0; idx < avail; idx++) {
        if (buf[idx] == '\n') {
            *len = idx + 1;
            file_consume(stream, idx + 1);
            return buf;
        }
    }

    // No newline found in current buffer
    // Truncate line at buffer end and discard remainder of line
    file_consume(stream, avail);

    int ch;
    while ((ch = fgetc(data->file)) != EOF) {
        if (ch == '\n')
            break;
    }

    *len = avail;
    return buf;
}


static int file_scan(
    struct asdf_stream *stream,
    const uint8_t **tokens,
    const size_t *token_lens,
    size_t n_tokens,
    size_t *match_offset,
    size_t *match_token_idx) {
    // File-based scan is a little trickier because tokens could straddle buffered pages of the
    // file, so we maintain a sliding window so that the last (max_token_len - 1) bytes are always
    // available at the beginning of the next read
    file_userdata_t *data = stream->userdata;
    size_t max_token_len = 0;
    size_t offset = 0;
    size_t token_idx = 0;

    for (size_t idx = 0; idx < n_tokens; idx++) {
        if (token_lens[idx] > max_token_len)
            max_token_len = token_lens[idx];
    }

    if (max_token_len == 0)
        return 1;

    while (true) {
        size_t avail = data->buf_avail - data->buf_pos;
        int res = 1;

        if (avail >= max_token_len)
            res = asdf_scan_tokens(
                data->buf + data->buf_pos,
                avail,
                tokens,
                token_lens,
                n_tokens,
                &offset,
                &token_idx);

        if (0 == res) {
            if (match_offset)
                *match_offset = data->file_pos + offset;

            if (match_token_idx)
                *match_token_idx = token_idx;

            file_consume(stream, offset);
            return res;
        }

        size_t preserve = (avail < max_token_len - 1) ? avail : max_token_len - 1;
        stream_capture(stream, data->buf, data->buf_avail - preserve);
        memmove(data->buf, data->buf + data->buf_avail - preserve, preserve);
        size_t n = fread(data->buf + preserve, 1, data->buf_size - preserve, data->file);
        size_t new_avail = preserve + n;
        data->file_pos += (data->buf_avail - preserve);
        data->buf_avail = new_avail;

        if (new_avail < max_token_len) {
            data->buf_pos = new_avail;
            return 1;
        }

        data->buf_pos = 0;
    }
}


static int file_seek(asdf_stream_t *stream, off_t offset, int whence) {
    // Should seek relative to where we've *consumed* up to
    file_userdata_t *data = stream->userdata;
    int ret = -1;

    if (SEEK_CUR == whence) {
        offset = data->file_pos + offset;
        ret = fseeko(data->file, offset, SEEK_SET);
    } else {
        ret = fseeko(data->file, offset, whence);
    }

    if (0 == ret) {
        // After a seek we need to reset the next buffer
        data->buf_avail = 0;
        data->buf_pos = 0;
        switch (whence) {
        case SEEK_SET:
        case SEEK_CUR:
            data->file_pos = offset;
            break;
        case SEEK_END:
            data->file_pos = ftello(data->file);
            break;
        default:
            // Should never get here
            assert(false);
        }
    }

    return ret;
}


static off_t file_tell(asdf_stream_t *stream) {
    file_userdata_t *data = stream->userdata;
    return data->file_pos;
}


static void file_close(asdf_stream_t *stream) {
    file_userdata_t *data = stream->userdata;

    if (data->should_close)
        fclose(data->file);

    free(data->buf);
    free(data);
    asdf_context_release(stream->base.ctx);
    free(stream);
}


static bool file_is_seekable(FILE *file) {
    off_t pos = ftello(file);

    if (pos == -1)
        return false;

    // Try seeking back to the same position to catch edge cases where ftello works
    // but seeking is still not possible.
    if (fseeko(file, pos, SEEK_SET) != 0)
        return false;

    return true;
}


static int file_fy_parser_set_input(asdf_stream_t *stream, struct fy_parser *fyp) {
    file_userdata_t *data = stream->userdata;

    // This should only be used if the file is seekable
    assert(file_is_seekable(data->file));

    // If some data is already in the buffer we may need to seek backwards
    // so the data we already buffered locally will be available to fyaml
    fseeko(data->file, -data->buf_avail + data->buf_pos, SEEK_CUR);
    return fy_parser_set_input_fp(fyp, data->filename, data->file);
}


asdf_stream_t *asdf_stream_from_fp(asdf_context_t *ctx, FILE *file, const char *filename) {
    if (!file)
        return NULL;

    file_userdata_t *data = calloc(1, sizeof(file_userdata_t));

    if (!data) {
        return NULL;
    }

    data->file = file;
    data->filename = filename;
    data->buf_size = BUFSIZ; // hard-coded for now, could make tuneable later
    data->buf = malloc(data->buf_size);

    if (!data->buf) {
        free(data);
        return NULL;
    }

    asdf_stream_t *stream = malloc(sizeof(asdf_stream_t));

    if (!stream) {
        free(data->buf);
        free(data);
        return NULL;
    }

    if (!ctx) {
        ctx = asdf_context_create();

        if (!ctx) {
            free(data->buf);
            free(data);
            free(stream);
            return NULL;
        }
    } else {
        // Share an existing reference to the context
        asdf_context_retain(ctx);
    }

    stream->base.ctx = ctx;
    stream->is_seekable = file_is_seekable(file);
    stream->userdata = data;
    stream->next = file_next;
    stream->consume = file_consume;
    stream->readline = file_readline;
    stream->scan = file_scan;
    stream->seek = file_seek;
    stream->tell = file_tell;
    stream->close = file_close;
    stream->fy_parser_set_input = file_fy_parser_set_input;
    asdf_stream_set_capture(stream, NULL, NULL, 0);

#if DEBUG
    stream->last_next_size = 0;
    stream->last_next_ptr = NULL;
    stream->unconsumed_next_count = 0;
#endif

    return stream;
}


asdf_stream_t *asdf_stream_from_file(asdf_context_t *ctx, const char *filename) {
    FILE *file = fopen(filename, "rb");

    if (!file)
        return NULL;

    asdf_stream_t *stream = asdf_stream_from_fp(ctx, file, filename);

    if (!stream) {
        fclose(file);
        return NULL;
    }

    file_userdata_t *data = stream->userdata;
    data->should_close = true;
    return stream;
}


/**
 * Memory-backed read handling
 */
static const uint8_t *mem_next(asdf_stream_t *stream, UNUSED(size_t count), size_t *avail) {
    assert(stream);
    assert(avail);
    mem_userdata_t *data = stream->userdata;
    if (data->pos >= data->size) {
        *avail = 0;
        return NULL;
    }

    *avail = data->size - data->pos;
    return data->buf + data->pos;
}


static void mem_consume(asdf_stream_t *stream, size_t n) {
    mem_userdata_t *data = stream->userdata;
    stream_capture(stream, data->buf + data->pos, n);
    data->pos += n;
}


static const uint8_t *mem_readline(asdf_stream_t *stream, size_t *len) {
    mem_userdata_t *data = stream->userdata;
    const uint8_t *buf = data->buf + data->pos;
    size_t remaining = data->size - data->pos;
    size_t idx = 0;

    if (UNLIKELY(remaining == 0)) {
        *len = 0;
        return NULL;
    }

    for (; idx < remaining; idx++) {
        if (buf[idx] == '\n')
            break;
    }

    mem_consume(stream, idx + 1);
    *len = idx + 1;
    return buf;
}


static int mem_scan(
    struct asdf_stream *stream,
    const uint8_t **tokens,
    const size_t *token_lens,
    size_t n_tokens,
    size_t *match_offset,
    size_t *match_token_idx) {
    // The mem_scan case is simple; we only need to wrap asdf_scan_tokens
    mem_userdata_t *data = stream->userdata;
    size_t offset = 0;
    size_t token_idx = 0;
    size_t avail = data->size - data->pos;
    int res = asdf_scan_tokens(
        data->buf + data->pos, avail, tokens, token_lens, n_tokens, &offset, &token_idx);

    if (0 == res) {
        if (match_offset)
            *match_offset = offset + data->pos;

        if (match_token_idx)
            *match_token_idx = token_idx;

        mem_consume(stream, offset);
    } else {
        // Scan exhausted the buffer without finding anything; the full buffer is thus consumed
        mem_consume(stream, avail);
    }

    return res;
}


static int mem_seek(asdf_stream_t *stream, off_t offset, int whence) {
    mem_userdata_t *data = stream->userdata;
    size_t new_pos;

    switch (whence) {
    case SEEK_SET:
        new_pos = offset;
        break;
    case SEEK_CUR:
        new_pos = data->pos + offset;
        break;
    case SEEK_END:
        new_pos = data->size + offset;
        break;
    default:
        return -1;
    }

    data->pos = new_pos > data->size ? data->size : new_pos;
    return 0;
}


static off_t mem_tell(asdf_stream_t *stream) {
    mem_userdata_t *data = stream->userdata;
    return (off_t)data->pos;
}


static void mem_close(asdf_stream_t *stream) {
    free(stream->userdata);
    asdf_context_release(stream->base.ctx);
    free(stream);
}


static int mem_fy_parser_set_input(asdf_stream_t *stream, struct fy_parser *fyp) {
    mem_userdata_t *data = stream->userdata;
    return fy_parser_set_string(fyp, (const char *)data->buf + data->pos, data->size);
}


// TODO: mmap opener
asdf_stream_t *asdf_stream_from_memory(asdf_context_t *ctx, const void *buf, size_t size) {
    mem_userdata_t *data = malloc(sizeof(mem_userdata_t));

    if (!data) {
        return NULL;
    }

    data->buf = buf;
    data->size = size;
    data->pos = 0;

    asdf_stream_t *stream = malloc(sizeof(asdf_stream_t));

    if (!stream) {
        free(data);
        return NULL;
    }

    if (!ctx) {
        ctx = asdf_context_create();

        if (!ctx) {
            free(data);
            free(stream);
            return NULL;
        }
    } else {
        // Share existing reference to the context
        asdf_context_retain(ctx);
    }

    stream->base.ctx = ctx;
    stream->is_seekable = true;
    stream->userdata = data;
    stream->next = mem_next;
    stream->consume = mem_consume;
    stream->readline = mem_readline;
    stream->scan = mem_scan;
    stream->seek = mem_seek;
    stream->tell = mem_tell;
    stream->close = mem_close;
    stream->fy_parser_set_input = mem_fy_parser_set_input;
    asdf_stream_set_capture(stream, NULL, NULL, 0);

#if DEBUG
    stream->last_next_size = 0;
    stream->last_next_ptr = NULL;
    stream->unconsumed_next_count = 0;
#endif

    return stream;
}
