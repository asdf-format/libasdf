/**
 * .. _asdf/block.h:
 *
 * Low-level APIs for working with ASDF binary blocks directly.
 *
 * More commonly you will use the ``core/ndarray`` suite of APIs for accessing
 * the block data associated with a ``core/ndarray``.  The functions here
 * provide lower-level access to blocks: enumerating and reading existing
 * blocks, and building and appending new ones when writing a file.
 */

#ifndef ASDF_BLOCK_H
#define ASDF_BLOCK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <asdf/util.h>

ASDF_BEGIN_DECLS

/*
 * asdf_file_t is defined in <asdf/file.h>; forward-declared here so that
 * <asdf/block.h> can be included on its own without a circular dependency.
 */
typedef struct asdf_file asdf_file_t;


/**
 * Opaque struct type representing information about an ASDF binary block
 *
 * Many of the block-related APIs work on `asdf_block_t *` handles.
 */
typedef struct asdf_block asdf_block_t;


/**
 * Return the total number of binary blocks in the ASDF file
 *
 * :param file: The `asdf_file_t *` for the file
 * :return: Number of blocks in the file as a `size_t`
 */
ASDF_EXPORT size_t asdf_block_count(asdf_file_t *file);

/**
 * Open a block for reading the raw bytes from it
 *
 * This needs to be called before using `asdf_block_data` and should have a
 * complementary `asdf_block_close` when done.  When the file is read from
 * disk this sets up a memory map to the block data.
 *
 * :param file: The `asdf_file_t *` for the file
 * :param index: The index of the block starting from 0
 * :return: An `asdf_block_t *` handle representing the block
 */
ASDF_EXPORT asdf_block_t *asdf_block_open(asdf_file_t *file, size_t index);

/**
 * Close an open `asdf_block_t *` handle
 *
 * After calling this any previous pointers to the block data are invalid.
 *
 * :param block: The `asdf_block_t *` handle
 */
ASDF_EXPORT void asdf_block_close(asdf_block_t *block);


/**
 * Create a new binary block not attached to any file
 *
 * The returned handle is *detached* and not associated with any file: it owns
 * its own state (including any data buffer) and only becomes part of a file's
 * block list, and bound to that file, when passed to `asdf_block_append` (which
 * also enforces that the file is open for writing).
 *
 * ``data``/``size`` set the block's initial (uncompressed) data:
 *
 * - If ``data`` is non-``NULL`` it is *borrowed* (as by `asdf_block_data_set`)
 *   and must remain valid until the file is written.
 * - If ``data`` is ``NULL`` and ``size`` is non-zero, an owned buffer of
 *   ``size`` bytes is allocated for the caller to fill; retrieve the writable
 *   pointer with `asdf_block_data_alloc` (which returns that same buffer).
 * - If ``data`` is ``NULL`` and ``size`` is 0, the block starts empty.
 *
 * Further configure it with `asdf_block_data_set` /
 * `asdf_block_data_set_compressed`, `asdf_block_compression_set` and
 * `asdf_block_allocated_size_set`, then either `asdf_block_append` it (ownership
 * transfers to the file) or discard it with `asdf_block_destroy`.
 *
 * :param data: Initial (uncompressed) data to borrow, or ``NULL``.
 * :param size: Size in bytes of ``data``, or the buffer size to allocate when
 *   ``data`` is ``NULL``.
 * :return: A new `asdf_block_t *` handle, or ``NULL`` on failure.
 */
ASDF_EXPORT asdf_block_t *asdf_block_create(const void *data, size_t size);

/**
 * Destroy a block created by `asdf_block_create` that was never appended
 *
 * Frees the handle and any data buffer it owns.  Do not use this on a handle
 * returned by `asdf_block_open`, or one that has already been appended to a
 * file.  Use `asdf_block_close` for those (the file owns an appended block's
 * data).
 *
 * :param block: The detached `asdf_block_t *` handle.
 */
ASDF_EXPORT void asdf_block_destroy(asdf_block_t *block);

