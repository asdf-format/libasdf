Added ``asdf_free`` for releasing buffers that libasdf allocates on the
caller's behalf: those returned by ``asdf_write_to_mem``,
``asdf_ndarray_read_all``, ``asdf_ndarray_read_tile_ndim`` and
``asdf_ndarray_read_tile_2d``.

It is currently just ``free()``, so existing code keeps working, but it stops
the allocator from being part of the ABI: freeing across a DLL boundary is
undefined where the library and the application link different C runtimes, and
naming ``free()`` in the contract would prevent these functions from ever
allocating differently.
