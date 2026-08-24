#pragma once

#define ASDF_CORE_NDARRAY_INTERNAL
#include "asdf/core/ndarray.h" // IWYU pragma: export


typedef struct {
    /*
     * The logical block that manages this ndarray's binary data.  It holds the
     * data whether it originated from `asdf_ndarray_data_alloc` (a detached,
     * file-less block owning an mmap'd buffer), from a binary block read out of
     * a file (a view opened with `asdf_block_open`), or from inline YAML data
     * materialized on first access.  When written with binary-block storage
     * this same block is appended to the file; with inline storage its data is
     * serialized into the YAML and it is never appended.  Created lazily.
     */
    asdf_block_t *block;
    asdf_file_t *file;
    /**
     * Optional compressor name to set on the block for the ndarray when
     * writing a new ndarray
     */
    const char *write_compression;
    /* Cloned YAML sequence for inline ndarrays; non-NULL iff this is an
     * inline ndarray whose data has not yet been parsed into the block.  This
     * is an ndarray-level property: the block itself is storage-agnostic. */
    asdf_sequence_t *inline_data;
    /* Storage mode to use when writing this ndarray */
    asdf_array_storage_t array_storage;
} asdf_ndarray_internal_t;


/** Internal definition of the asdf_ndarray_t type with extended internal fields */
typedef struct asdf_ndarray {
    size_t source;
    uint32_t ndim;
    uint64_t *shape;
    asdf_datatype_t datatype;
    asdf_byteorder_t byteorder;
    uint64_t offset;
    int64_t *strides;

    // Internal fields
    asdf_ndarray_internal_t *internal;
} asdf_ndarray_t;