/**
 * Allocate a new writable data buffer of ``size`` bytes owned by the block
 *
 * This is for *building* a block's data: the caller fills the returned buffer
 * with the block's uncompressed data.  The block owns the buffer; it is freed
 * when the file is closed (for an appended block) or by `asdf_block_destroy`
 * (for a detached one).  If a compressor has been set with
 * `asdf_block_compression_set`, the data is compressed when the file is
 * written.
 *
 * If the block already owns a buffer of exactly ``size`` bytes (e.g. one
 * allocated by ``asdf_block_create(file, NULL, size)``) the existing buffer is
 * returned rather than reallocating.  To *read* a block's data, use
 * `asdf_block_data` / `asdf_block_data_raw` instead.
 *
 * :param block: The `asdf_block_t *` handle.
 * :param size: The uncompressed size in bytes to allocate.
 * :return: A writable pointer to the buffer, or ``NULL`` on failure.
 */
ASDF_EXPORT void *asdf_block_data_alloc(asdf_block_t *block, size_t size);

/**
 * Set the block's uncompressed data to a caller-owned buffer.
 *
 * The buffer is *borrowed*: it is not copied and must remain valid until the
 * file is written.  If a compressor has been set with
 * `asdf_block_compression_set`, the data is compressed when the file is
 * written.
 *
 * :param block: The `asdf_block_t *` handle.
 * :param data: The uncompressed data buffer (borrowed).
 * :param size: The size of ``data`` in bytes.
 * :return: ``0`` on success, non-zero on failure.
 */
ASDF_EXPORT int asdf_block_data_set(asdf_block_t *block, const void *data, size_t size);

/**
 * Set the block's data to already-compressed bytes, emitted verbatim.
 *
 * Like `asdf_block_data_set` the ``data`` buffer is *borrowed* and must
 * remain valid until the file is written.  The difference is that these
 * bytes are already compressed and are written as-is with the given
 * ``compression`` field, so a compressed block can be reproduced
 * byte-for-byte without decompressing, whereas `asdf_block_data_set` takes
 * uncompressed data that may be compressed on write.  This is a corner case
 * mainly intended for internal use (e.g. copying a compressed block); most
 * callers want `asdf_block_data_set` or `asdf_block_data_alloc`, though may
 * use this for implementing a custom data copying scheme.
 *
 * :param block: The `asdf_block_t *` handle.
 * :param data: The already-compressed bytes (borrowed).
 * :param size: The number of compressed bytes in ``data``.
 * :param data_size: The uncompressed size recorded in the block header.
 * :param compression: The (up to 4-character) compression name, or
 *   ``NULL``/``""`` for uncompressed.
 * :return: ``0`` on success, non-zero on failure.
 */
ASDF_EXPORT int asdf_block_data_set_compressed(
    asdf_block_t *block,
    const void *data,
    size_t size,
    uint64_t data_size,
    const char *compression);

/**
 * Set the allocated (reserved) size of the block in the file.
 *
 * ``allocated_size`` may be larger than the block's used size to reserve room
 * for the data to grow in place without moving later parts of the file.  A
 * value of ``0`` (the default) means "same as the used size".
 *
 * :param block: The `asdf_block_t *` handle.
 * :param allocated_size: The number of bytes to reserve, or ``0`` for auto.
 * :return: ``0`` on success, non-zero on failure.
 */
ASDF_EXPORT int asdf_block_allocated_size_set(asdf_block_t *block, uint64_t allocated_size);

/**
 * Append a block to the file's list of binary blocks.
 *
 * ``block`` must be a detached handle from `asdf_block_create`.  Ownership of
 * the block (and any data buffer it owns) transfers to ``file``; the handle
 * becomes a view onto the appended block and should subsequently be released
 * with `asdf_block_close`.  Requires the file to be open for writing.
 *
 * The same data can be written to multiple blocks by creating and appending
 * multiple blocks; there is no deduplication.
 *
 * :param file: The `asdf_file_t *` handle.
 * :param block: A detached `asdf_block_t *` from `asdf_block_create`.
 * :return: The now-appended ``block`` handle, or ``NULL`` on failure.
 */
