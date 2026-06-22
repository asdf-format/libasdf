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
           printf("Software: %s\n", software);
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
           if (meta->history.entries && meta->history.entries[0]) {
               // This is a NULL-terminated array of asdf_history_entry_t*
               printf("First history entry: %s\n", meta->history.entries[0]->description);
           } else {
               printf("File does not contain any history entries\n");
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
   
       // Find the first ndarray in the file, if any
       asdf_value_t *root = asdf_get_value(file, "");
       asdf_value_t *value = asdf_value_find(root, asdf_value_is_ndarray);
   
       if (!value) {
           fprintf(stderr, "no ndarray found in the file\n");
           return 1;
       }
   
       // Generic values can be *cast* to a specific type with the
       // `asdf_value_as_` API, for example to cast to an ndarray and check that
       // the cast succeeded:
       asdf_ndarray_t *ndarray = NULL;
       err = asdf_value_as_ndarray(value, &ndarray);
       if (err != ASDF_VALUE_OK) {
           fprintf(stderr, "error reading ndarray metadata: %d\n", err);
           return 1;
       }
   
       printf("Using ndarray at: %s\n", asdf_value_path(value));
       printf("Number of data dimensions: %d\n", ndarray->ndim);
   
       // The generic value wrappers are no longer needed and should be freed.
       asdf_value_destroy(value);
       asdf_value_destroy(root);
   
       // Get just a raw pointer to the ndarray data block (if uncompressed).
       // Optionally returns the size in bytes as well
       size_t size = 0;
       const void *data = asdf_ndarray_data(ndarray, &size);
   
       if (data == NULL) {
           fprintf(stderr, "error reading ndarray data\n");
           return 1;
       }
   
       // The asdf_ndarray_read_tile_ functions copy a rectangular cutout of
       // the array into a buffer, converting datatype and endianness as needed.
       // If you don't pass your own buffer one is allocated for you; either way
       // you are responsible for freeing it.
   
       // origin and shape must have one entry per array dimension, so we size
       // them to the array we found.  Here we take a cutout of up to 5 elements
       // along each axis, clamped to the array's actual size.
       uint64_t *origin = calloc(ndarray->ndim, sizeof(uint64_t));
       uint64_t *shape = calloc(ndarray->ndim, sizeof(uint64_t));

       if (origin == NULL || shape == NULL) {
           fprintf(stderr, "out of memory\n");
           return 1;
       }
   
       uint64_t tile_nelem = 1;
       for (uint32_t dim = 0; dim < ndarray->ndim; dim++) {
           shape[dim] = ndarray->shape[dim] < 5 ? ndarray->shape[dim] : 5;
           tile_nelem *= shape[dim];
       }
   
       // Read the cutout, converting to double regardless of the source type
       double *tile = NULL;
       asdf_ndarray_err_t array_err = asdf_ndarray_read_tile_ndim(
           ndarray,
           origin,
           shape,
           ASDF_DATATYPE_FLOAT64,
           (void **)&tile
       );
   
       free(origin);
       free(shape);
   
       if (array_err != ASDF_NDARRAY_OK) {
           fprintf(stderr, "error reading ndarray: %d\n", array_err);
           return 1;
       }
   
       printf("Value at center of cutout: %g\n", tile[tile_nelem / 2]);
   
       free(tile);
       asdf_ndarray_destroy(ndarray);
       asdf_close(file);
       return 0;
    }


With libasdf installed on your system you can compile and run this test like:

.. code:: sh

   $ gcc asdf-test.c -o asdf-test -lasdf
   $ ./asdf-test tests/fixtures/cube.asdf

Which outputs::

   Software: The ASDF Developers
   First history entry: A very small data cube for testing
   Using ndarray at: /cube
   Number of data dimensions: 3
   Value at center of cutout: 222

In this case the test is run on the file
`cube.asdf <https://github.com/asdf-format/libasdf/blob/main/tests/fixtures/cube.asdf>`__
in this repository's test fixtures, though the program should work on any ASDF
file containing a non-empty ndarray.


Development
===========

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
- **zlib**, **bzip2**, and **lz4** (for compression support)
- **libbsd** (required for MD5 checksum support)
- **libstatgrab** (optional, for system resource heuristics)
- **argp** (this is a feature of glibc, but if compiling with a different libc you need a
  standalone version of this; also it is only needed if building the command-line tool)

On **Debian/Ubuntu**::

    sudo apt install build-essential pkg-config libfyaml-dev \
      zlib1g-dev libbz2-dev liblz4-dev libstatgrab-dev libbsd-dev

On **Fedora**::

    sudo dnf install gcc make pkgconf libfyaml-devel \
      zlib-devel bzip2-devel lz4-devel libstatgrab-devel libbsd-devel

On **macOS** (with Homebrew)::

    brew install pkg-config libfyaml argp-standalone \
      zlib bzip2 lz4 libstatgrab libbsd

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

Official Extensions
===================

- `libasdf-gwcs <https://github.com/asdf-format/libasdf-gwcs>`__ — GWCS (Generalized
  World Coordinate System) extension for reading ASDF files containing WCS transforms
  and coordinate frames.
