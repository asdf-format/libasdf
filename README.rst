libasdf
#######

.. _begin-badges:

.. image:: https://github.com/asdf-format/libasdf/workflows/Build/badge.svg
    :target: https://github.com/asdf-format/libasdf/actions
    :alt: CI Status

.. image:: https://app.readthedocs.org/projects/libasdf/badge/?version=latest
    :target: https://libasdf.readthedocs.io/en/latest/
    :alt: Documentation Status

.. image:: https://anaconda.org/conda-forge/libasdf/badges/version.svg
    :target: https://anaconda.org/channels/conda-forge/packages/libasdf
    :alt: Conda Package

.. image:: https://img.shields.io/badge/dynamic/regex?url=https%3A%2F%2Fraw.githubusercontent.com%2Fasdf-format%2Fhomebrew-tap%2Fmain%2FFormula%2Flibasdf.rb&search=releases%2Fdownload%2F%28%5B0-9%5D%5B0-9a-zA-Z.%5D%2A%29%2F&replace=%241&label=Homebrew%20Tap&color=orange
    :target: https://github.com/asdf-format/homebrew-tap
    :alt: Homebrew Tap

.. _end-badges:

A C library for reading (and eventually writing) `ASDF
<https://www.asdf-format.org/en/latest/>`__ files.


Introduction
============

libasdf is largely a wrapper around `libfyaml
<https://pantoniou.github.io/libfyaml/>`__ but with an understanding of the
structure of ASDF files, with the capability to read and extract binary block
data, as well as typed getters for metadata in the ASDF tree.

It also features an extension mechanism for reading ASDF schemas, including the
core schemas such as ``core/ndarray-<x.y.z>`` into C-native datastructures.

libasdf additionally installs a companion command-line tool, ``asdf``: a
wrapper around the library providing utilities for inspecting and extracting
data from ASDF files.  Its capabilities are currently modest but will be
expanded in the future; see the `command-line tool documentation
<https://libasdf.readthedocs.io/en/latest/usage/cli.html>`__ for details.


.. _installation:

Installation
============

libasdf is packaged for conda and Homebrew.  Both install the shared library,
the public headers, and the ``asdf`` command-line tool.  It can also be built
from source.  If you would rather see what using the library looks like first,
skip ahead to `Getting started`_.

conda
-----

libasdf is available from `conda-forge
<https://anaconda.org/conda-forge/libasdf>`__::

    conda install -c conda-forge libasdf

Packages are built for Linux and macOS; there is currently no Windows build.

Homebrew
--------

libasdf is distributed through the asdf-format `tap
<https://github.com/asdf-format/homebrew-tap>`__::

    brew tap asdf-format/tap
    brew install libasdf

or, equivalently, in a single step::

    brew install asdf-format/tap/libasdf

Bottles (pre-built binaries) are provided for Apple Silicon macOS and x86-64
Linux.  On Intel macOS the formula builds from source, which takes
considerably longer.

.. note::

   The formula declares ``conflicts_with "asdf"``: libasdf's command-line tool
   and the `asdf <https://asdf-vm.com/>`__ version manager both install a
   binary named ``asdf``, so Homebrew will not link the two at once.

From source
-----------

libasdf's build system is built with CMake.  To build from a release tarball
or from a git checkout, you'll need the following software installed on your
system:

Requirements
^^^^^^^^^^^^

- **CMake** (for generating the build system)
- **C compiler** (e.g., ``gcc`` or ``clang``)
- **Make** (e.g., ``GNU make``)
- **pkg-config**
- **libfyaml**
  - Version >=0.8 is tested to work
- **zlib**, **bzip2**, and **lz4** (for compression support)
- **libmd** (required for MD5 checksum support)
- **libstatgrab** (optional, for system resource heuristics)
- **argp** (this is a feature of glibc, but if compiling with a different libc you need a
  standalone version of this; also it is only needed if building the command-line tool)

On **Debian/Ubuntu**::

    sudo apt install build-essential pkg-config libfyaml-dev \
      zlib1g-dev libbz2-dev liblz4-dev libstatgrab-dev libmd-dev

On **Fedora**::

    sudo dnf install gcc make pkgconf libfyaml-devel \
      zlib-devel bzip2-devel lz4-devel libstatgrab-devel libmd-devel

