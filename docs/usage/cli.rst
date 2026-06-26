.. This page is the source for the asdf(1) man page (docs/man/asdf.1), which is
   generated with Sphinx but checked into the repository so that building a
   release tarball or installing the tool does not require Sphinx in the
   toolchain.  After editing this page, regenerate the committed man page by
   running ``make man-page`` from the docs build directory, then commit the
   updated docs/man/asdf.1.

.. _cli:

asdf
====

The ``asdf`` command-line tool is a companion utility installed alongside
libasdf.  It is a thin wrapper around the library offering a handful of
sub-commands for inspecting ASDF files and extracting their data.  Its
capabilities are currently modest but will be expanded in future releases.

Each tool provided by ``asdf`` is run as a separate sub-command.

Synopsis
--------

.. code:: text

   asdf COMMAND [ARGS...]

Run ``asdf COMMAND --help`` for detailed help on any sub-command.  The
available sub-commands are:

.. list-table::
   :header-rows: 1
   :widths: 25 75

   * - Command
     - Description
   * - :ref:`info <cli-info>`
     - Print a rendering of an ASDF tree
   * - :ref:`dd <cli-dd>`
     - Dump data from an ASDF binary block
   * - :ref:`events <cli-events>`
     - Print the event stream from the ASDF parser (for debugging)
   * - :ref:`verify-checksums <cli-verify-checksums>`
     - Verify binary block MD5 checksums

The global ``asdf`` command itself accepts ``-h`` / ``--help`` and ``--usage``.


.. _cli-info:

asdf info
---------

.. program:: asdf info

Print a human-readable rendering of an ASDF file's YAML tree, and optionally
information about its binary blocks.

.. code:: text

   asdf info [OPTIONS] FILENAME

.. option:: --no-tree

   Do not show the tree.

.. option:: -b, --blocks

   Show information about the file's binary blocks.

.. option:: --verify-checksums

   Verify the MD5 checksum of each block (implies :option:`-b`).

For example, to print the tree of a file together with its block information::

   asdf info --blocks observation.asdf


.. _cli-dd:

asdf dd
-------

.. program:: asdf dd

Dump the raw bytes of a single binary block to a file or to standard output,
named either by block index or by the tree path of an ndarray that references
it.  Exactly one of :option:`--block` or :option:`--ndarray` must be given.

.. code:: text

   asdf dd [OPTIONS] INPUT [OUTPUT|-]

``INPUT`` is the ASDF file to read.  ``OUTPUT`` is the destination file; a
single ``-`` writes the data to standard output.

.. option:: -b N, --block N

   Index of the block to extract (starting from ``0``).

.. option:: -n PATH, --ndarray PATH

   :ref:`Tree path <yaml-pointer>` of an ndarray whose underlying block should
   be extracted.

.. option:: -r, --raw

   For a compressed block, dump the raw compressed bytes rather than
   decompressing them first.  Has no effect on uncompressed blocks.

.. option:: -c N, --chunk-size N

   Read/write the data in chunks of ``N`` bytes.

.. option:: --no-lazy-decompression

   Disable lazy (page-fault-driven) decompression; decompress eagerly instead.
   Intended mainly for debugging.

For example, to extract the data of the ndarray at ``/data`` into a file::

   asdf dd --ndarray data observation.asdf data.bin


.. _cli-events:

asdf events
-----------

.. program:: asdf events

Print the low-level event stream produced by the ASDF parser as it walks a
file.  This is primarily a debugging aid for libasdf itself and exposes the
event-based parsing API (see :ref:`asdf`).

.. code:: text

   asdf events [OPTIONS] FILENAME

.. option:: -v, --verbose

   Show extra information about each event.

.. option:: --no-yaml

   Do not produce YAML stream events (emit only the ASDF-structural events).

.. option:: --cap-tree

   Buffer the YAML tree and print it.  This corresponds to the parser's
   tree-buffering option and is mainly useful for debugging.


.. _cli-verify-checksums:

asdf verify-checksums
---------------------

.. program:: asdf verify-checksums

Verify the MD5 checksum recorded in each binary block header against the
checksum computed from the block's data.  The exit status is non-zero if any
block fails verification.

.. code:: text

   asdf verify-checksums [OPTIONS] FILENAME

.. option:: -v, --verbose

   Print the checksum of every block, whether or not it verifies.  Without this
   option the command is quiet on success and reports only mismatches.

.. note::

   Checksum verification requires that libasdf was built with MD5 support
   (via ``libbsd``).  Without it, blocks are always reported as valid.
