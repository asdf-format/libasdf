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
   
   int main(int argc, char **argv) {
       const char *filename = "out.asdf";
       
       if (argc > 1) {
           filename = argv[1];
       }
       asdf_file_t *file = asdf_open(NULL);
       asdf_set_string0(file, "observer", "Dennis Richie");
       
       asdf_time_t time = {
           .value = "1948.78707178",
           .format = ASDF_TIME_FORMAT_JYEAR
       };
       asdf_set_time(file, "obstime", &time);
       
       asdf_ndarray_t array = {
           .ndim = 2,
           .shape = (uint64_t[]){10, 10},
           .datatype = {.type = ASDF_DATATYPE_UINT8}
       };
       uint8_t *data = asdf_ndarray_data_alloc(&array);
       
       for (int idx = 0; idx < 10 * 10; idx++)
           data[idx] = idx;
       
       asdf_set_ndarray(file, "data", &array);
       asdf_write_to(file, filename);
       asdf_ndarray_data_dealloc(&array);
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

   observer: Dennis Richie
   obstime: !time/time-1.4.0
     value: 1948.78707178
     format: jyear
   data: !core/ndarray-1.1.0
     source: 0
     datatype: uint8
     shape: [10, 10]
     byteorder: little

The next example shows how to read back in the same file:

.. code:: c
   :test: test-read-file
   :fixture: write-example.asdf

   #include <stdio.h>
   #include <stdlib.h>
   #include <asdf.h>
   
   int main(int argc, char **argv) {
       const char *filename = "out.asdf";
       
       if (argc > 1) {
           filename = argv[1];
       }
       
       asdf_file_t *file = asdf_open(filename, "r");

       if (!file) {
           fprintf(stderr, "Failed to open the file: %s\n", asdf_error(file));
           return 1;
       }

       const char *observer = NULL;
       asdf_time_t *obstime = NULL;
       asdf_ndarray_t *array = NULL;
       double *data = NULL;

       if (asdf_get_string0(file, "observer", &observer) == ASDF_VALUE_OK) {
           printf("Observer: %s\n", observer);
       }

       if (asdf_get_time(file, "obstime", &obstime) == ASDF_VALUE_OK) {
           printf("Observation time: %s (%s)\n",
                  obstime->value, asdf_time_format_string(obstime->format));
       }

       if (asdf_get_ndarray(file, "data", &array) == ASDF_VALUE_OK) {
           if (asdf_ndarray_read_all(array, ASDF_DATATYPE_FLOAT64, (void **)&data) == ASDF_NDARRAY_OK) {
               uint64_t nelem = asdf_ndarray_size(array);
               double mean = 0.0;
               for (uint64_t idx = 0; idx < nelem; idx++) {
                   mean += data[idx];
               }
               mean /= nelem;
               printf("Mean value: %.1f\n", mean);
           }
       }
       free(data);
       asdf_time_destroy(obstime);
       asdf_ndarray_destroy(array);
       asdf_close(file);
       return 0;
    }

Likewise compile and run the example with the output from the previous program:

.. code:: sh

   $ gcc asdf-read.c -o asdf-read -lasdf
   $ ./asdf-read out.asdf

This should output::

    Observer: Dennis Richie
    Observation time: 1948.78707178 (jyear)
    Mean value: 49.5

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