ASDF_EXPORT asdf_block_t *asdf_block_append(asdf_file_t *file, asdf_block_t *block);


/**
 * Get the (uncompressed) size of the block data
 *
 * :param block: The `asdf_block_t *` handle
 * :return: The size of the block data as a `size_t`
 */
ASDF_EXPORT size_t asdf_block_data_size(asdf_block_t *block);


/**
 * Get the compression type, if any, of a block
 *
 * :param block: The `asdf_block_t *` handle
 * :return: A NULL-terminated string containing the compression type, if any
 */
ASDF_EXPORT const char *asdf_block_compression(asdf_block_t *block);


/**
 * Set the output compression type, if any, of a block
 *
 * :param block: The `asdf_block_t *` handle
 * :param compression: String representing the compressor to use (e.g. "bzp2")
 *   if any, or NULL or the empty string to set no compression
 * :return: Non-zero if the compression could not be set (e.g. invalid/unknown
 *   compressor); use `asdf_error` to check the error code
 */
ASDF_EXPORT int asdf_block_compression_set(asdf_block_t *block, const char *compression);


/**
 * Return the checksum from the block header
 *
 * :param block: The `asdf_block_t *` handle
 * :return: Pointer to the MD5 checksum digest array of 16 bytes
 */
ASDF_EXPORT const unsigned char *asdf_block_checksum(asdf_block_t *block);


/**
 * Size in bytes of the MD5 digest for block checksums
 */
#define ASDF_BLOCK_CHECKSUM_DIGEST_SIZE 16


/**
 * Verify the MD5 checksum of the block
 *
 * By default this is not done automatically when reading the block.
 *
 * If libasdf was built without MD5 support this always returns true.
 *
 * .. todo::
 *
 *   Maybe disable entirely if MD5 support was not available at build time.
 *
 * .. todo::
 *
 *   Add and document option to automatically verify checksums.
 *
 * :param block: The `asdf_block_t *` handle
 * :param expected: Optional pointer to a `uint8_t` buffer to receive the
 *   computed MD5 digest on return
 * :return: True if the checksum is valid
 */
ASDF_EXPORT bool asdf_block_checksum_verify(
    asdf_block_t *block, uint8_t expected[ASDF_BLOCK_CHECKSUM_DIGEST_SIZE]);


/**
 * Return a pointer to the (uncompressed) block data, and optionally its size
 *
 * This is the recommended way to read a block's data (mirroring
 * `asdf_ndarray_data`).  For a compressed block the data is decompressed on
 * first access.  Returns ``NULL`` if the block has no data (e.g. a freshly
 * created block with nothing assigned yet), in which case ``*size`` is 0.
 *
 * :param block: The `asdf_block_t *` handle
 * :param size: Optional `size_t *` into which the size of the block data is
 *   returned
 * :return: A pointer to the uncompressed block data, or ``NULL`` if none
 */
ASDF_EXPORT const void *asdf_block_data(asdf_block_t *block, size_t *size);


/**
 * Returns a `void *` to the beginning of the block data, and optionally its size
 *
 * For uncompressed block data this is equivalent to `asdf_block_data`; for
 * compressed blocks, however, this returns the raw compressed data without
 * decompression, and the size returned is the size of the compressed data.
 *
 * Use `asdf_block_data` for access to the uncompressed data.  Returns ``NULL``
 * if the block has no data, in which case ``*size`` is 0.
 *
 * :param block: The `asdf_block_t *` handle
 * :param size: Optional `size_t *` into which the size of the block data is
 *   returned
 * :return: A pointer to the raw block data, or ``NULL`` if none
 */
ASDF_EXPORT const void *asdf_block_data_raw(asdf_block_t *block, size_t *size);

ASDF_END_DECLS

#endif /* ASDF_BLOCK_H */
