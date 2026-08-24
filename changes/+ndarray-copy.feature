Implemented ``asdf_ndarray_copy``, which makes an independent deep copy of an
ndarray.

The copy duplicates the array's metadata (shape, strides, datatype) and its
data: an inline array clones its YAML data, while a block-backed array copies
its block into a new block managed for the destination file (preserving
compression).  Because the copy is fully independent it may be assigned to a
different file than the source and written like any other ndarray.
