.. _working-with-ndarrays:

Working with ndarrays
=====================

The :ref:`overview <ndarrays>` introduced the **ndarray** -- a typed,
multi-dimensional array whose bulk numeric data lives in a binary block while
its shape, datatype, and byte order are described by metadata in the YAML tree.
This page is a practical guide to *doing things* with ndarrays in libasdf:
reading them out of a file, copying and converting their data (including
sub-array "tiles"), and building new arrays to write back out.

Everything here is declared in ``asdf/core/ndarray.h`` (pulled in by the
umbrella ``asdf.h``) with supporting datatype definitions in
``asdf/core/datatype.h``.


.. _ndarray-struct:

The ``asdf_ndarray_t`` structure
--------------------------------

An ndarray is represented by an `asdf_ndarray_t`.  When *reading*, the library
heap-allocates one for you and fills in the metadata; when *writing*, you fill
one in yourself, typically on the stack.  The publicly visible fields are:

.. list-table::
   :header-rows: 1
   :widths: 20 30 50

   * - Field
     - Type
     - Meaning
   * - ``ndim``
     - ``uint32_t``
     - Number of dimensions.
   * - ``shape``
     - ``const uint64_t *``
     - Length of each dimension; an array of ``ndim`` values.
   * - ``datatype``
     - `asdf_datatype_t`
     - Element datatype (see :ref:`ndarray-datatypes` below).
   * - ``byteorder``
     - `asdf_byteorder_t`
     - Byte order of the stored data (``ASDF_BYTEORDER_LITTLE`` or
       ``ASDF_BYTEORDER_BIG``).
   * - ``source``
     - ``size_t``
     - Index of the binary block holding the data.
   * - ``offset``
     - ``uint64_t``
     - Optional byte offset into the block where the data begins.
   * - ``strides``
     - ``const int64_t *``
     - Optional per-dimension strides; ``NULL`` means C-contiguous.

.. note::

   Only a subset of full ndarray functionality is implemented so far.  In
   particular ``complex`` datatypes, string (``ascii`` / ``ucs4``) and
   structured datatypes, masks, and arbitrarily strided data are not yet fully
   supported for reading.  See the
   `asdf/core/ndarray.h <https://github.com/asdf-format/libasdf/blob/main/include/asdf/core/ndarray.h>`__
   header for the current status.


.. _ndarray-reading:

Reading an ndarray from a file
------------------------------

If you know the :ref:`yaml-pointer` path to an array, read it with
`asdf_get_ndarray`:

.. code:: c

   asdf_ndarray_t *array = NULL;

   if (asdf_get_ndarray(file, "data", &array) != ASDF_VALUE_OK) {
       fprintf(stderr, "no ndarray at /data\n");
       return 1;
   }

   printf("%u-dimensional array\n", array->ndim);
   for (uint32_t dim = 0; dim < array->ndim; dim++)
       printf("  axis %u: %" PRIu64 " elements\n", dim, array->shape[dim]);

The library allocates ``array``; release it with `asdf_ndarray_destroy` when
you are done:

.. code:: c

   asdf_ndarray_destroy(array);

If you do not know the path in advance you can search the tree for the first
ndarray with `asdf_value_find` and the ``asdf_value_is_ndarray`` predicate, then
cast the matching value with `asdf_value_as_ndarray`:

.. code:: c

   asdf_value_t *root = asdf_get_value(file, "");
   asdf_value_t *found = asdf_value_find(root, asdf_value_is_ndarray);

   asdf_ndarray_t *array = NULL;
   if (found && asdf_value_as_ndarray(found, &array) == ASDF_VALUE_OK) {
       /* ... */
   }

   asdf_value_destroy(found);
   asdf_value_destroy(root);

See :ref:`values` for more on generic value handles and tree traversal.


.. _ndarray-data:

Accessing the raw data
----------------------

The quickest way to reach the element data is `asdf_ndarray_data`, which returns
a pointer to the (decompressed) bytes and optionally their size:

.. code:: c

   size_t nbytes = 0;
   const void *data = asdf_ndarray_data(array, &nbytes);

This pointer is owned by ``array`` and must not be freed.  The data is in the
array's *source* datatype and byte order, exactly as stored -- no conversion is
performed.  Two helpers describe the array's extent without touching the data:

* `asdf_ndarray_size` -- total number of elements (the product of the shape).
* `asdf_ndarray_nbytes` -- total size of the data in bytes.

`asdf_ndarray_data_raw` is a lower-level variant that, for compressed arrays,
returns the still-compressed bytes without decompressing them; for uncompressed
arrays it is equivalent to `asdf_ndarray_data`.

Because `asdf_ndarray_data` exposes the data in its original layout, you are
responsible for honoring ``byteorder`` and ``datatype`` yourself.  When you want
the data converted to a convenient host type, use the reading functions in the
next section instead.


.. _ndarray-read-convert:

Reading data with conversion
----------------------------

`asdf_ndarray_read_all` copies the whole array into a buffer, converting it to
the host's native byte order and, optionally, to a different numeric datatype:

.. code:: c

   double *values = NULL;
   asdf_ndarray_err_t err =
       asdf_ndarray_read_all(array, ASDF_DATATYPE_FLOAT64, (void **)&values);

   if (err == ASDF_NDARRAY_OK) {
       uint64_t n = asdf_ndarray_size(array);
       /* values[0 .. n-1] are now native-endian doubles */
       free(values);
   }

Passing ``NULL`` for the destination (as above, via a pointer whose value is
``NULL``) asks the library to allocate a buffer of the right size; the caller
then owns that memory and must ``free()`` it.  Alternatively, pre-allocate a
buffer of `asdf_ndarray_nbytes` (or the appropriate size for the converted
type) and pass its address.  Pass `ASDF_DATATYPE_SOURCE` as the destination
datatype to keep the array's original element type and only normalize byte
order.

