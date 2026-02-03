.. _opening:

Opening and reading ASDF files
==============================

The library's main handle to any given ASDF file is the opaque `asdf_file_t`
struct.  It is allocated, and a pointer to it returned (much like a `FILE *`
from stdlib functions like `open()`) using `asdf_open`:

.. code:: c

   asdf_file_t *file = asdf_open("observation.asdf", "r");

   if (file == NULL) {
       const char *error = asdf_error(file);
       fprintf(stderr, "error opening the ASDF file: %s\n", error);
       return 1;
   }

The second argument ``"r"`` is a mode string.  Currently ``"r"`` is the only
accepted value, but more will be added when write support is added.

In addition to `asdf_open` can read from an existing `FILE *`, or memory buffer
passed as a `const void *` and `size_t` arguments.  See equivalently
`asdf_open_file`, `asdf_open_fp`, and `asdf_open_mem` respectively.
These functions also have ``_ex`` variants that can accept additional
configuration parameters; see :ref:`configuration`.

In all these cases it is also valid to pass a ``NULL`` pointer as the first
argument, like:

.. code:: c

   asdf_file_t *file = asdf_open(NULL);

This indicates creation of a new file.


.. _error-handling:

Error handling
--------------

Most state in libasdf, including the state of the parser, error messages, and
log messages, are tied to a single open file represented by an `asdf_file_t`.

Most functions, like `asdf_open`, that return a pointer to some kind of object
will return `NULL` if an error occurred, unless otherwise noted in the API
documentation.  In such cases, calling `asdf_error(file)` will return a message
explaining the error.

In the specific case of `asdf_open`, as an error result does not return an
`asdf_file_t *` in the first place, calling `asdf_error(NULL)` will return
global error messages.

`asdf_error` is used primarily for errors in the parsing/file structure itself,
as well as memory errors.  Other functions return a value of some error enum,
such as `asdf_value_err_t`.  This is used when reading values out of the file,
to indicate why reading the value failed (not found, wrong type, etc.). More on
this in :ref:`values`.

Closing the file and cleanup
----------------------------

Calling `asdf_close(file)` closes the underlying file handler (unless the file
was opened with `asdf_open_fp` in which case the caller is responsible for closing
the file), and releases data structures used by the ASDF parser.

It does *not* release memory used by other objects allocated in the process of
reading the file, `asdf_value_t`, `asdf_ndarray_t`, etc.  Those should be
cleaned up with calls to `asdf_value_destroy`, `asdf_ndarray_destroy`, and
other respective ``asdf_<type>_destroy`` calls when those objects are no longer
needed.

.. note::

   There are, however, plans to improve this by tracking references to
   resources associated with a single file, and releasing those resources,
   where possible, when closing the file.  This will make simple use cases a
   lot easier to manage.  Nevertheless, it's still good practice to release
   resources when no longer needed to avoid memory leaks in your own
   application.


.. _configuration:

Advanced configuration
----------------------

`asdf_open`, `asdf_open_fp`, etc. all have ``_ex`` variants: `asdf_open_ex`,
`asdf_open_fp_ex`, `asdf_open_mem_ex`.

These take an additional `asdf_config_t *` argument.  This is a struct in which
you can provide additional per-file configuration for the library to control
the behavior of the parser, and other options.  The simplest example is an
empty config struct (equivalent also to passing `NULL`) which will then be
filled in with the default settings.

.. code:: c

   asdf_config_t config = {0};
   asdf_file_t *file = asdf_open_ex("observation.asdf", "r", &config);

.. note::

   The provided ``config`` object is copied/modified internally (e.g. to fill
   in defaults), so it's safe to point to a local variable.

Any option in the user-provided config left as ``0`` will be filled in from the
default configuration (though this may change in the future).

Currently, the only documented configuration options that may be of interest to
end users are the options for controlling the behavior of *decompression* of
compressed blocks.  For example:

.. code::

   asdf_config_t config = {
       .decomp = {
           mode = ASDF_BLOCK_DECOMP_MODE_EAGER,
           max_memory_bytes = 1073741824,
           max_memory_threshold = 0.8,
           chunk_size = 409600,
           tmp_dir = "/var/tmp"
         }
   };

Two things that deserve explanation here are handling of the *size* of
the decompressed data, and the decompression *mode*.

Compressed data size
^^^^^^^^^^^^^^^^^^^^

An array containing sparsely populated data may be very small compressed, but
explode significantly when decompressed.

.. todo::

   Document better how to inspect the compressed vs. decompressed sizes of a
   block.

By *default* decompression is performed entirely in-memory.  For most files on
modern systems with significant RAM and swap space this won't be an issue.
However, libasdf also has the option to decompress to a temporary file on disk
(effectively a temporary pagefile).  To control this behavior you can use one
or both of the
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
   pass the :c:member`tmp_dir <asdf_config_t.tmp_dir` option to also
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
