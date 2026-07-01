.. _writing:

Writing ASDF files
==================

So far we have only discussed reading from existing files.  Creating a new ASDF
file from scratch -- or writing one out to disk -- is the subject of this
section.


Creating a new file
-------------------

Whereas :ref:`opening` showed how to open an existing file using a filename,
``FILE *``, or memory buffer, creating a new *empty* file is done by passing
``NULL`` as the first argument to `asdf_open`:

.. code:: c

   asdf_file_t *file = asdf_open(NULL);

This gives you a blank slate: an `asdf_file_t` with an empty YAML tree and no
binary blocks.  From here you can populate the tree with metadata and add
ndarrays before writing it out.  The same ``_ex`` variants described in
:ref:`configuration` also work here, so `asdf_open_ex` accepts ``NULL`` as its
first argument if you need to pass a custom `asdf_config_t`.


Setting metadata values
-----------------------

The functions used to *read* scalar values (described in :ref:`values`) all
have symmetric write counterparts in the ``asdf_set_<type>`` family.  The
signatures mirror the getters: take the file pointer, a :ref:`yaml-pointer`
path, and the value to write.

.. code:: c

   asdf_set_string0(file, "author",      "Nancy Roman");
   asdf_set_int64(file,   "observation", 42);
   asdf_set_double(file,  "exposure",    300.0);
   asdf_set_bool(file,    "calibrated",  true);

If any intermediate mapping keys in the path do not yet exist they are created
automatically.  For example, writing to ``"metadata/telescope/name"`` will
create the ``metadata`` and ``telescope`` mappings on the fly if they are not
already present.

Existing values at a path are silently replaced when you write to the same
path a second time.


Adding ndarrays
---------------

Ndarrays are described by an `asdf_ndarray_t` struct.  Unlike the
heap-allocated ``asdf_ndarray_t *`` returned by `asdf_get_ndarray` when
reading, for *writing* you stack-allocate the struct yourself and fill in the
fields that describe the array's shape, element type, and byte order:

.. code:: c

   const uint64_t shape[] = {128, 128};
   asdf_ndarray_t nd = {
       .datatype  = (asdf_datatype_t){.type = ASDF_DATATYPE_FLOAT32},
       .byteorder = ASDF_BYTEORDER_LITTLE,
       .ndim      = 2,
       .shape     = shape,
   };

Next, allocate a data buffer of the right size with `asdf_ndarray_data_alloc`
and fill it however your application requires:

.. code:: c

   float *data = asdf_ndarray_data_alloc(&nd);
   for (int idx = 0; idx < 128 * 128; idx++)
       data[idx] = (float)idx;

This will allocate a buffer on the heap of the correct size for your ndarray
given the required datatype and shape specifications.

Then place the ndarray in the file's YAML tree at the path of your choice
with `asdf_set_ndarray`:

.. code:: c

   asdf_set_ndarray(file, "image", &nd);

After the file has been written you should release the buffer:

.. code:: c

   asdf_ndarray_data_dealloc(&nd);

.. note::

   `asdf_ndarray_data_alloc` is appropriate when you build the ndarray *before*
   the write call.  If you are writing an ndarray inside a serialization
   callback you should use `asdf_ndarray_data_alloc_temp` instead: the
   allocated memory is freed automatically once the write finishes.


Writing the file
----------------

Once the tree is populated, write it out with the `asdf_write_to` macro.  It
dispatches to one of three underlying functions based on the type of its second
argument.

**To a file path** -- the most common case:

.. code:: c

   if (asdf_write_to(file, "output.asdf") != 0) {
       fprintf(stderr, "write failed\n");
   }

This calls `asdf_write_to_file`, which creates (or truncates) the named path
and writes the complete ASDF file -- header, YAML tree, binary blocks, and
block index -- in one shot.

**To a memory buffer**:

.. code:: c

   void *buf  = NULL;
   size_t len = 0;
   if (asdf_write_to(file, &buf, &len) != 0) {
       fprintf(stderr, "write failed\n");
   }
   /* ... use buf[0..len-1] ... */
   free(buf);

This calls `asdf_write_to_mem`.  When the first output argument ``*buf`` is
``NULL``, the library allocates a buffer large enough for the whole file and
stores its address in ``*buf`` and its size in ``*len``.  The caller is
responsible for freeing that buffer with ``free()``.  Alternatively, you can
pre-allocate a buffer and pass a non-``NULL`` ``*buf`` with the available
capacity in ``*len``; if the capacity is insufficient the output is truncated
and a non-zero value is returned.

**To an open** ``FILE *`` **stream** -- calls `asdf_write_to_fp`:

.. code:: c

   asdf_write_to(file, fp);

This can be useful when you have already opened the file yourself, for instance
to write to ``stdout`` or to an already-positioned file descriptor.

