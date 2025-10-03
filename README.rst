libasdf
#######

.. _begin-badges:

.. image:: https://github.com/asdf-format/libasdf/workflows/Build/badge.svg
    :target: https://github.com/asdf-format/libasdf/actions
    :alt: CI Status

.. image:: https://app.readthedocs.org/projects/libasdf/badge/?version=latest
    :target: https://libasdf.readthedocs.io/en/latest/
    :alt: Documentation Status

.. _end-badges:

A C library for reading (and eventually writing) `ASDF
<https://www.asdf-format.org/en/latest/>`__ files.


Introduction
============

libasdf is largely a wrapper around `libfyaml <https://pantoniou.github.io/libfyaml/>`__
but with an understanding of the structure of ASDF files, with the capability to read and
extract binary block data, as well as typed getters for metadata in the ASDF tree.

It also features an extension mechanism (still nascent) for reading ASDF schemas, including
the core schemas such as ``core/ndarray-<x.y.z>`` into C-native datastructures.

Getting Started
---------------

To open an ASDF file with libasdf the simplest way is to use the ``asdf_open`` function.
This returns an ``asdf_file_t *`` which is your main interface to the ASDF file.
When done with the file make sure to call ``asdf_close`` to free resources:

.. code:: c
   :name: test-open-close-file

   #include <stdio.h>
   #include <asdf.h>
   
   int main(int argc, char **argv) {
       if (argc < 2) {
           fprintf(stderr, "Usage: %s filename\n", argv[0]);
           return 1;
       }
       const char *filename = argv[1];
       asdf_file_t *file = asdf_open(filename, "r");

       if (file == NULL) {
           fprintf(stderr, "error opening the ASDF file\n");
           return 1;
       }

       asdf_close(file);
       return 0;
    }

The following more complete example demonstrates how to read different metadata out of
the ASDF tree, as well as extract block data.  Inline comments provide further explanation:

.. code:: c
   :name: test-read-metadata-ndarray

   #include <stdio.h>
   #include <stdlib.h>
   #include <asdf.h>
   
   int main(int argc, char **argv) {
       if (argc < 2) {
           fprintf(stderr, "Usage: %s filename\n", argv[0]);
           return 1;
       }
       const char *filename = argv[1];

       // The mode string "r" is required and is the only currently-supported mode
       asdf_file_t *file = asdf_open(filename, "r");
 
       if (file == NULL) {
           fprintf(stderr, "error opening the ASDF file\n");
           return 1;
       }
 
       // The simplest way to read metadata from the file is with the
       // `asdf_get_<type>*` family of functions
       // They all return a value by pointer argument and return an
       // `asdf_value_error_t`
       // For example you can read a string from the metadata like:
 
       const char *software = NULL;
       // Returns a 0-terminated string into *software.
       asdf_value_err_t err = asdf_get_string0(file, "asdf_library/author", &software);
 
       if (err == ASDF_VALUE_OK) {
           printf("software: %s\n", software);
       }
 
       // Other errors could be e.g. ASDF_VALUE_ERR_NOT_FOUND if the key doesn't
       // exist, or ASDF_VALUE_ERR_TYPE_MISMATCH if it's not a string.
 
       // There are also extensions registered for some (not all yet) of the
       // core schemas.  Objects defined by extension schemas (identified by
       // their YAML tags) also have corresponding asdf_get_<type> functions:
       asdf_meta_t *meta = NULL;
 
       // This reads the top-level core/asdf-1.0.0 schema
       err = asdf_get_meta(file, "/", &meta);
       if (err == ASDF_VALUE_OK) {
           if (meta->history.entries[0]) {
               // This is a NULL-terminated array of asdf_history_entry_t*
               printf("first history entry: %s\n", meta->history.entries[0]->description);
           }
       }
 
       // Functions like `asdf_get_meta` that return into a double-pointer to a
       // struct allocate memory for that structure automatically.
       // The all have a corresponding `asdf_<type>_destroy` function.
       // The plan is to track these on the file object (issue #34) to make
       // memory management easier and cleaner, but for now you have to free
       // them manually when you're done with them. This is good practice in any
       // case.
       asdf_meta_destroy(meta);
 
       // ndarrays work no differently; this reads an ndarray named "cube".
       asdf_ndarray_t *ndarray = NULL;
       err = asdf_get_ndarray(file, "cube", &ndarray);
       if (err != ASDF_VALUE_OK) {
           fprintf(stderr, "error reading ndarray metadata: %d\n", err);
           return 1;
       }
 
       printf("number of data dimensions: %d\n", ndarray->ndim);
 
       // Get just a raw pointer to the ndarray data block (if uncompressed).
       // Optionally returns the size in bytes as well
       size_t size = 0;
       void *data = asdf_ndarray_data_raw(ndarray, &size);

       if (data == NULL) {
           fprintf(stderr, "error reading ndarray data\n");
           return 1;
       }
 
       // Slightly more useful is the asdf_ndarray_read_tile_ functions.
       // They can copy the data, including converting endianness into a tile
       // buffer.  If an existing buffer is not passed it will allocate one of
       // the correct size to hold the data.  The user is responsible for
       // freeing the buffer.
 
       // Read a 10x10x10 cube
       const uint64_t origin[3] = {0, 0, 0};
       const uint64_t shape[3] = {10, 10, 10};
       void *tile = NULL;
       asdf_ndarray_err_t array_err = asdf_ndarray_read_tile_ndim(
           ndarray,
           origin,
           shape,
           ASDF_DATATYPE_SOURCE,
           &tile
       );
 
       if (array_err != ASDF_NDARRAY_OK) {
           fprintf(stderr, "error reading ndarray: %d\n", array_err);
           return 1;
       }
 
       free(tile);
       asdf_ndarray_destroy(ndarray);
       asdf_close(file);
       return 0;
   }


