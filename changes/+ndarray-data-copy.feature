Added ``asdf_ndarray_data_copy`` for copying data into an ndarray's buffer.

This is a convenience over ``asdf_ndarray_data_alloc`` that both allocates the
ndarray's data buffer (sized from its shape and datatype) and copies
``asdf_ndarray_nbytes`` bytes into it from a source buffer.
