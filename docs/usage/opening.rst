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
explaining the error. For more fine-grained error handling you can use
`asdf_error_code(file)` to return an `asdf_error_code_t` error code enum
(similar to ``errno``).

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
default configuration.

Currently the only configuration options documented for end users are those
under ``decomp``, which control the behavior of *decompression* of compressed
blocks:

* :c:member:`decomp.mode <asdf_config_t.mode>` -- eager, lazy, or automatic
  decompression (see `asdf_block_decomp_mode_t`).
* :c:member:`max_memory_bytes <asdf_config_t.max_memory_bytes>` and
  :c:member:`max_memory_threshold <asdf_config_t.max_memory_threshold>` --
  thresholds above which decompression spills to a temporary file on disk.
* :c:member:`chunk_size <asdf_config_t.chunk_size>` -- chunk size for lazy
  decompression.
* :c:member:`tmp_dir <asdf_config_t.tmp_dir>` -- directory for the on-disk
  decompression temporary file.

These are described in detail, along with the trade-offs between the different
decompression modes, in :ref:`compression`.  The remaining ``parser``,
``emitter``, and ``log`` sub-structs are lower-level and not yet documented for
general use.
