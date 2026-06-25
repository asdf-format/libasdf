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
   :test: test-open-close-file
   :fixture: cube.asdf

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

The next example demonstrates how to write some simple metadata and an ndarray
to a new file:

.. code:: c
   :test: test-write-file
   :fixture: temp:out.asdf

   #include <asdf.h>

   int main(void) {
       const char *filename = "out.asdf";
   
       // open a "NULL" file for writing
       asdf_file_t *file = asdf_open(NULL);
   
       // assign a string to the "name" key of the ASDF tree
       asdf_set_string0(file, "name", "Dennis Richie");
   
       // assign a numeric value to the "foo" key
       asdf_set_int64(file, "foo", 42);
   
       // construct 2 arrays containing numeric values
       uint64_t N = 100;
   
       asdf_ndarray_t sequence = {
           .ndim = 1,
           .shape = (uint64_t[]){N},
           .datatype = {.type = ASDF_DATATYPE_UINT64}
       };
       uint8_t *sequence_data = asdf_ndarray_data_alloc(&sequence);
   
       asdf_ndarray_t squares = {
           .ndim = 1,
           .shape = (uint64_t[]){N},
           .datatype = {.type = ASDF_DATATYPE_UINT64}
       };
       uint64_t *squares_data = asdf_ndarray_data_alloc(&squares);
   
       for (uint64_t idx = 0; idx < N; idx++) {
           sequence_data[idx] = idx;
           squares_data[idx] = idx * idx;
       };
   
       // assign the "sequence" array to the "sequence" key
       asdf_set_ndarray(file, "sequence", &sequence);
   
       // nest the "squares" array under a parent "powers" key
       asdf_set_ndarray(file, "powers/squares", &squares);
   
       // write the ASDF file to disk
       asdf_write_to(file, filename);
   
       // clean up allocations
       asdf_ndarray_data_dealloc(&sequence);
       asdf_ndarray_data_dealloc(&squares);
       asdf_close(file);
       return 0;
   }

With libasdf installed on your system (see :ref:`development`) you can compile
and run this test like:

.. code:: sh

   $ gcc asdf-write.c -o asdf-write -lasdf
   $ ./asdf-write

It should produce an output file at ``out.asdf`` which you can inspect by hand.
The YAML portion of the ASDF file should contain:

.. code:: yaml

   #ASDF 1.0.0
   #ASDF_STANDARD 1.6.0
   %YAML 1.1
   %TAG ! tag:stsci.edu:asdf/
   --- !core/asdf-1.1.0
   asdf_library: !core/software-1.0.0
     name: libasdf
     version: 0.1.0a2
     author: The libasdf Developers
     homepage: https://github.com/asdf-format/libasdf
   name: Dennis Richie
   foo: 42
   sequence: !core/ndarray-1.1.0
     source: 0
     datatype: uint64
     shape: [
       100
       ]
     byteorder: little
   powers:
     squares: !core/ndarray-1.1.0
       source: 1
       datatype: uint64
       shape: [
         100
         ]
       byteorder: little
   ...


The next example shows how to read back in the same file:

.. code:: c
   :test: test-read-file
   :fixture: test-write-file.asdf

   #include <stdio.h>
   #include <stdlib.h>
   #include <asdf.h>
   
   int main(void) {
       const char *filename = "out.asdf";
   
       // open the ASDF file for reading
       asdf_file_t *file = asdf_open(filename, "r");
       if (!file) {
           fprintf(stderr, "Failed to open the file: %s\n", asdf_error(file));
           return 1;
       }
   
       // read and print the string stored under "name"
       const char *name = NULL;
       if (asdf_get_string0(file, "name", &name) == ASDF_VALUE_OK) {
           printf("name: %s\n", name);
       }
   
       // read and print the numeric value stored under "foo"
       int64_t foo = 0;
       if (asdf_get_int64(file, "foo", &foo) == ASDF_VALUE_OK) {
           printf("foo: %li\n", foo);
       }
   
       // read the "squares" array nested under the "powers" key
       asdf_ndarray_t *squares = NULL;
       uint64_t *squares_data = NULL;
       if (asdf_get_ndarray(file, "powers/squares", &squares) == ASDF_VALUE_OK) {
           if (asdf_ndarray_read_all(squares, ASDF_DATATYPE_UINT64, (void **)&squares_data) == ASDF_NDARRAY_OK) {
               // print the sum of the squares array
               uint64_t nelem = asdf_ndarray_size(squares);
               uint64_t sum = 0;
               for (uint64_t idx = 0; idx < nelem; idx++) {
                   sum += squares_data[idx];
               }
               printf("sum of squares values: %li\n", sum);
           }
       }
   
       // clean up allocations
       free(squares_data);
       asdf_ndarray_destroy(squares);
       asdf_close(file);
       return 0;
   }

Likewise compile and run the example with the output from the previous program:

.. code:: sh

   $ gcc asdf-read.c -o asdf-read -lasdf
   $ ./asdf-read

This should output::

    name: Dennis Richie
    foo: 42
    sum of squares values: 328350

Additional examples can be found in the
`libasdf documentation <https://libasdf.readthedocs.io/en/latest/usage/examples.html>`__.


.. _development:

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