After writing, close the `asdf_file_t` with `asdf_close` as you normally
would:

.. code:: c

   asdf_close(file);

It is safe (and recommended) to call `asdf_ndarray_data_dealloc` *after*
`asdf_close`.


Example
-------

The following program opens the test file ``cube.asdf``, reads its
``cube`` ndarray (a 10x10x10 array of ``uint8`` values), builds a new ASDF
file with a couple of metadata fields and a derived ndarray whose elements are
the *cubes* of the original values (i.e. ``v^3 mod 256``), and writes the
result to an in-memory buffer.

.. code:: c
   :test: test-write-cube
   :fixture: cube.asdf

   #include <stdio.h>
   #include <stdlib.h>
   #include <stdint.h>
   #include <asdf.h>

   int main(int argc, char **argv) {
       if (argc < 2) {
           fprintf(stderr, "Usage: %s input.asdf\n", argv[0]);
           return 1;
       }

       /* Open an existing ASDF file for reading */
       asdf_file_t *src = asdf_open(argv[1], "r");
       if (src == NULL) {
           fprintf(stderr, "error opening %s\n", argv[1]);
           return 1;
       }

       /* Read the ndarray named "cube" */
       asdf_ndarray_t *cube = NULL;
       asdf_value_err_t err = asdf_get_ndarray(src, "cube", &cube);
       if (err != ASDF_VALUE_OK) {
           fprintf(stderr, "error reading ndarray: %d\n", err);
           asdf_close(src);
           return 1;
       }

       /* Copy all element data into a freshly-allocated buffer */
       uint8_t *src_data = NULL;
       asdf_ndarray_err_t nerr = asdf_ndarray_read_all(
           cube, ASDF_DATATYPE_SOURCE, (void **)&src_data);
       if (nerr != ASDF_NDARRAY_OK) {
           fprintf(stderr, "error reading ndarray data: %d\n", nerr);
           asdf_ndarray_destroy(cube);
           asdf_close(src);
           return 1;
       }

       uint64_t nelem = asdf_ndarray_size(cube);

       /* Create a new, empty ASDF file */
       asdf_file_t *out = asdf_open(NULL);
       if (out == NULL) {
           fprintf(stderr, "error creating output file\n");
           free(src_data);
           asdf_ndarray_destroy(cube);
           asdf_close(src);
           return 1;
       }

       /* Add a couple of metadata values */
       asdf_set_string0(out, "description", "Cube of original values (v^3 mod 256)");
       asdf_set_int64(out, "ndim", (int64_t)cube->ndim);

       /*
        * Build the output ndarray: same shape and datatype as the source but
        * with each element replaced by the cube of its value (mod 256 to keep
        * the result in uint8 range).
        */
       asdf_ndarray_t out_cube = {
           .datatype  = (asdf_datatype_t){.type = ASDF_DATATYPE_UINT8},
           .byteorder = ASDF_BYTEORDER_BIG,
           .ndim      = cube->ndim,
           .shape     = cube->shape,
       };

       uint8_t *out_data = asdf_ndarray_data_alloc(&out_cube);
       if (out_data == NULL) {
           fprintf(stderr, "out of memory\n");
           free(src_data);
           asdf_ndarray_destroy(cube);
           asdf_close(src);
           asdf_close(out);
           return 1;
       }

       for (uint64_t idx = 0; idx < nelem; idx++) {
           uint32_t v = src_data[idx];
           out_data[idx] = (uint8_t)((v * v * v) % 256);
       }

       /* Place the ndarray in the output file's tree */
       asdf_set_ndarray(out, "cube", &out_cube);

       /* Write the file to a memory buffer */
       void *buf  = NULL;
       size_t len = 0;
       if (asdf_write_to(out, &buf, &len) != 0) {
           fprintf(stderr, "write failed\n");
           free(src_data);
           asdf_ndarray_destroy(cube);
           asdf_ndarray_data_dealloc(&out_cube);
           asdf_close(out);
           asdf_close(src);
           return 1;
       }

       printf("wrote %zu-byte ASDF file to memory\n", len);

       free(buf);
       asdf_close(out);
       asdf_ndarray_data_dealloc(&out_cube);
       free(src_data);
       asdf_ndarray_destroy(cube);
       asdf_close(src);
       return 0;
   }


Compressing ndarray blocks
--------------------------

By default ndarray data is written uncompressed.  To enable compression, call
`asdf_ndarray_compression_set` on the ndarray *before* passing it to
`asdf_set_ndarray` (or `asdf_write_to`):

.. code:: c

   asdf_ndarray_compression_set(&nd, "lz4");
   asdf_set_ndarray(file, "image", &nd);

See :ref:`compression` for the list of supported compressors and for more
detail on how compression and decompression work.