Development
===========

Minimal requirements
--------------------

First we'll have to have some (probably libfyaml + headers)


Building from source tarballs
-----------------------------

First we'd have to have some.


Building from git
-----------------

libasdf's build system is built with CMake. To build this project
from source, you'll need the following software installed on your system:

Requirements
^^^^^^^^^^^^

To build this project from source, you'll need the following software installed
on your system:

- **CMake** (for generating the build system)
- **C compiler** (e.g., ``gcc`` or ``clang``)
- **Make** (e.g., ``GNU make``)
- **pkg-config**
- **libfyaml**
- **argp** (this is a feature of glibc, but if compiling with a different libc you need a
  standalone version of this; also it is only needed if building the command-line tool)

On **Debian/Ubuntu**::

    sudo apt install build-essential pkg-config libfyaml-dev

On **Fedora**::

    sudo dnf install gcc make pkgconf libfyaml-devel

On **macOS** (with Homebrew)::

    brew install pkg-config libfyaml argp-standalone

Building
^^^^^^^^

Clone the repository and build the project as follows::

    git clone https://github.com/asdf-format/libasdf.git
    cd libasdf
    mkdir build
    cd build
    cmake .. \
        -D ENABLE_TESTING=[YES/NO] \
        -D ENABLE_TESTING_SHELL=[YES/NO] \
        -D ENABLE_ASAN=[YES/NO] \
        -D FYAML_NO_PKGCONFIG=[YES/NO] \
            # If YES \
            -D FYAML_LIBDIR=[path/lib] \
            -D FYAML_INCLUDEDIR=[path/include] \
        -D ARGP_NO_PKGCONFIG=[YES/NO] \
            # If YES \
            -D ARGP_LIBDIR=[path/lib] \
            -D ARGP_INCLUDEDIR=[path/include]
    make
    sudo make install   # Optional, installs the binary system-wide

If doing a system install, as usual it's recommended to install to ``/usr/local``
by providing ``-DCMAKE_INSTALL_PREFIX=/usr/local`` when running ``cmake``.  Or, if you
have a ``${HOME}/.local`` you can set the prefix there, etc.

Notes
^^^^^

- Run ``make clean`` to clean build artifacts.
- Run ``make project_source`` to generate a source archive with CPack
- Run ``ctest --output-on-failure`` to execute unit tests