On **macOS** (with Homebrew)::

    brew install pkg-config libfyaml argp-standalone \
      zlib bzip2 lz4 libstatgrab libmd

Building
^^^^^^^^

Clone the repository and build the project as follows (if you are building
from a release tarball, unpack it and skip the ``git clone``)::

    git clone https://github.com/asdf-format/libasdf.git
    cd libasdf
    mkdir build
    cd build
    cmake .. \
        -D ENABLE_TESTING=[YES/NO] \
        -D ENABLE_TESTING_SHELL=[YES/NO] \
        -D ENABLE_TOOL=[YES/NO] \
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

If doing a system install, as usual it's recommended to install to
``/usr/local`` by providing ``-DCMAKE_INSTALL_PREFIX=/usr/local`` when running
``cmake``.  Or, if you have a ``${HOME}/.local`` you can set the prefix there,
etc.

Logging
^^^^^^^

libasdf can emit diagnostic log messages, controlled by the following options:

- ``-D ENABLE_LOG=[YES/NO]`` -- compile libasdf's internal log statements into
  the library (default ``YES``).  When ``NO`` they compile to nothing.
- ``-D ENABLE_LOG_COLOR=[YES/NO]`` -- colorize log output (default ``YES``).
- ``-D LOG_DEFAULT=LEVEL`` -- the default runtime log level, used when none is
  set explicitly; one of ``TRACE``, ``DEBUG``, ``INFO``, ``WARN`` (the
  default), ``ERROR``, ``FATAL``, or ``NONE``.
- ``-D LOG_MIN=LEVEL`` -- the compile-time minimum level; messages below it are
  compiled out entirely (default ``TRACE``).

At runtime the default level can also be overridden through the
``ASDF_LOG_LEVEL`` environment variable.  See the
`logging documentation <https://libasdf.readthedocs.io/en/latest/usage/opening.html#logging>`__
for details.

Notes
^^^^^

- Run ``make clean`` to clean build artifacts.
- Run ``ctest --output-on-failure`` to execute unit tests


Getting started
===============

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

       if (argc > 1)
           filename = argv[1];
   
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

With libasdf installed on your system (see `Installation`_) you can compile
and run this test like:

.. code:: console

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

   #include <inttypes.h>
   #include <stdio.h>
   #include <stdlib.h>
   #include <asdf.h>
   
   int main(int argc, char **argv) {
       const char *filename = "out.asdf";

       if (argc > 1)
           filename = argv[1];
   
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
           printf("foo: %" PRId64 "\n", foo);
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
               printf("sum of squares values: %" PRIu64 "\n", sum);
           }
       }
   
       // clean up allocations
       free(squares_data);
       asdf_ndarray_destroy(squares);
       asdf_close(file);
       return 0;
   }

Likewise compile and run the example with the output from the previous program:

.. code:: console

   $ gcc asdf-read.c -o asdf-read -lasdf
   $ ./asdf-read

This should output::

    name: Dennis Richie
    foo: 42
    sum of squares values: 328350

Additional examples can be found in the
`libasdf documentation <https://libasdf.readthedocs.io/en/latest/usage/examples.html>`__.


Versioning and stability
========================

libasdf follows `semantic versioning <https://semver.org/>`__, and is
currently in the ``0.x`` series.

**API stability.**  Source compatibility is maintained between ``0.x``
releases: code that compiles against one release compiles against the next.

**ABI stability.**  The shared library carries a SONAME (``libasdf.so.0``)
derived from an interface version that moves independently of the package
version.  It changes only when an interface is removed or altered, never when
interfaces are merely added, so a binary linked against one release keeps
working with later releases that only add to the API.  This is guaranteed for
64-bit targets; 32-bit ABI compatibility is not promised.  See `shared library
versioning
<https://libasdf.readthedocs.io/en/latest/development.html#abi-versioning>`__
for how this is managed.

**Road to 1.0.**  Version 1.0 aims for at least complete *read* support for
every feature of the ASDF standard and the core schemas.  libasdf already
supports reading *most* ASDF files one is likely to encounter in the wild.


Official Extensions
===================

- `libasdf-gwcs <https://github.com/asdf-format/libasdf-gwcs>`__ — GWCS (Generalized
  World Coordinate System) extension for reading ASDF files containing WCS transforms
  and coordinate frames.
