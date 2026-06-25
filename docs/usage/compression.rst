.. _compression:

Compression
===========

ASDF can compress the bulk data of an ndarray's binary block, trading CPU time
for a smaller file.  Compression applies only to data stored in *internal*
blocks (see :ref:`ndarray-storage-modes`); inline data lives in the YAML tree
and is never compressed.

libasdf ships with three compressors, named by the strings ASDF uses to
identify them on disk:

* ``"zlib"`` -- the zlib/DEFLATE algorithm; broad compatibility.
* ``"bzp2"`` -- bzip2; typically smaller output, but slower.
* ``"lz4"`` -- LZ4; very fast, with more modest compression ratios; see
  :ref:`compression-lz4` below for more details.

.. _compression-writing:

Compressing on write
--------------------

By default ndarray data is written uncompressed.  To enable compression, call
`asdf_ndarray_compression_set` on the ndarray *before* passing it to
``asdf_set_ndarray`` (or `asdf_write_to`):

.. code:: c

   asdf_ndarray_compression_set(&nd, "lz4");
   asdf_set_ndarray(file, "image", &nd);

The compression string must name one of the compressors built into libasdf
listed above.  Pass ``NULL`` or an empty string to explicitly request no
compression.

`asdf_ndarray_compression_set` is a thin wrapper around the lower-level
`asdf_block_compression_set`, which operates on the raw `asdf_block_t`
associated with a block.  For the vast majority of use cases the ndarray-level
shortcut is all you need.


.. _compression-reading:

Decompression on read
---------------------

When *reading* a compressed array, its block is decompressed on demand the
first time you access the data -- for example via `asdf_ndarray_data` or any of
the reading functions (see :ref:`ndarray-data`).  `asdf_ndarray_data_raw`
instead returns the still-compressed bytes without decompressing them.

How and when decompression happens is governed by the ``decomp`` options of
`asdf_config_t`, passed to `asdf_open_ex` (see :ref:`configuration`):

.. code::

   asdf_config_t config = {
       .decomp = {
           .mode = ASDF_BLOCK_DECOMP_MODE_EAGER,
           .max_memory_bytes = 1073741824,
           .max_memory_threshold = 0.8,
           .chunk_size = 409600,
           .tmp_dir = "/var/tmp"
         }
   };

For most use cases the defaults are appropriate and no configuration is needed.
The two things that most deserve explanation are the handling of the *size* of
the decompressed data, and the decompression *mode*.

Compressed data size
^^^^^^^^^^^^^^^^^^^^

An array containing sparsely populated data may be very small compressed, but
explode significantly when decompressed.

By *default* decompression is performed entirely in-memory.  For most files on
modern systems with significant RAM and swap space this won't be an issue.
However, libasdf also has the option to decompress to a temporary file on disk
(effectively a temporary pagefile).  This may be useful if the file contains
very large arrays that do not fit in system memory.  To control this behavior
you can use one or both of the
:c:member:`max_memory_bytes <asdf_config_t.max_memory_bytes>` and
:c:member:`max_memory_threshold <asdf_config_t.max_memory_threshold>`
options.

The former sets a maximum number of *bytes* of decompressed data above which to
use decompression to disk.  The latter sets a percentage (from ``0.0`` to
``1.0``) of total system memory above which to enable this behavior.  If both
are specified, then the lower value of the two is applied as the absolute
threshold.

Most users won't need these settings but they are there in case you do.

By default this will write the file to your system's ``TMPDIR`` (typically
``/tmp`` or ``/var/tmp``).  It also understands the environment variable
``ASDF_TMPDIR`` to use as the default for all ASDF files read with libasdf.

.. warning::

   However, many systems use a RAM-based filesystem like
   `tmpfs <https://en.wikipedia.org/wiki/Tmpfs>`_ to back their temporary
   directory, which also renders this feature meaningless.  If you are
   sure you definitely need this for large file support, you can either
   pass the :c:member:`tmp_dir <asdf_config_t.tmp_dir>` option to also
   specify a specific disk-backed directory to use for the temp file.

Currently every individual `asdf_file_t*` handle does its own decompression
separately, though a future option might be to allow multiple `asdf_file_t*`
to share the same decompressed data pages.

Decompression mode
^^^^^^^^^^^^^^^^^^

In most cases, compressed block data is decompressed eagerly by default when
using either `ASDF_BLOCK_DECOMP_MODE_AUTO` or `ASDF_BLOCK_DECOMP_MODE_EAGER`.
This means that as soon as the decompressed data is needed the full block is
decompressed.

However, there is also experimental support for `ASDF_BLOCK_DECOMP_MODE_LAZY`
(currently on supported Linux versions *only*).  This allows blocks to be
decompressed one or more pages at a time on an as-needed basis, and works
totally transparently.

For zlib and bzip2 compression this is mostly only useful if you want to access
just bytes early in the block, as these algorithms can only be decompressed
sequentially (and the ASDF Standard does not currently define a scheme for
tiled compression).  If you need the entire block data it will all be
decompressed anyways.  This can still be useful even in that case if one is
taking chunks of the data and processing them sequentially.

By default lazy compression decompresses one system page at a time.  However,
it may be more efficient to use a larger value--this can be controlled with the
`asdf_config_t.chunk_size` setting (in bytes).  This will always be
automatically rounded up to the nearest page size.

.. warning::

   Due to technical limitations, lazy decompression does *not* work with
   disk-backed decompression, and the memory threshold options are ignored.
   If you really need both the best way is to ensure a sufficiently large
   pagefile available on your system, and to let the kernel manage swapping.
   See your system's documentation for the best way to create and manage
   a pagefile.


.. _compression-lz4:

A note on LZ4
-------------

The ``"lz4"`` compression is little peculiar.  It is not formally part of the
ASDF Standard, but it has been supported by the Python
`asdf <https://asdf.readthedocs.io>`_ library for some time, so libasdf
supports it for interoperability.  Rather than compressing the whole block
as a single stream, the ASDF LZ4 format stores the data as a series of
sequential chunks, each of which can be decompressed independently.

In principle this allows for more efficient *random* access to compressed data:
because an index can be built mapping each chunk to its range of decompressed
bytes, only the chunks actually needed have to be decompressed.  libasdf does
not yet take advantage of this -- LZ4 blocks are currently decompressed
sequentially like the other codecs -- but true random-access decompression is
planned for a future version (see `GitHub issue #90
<https://github.com/asdf-format/libasdf/issues/90>`_).