Reading tiles
~~~~~~~~~~~~~

Often you only need a rectangular sub-region ("tile") of a large array.
`asdf_ndarray_read_tile_ndim` reads an arbitrary N-dimensional cutout given an
``origin`` and a ``shape``, each an array of ``ndim`` values:

.. code:: c

   /* A 5x5 tile anchored at the array origin of a 2-D array */
   uint64_t origin[2] = {0, 0};
   uint64_t shape[2]  = {5, 5};

   float *tile = NULL;
   asdf_ndarray_err_t err = asdf_ndarray_read_tile_ndim(
       array, origin, shape, ASDF_DATATYPE_FLOAT32, (void **)&tile);

   if (err == ASDF_NDARRAY_OK) {
       /* tile holds 25 native-endian floats in row-major order */
       free(tile);
   }

The same buffer-ownership and datatype-conversion rules as
`asdf_ndarray_read_all` apply: pass ``NULL`` to have a buffer allocated for you,
or your own buffer to read into, and ``ASDF_DATATYPE_SOURCE`` to keep the source
type.  A tile that extends past the bounds of the array yields
`ASDF_NDARRAY_ERR_OUT_OF_BOUNDS`.

For the common two-dimensional case, `asdf_ndarray_read_tile_2d` offers a
friendlier signature taking ``x``, ``y``, ``width``, and ``height`` directly
(plus an optional ``plane_origin`` selecting a plane of a higher-dimensional
array):

.. code:: c

   float *tile = NULL;
   asdf_ndarray_read_tile_2d(
       array, /* x= */ 10, /* y= */ 20, /* width= */ 8, /* height= */ 8,
       /* plane_origin= */ NULL, ASDF_DATATYPE_FLOAT32, (void **)&tile);


.. _ndarray-datatypes:

Datatypes and byte order
------------------------

An element datatype is described by `asdf_datatype_t`, whose ``type`` field is
one of the `asdf_scalar_datatype_t` enums -- for example ``ASDF_DATATYPE_UINT8``,
``ASDF_DATATYPE_INT32``, ``ASDF_DATATYPE_FLOAT32``, or ``ASDF_DATATYPE_FLOAT64``.
For simple numeric arrays setting ``type`` is all that is required;
`asdf_datatype_size` then reports the size of a single element in bytes.

Byte order is given by `asdf_byteorder_t`: `ASDF_BYTEORDER_LITTLE` or
`ASDF_BYTEORDER_BIG` for explicit endianness, or `ASDF_BYTEORDER_DEFAULT` to
let the library choose when writing.  `asdf_scalar_datatype_from_string` and
`asdf_scalar_datatype_to_string` convert between the enumerators and their ASDF
string names (e.g. ``"float64"``).


.. _ndarray-writing:

Building an ndarray to write
----------------------------

For writing you stack-allocate an `asdf_ndarray_t` and fill in the fields that
describe the array, then allocate a data buffer for its contents with
`asdf_ndarray_data_alloc`:

.. code:: c

   const uint64_t shape[] = {128, 128};
   asdf_ndarray_t nd = {
       .datatype  = (asdf_datatype_t){.type = ASDF_DATATYPE_FLOAT32},
       .byteorder = ASDF_BYTEORDER_LITTLE,
       .ndim      = 2,
       .shape     = shape,
   };

   float *data = asdf_ndarray_data_alloc(&nd);
   for (int idx = 0; idx < 128 * 128; idx++)
       data[idx] = (float)idx;

   asdf_set_ndarray(file, "image", &nd);

`asdf_ndarray_data_alloc` allocates a correctly sized buffer on the heap based
on the array's shape and datatype.  After the file has been written, release it
with `asdf_ndarray_data_dealloc` (safe to call after `asdf_close`):

.. code:: c

   asdf_write_to(file, "out.asdf");
   asdf_close(file);
   asdf_ndarray_data_dealloc(&nd);

.. note::

   When building an ndarray *inside* an extension's serialize callback, use
   `asdf_ndarray_data_alloc_temp` instead -- its buffer is freed automatically
   once the write completes.

See :ref:`writing` for the surrounding file-writing workflow.


.. _ndarray-storage-modes:

Storage modes
-------------

By default an array's data is written into a binary block ("internal"
storage).  `asdf_ndarray_storage_set` selects a different
`asdf_array_storage_t` mode:

* ``ASDF_ARRAY_STORAGE_INTERNAL`` -- data is written to a binary block in the
  same file (the default).
* ``ASDF_ARRAY_STORAGE_INLINE`` -- data is serialized directly into the YAML
  tree as a nested sequence.  This is convenient for very small arrays but a
  warning is logged above a configurable element-count threshold.
* ``ASDF_ARRAY_STORAGE_EXTERNAL`` -- reserved for data stored in a separate
  file; not yet supported.

.. code:: c

   asdf_ndarray_storage_set(&nd, ASDF_ARRAY_STORAGE_INLINE);

`asdf_ndarray_storage` reports the mode that will be used for an array when
writing it out.  If the array was ready from an existing file, it will also
report the storage format that was used in that file.

.. _ndarray-compression:

Compression
-----------

Internal block data can be compressed.  Call `asdf_ndarray_compression_set` on
the array *before* passing it to ``asdf_set_ndarray`` (or `asdf_write_to`):

.. code:: c

   asdf_ndarray_compression_set(&nd, "lz4");
   asdf_set_ndarray(file, "image", &nd);

Compressed arrays are transparently decompressed when read back.  See
:ref:`compression` for the full list of supported compressors.
