libasdf 0.1.0a2 (2025-12-05)
============================

Bugfix
------

- Fix bug in correctly identifying the number of binary blocks when the block
  index is missing or corrupt. (`#93
  <https://github.com/asdf-format/asdf/issues/93>`_)
- Fixed segfault when passing an unknown log level name to ASDF_LOG_LEVEL


Feature
-------

- Support for reading compressed block data with zlib or bzip2.

  Includes experimental "lazy decompression" (Linux only at the moment) that
  can
  transparently decompress blocks sequentially on an as-needed basis (e.g. it's
  possible to read just the first few pages of a block without full
  decompression). (`#37 <https://github.com/asdf-format/asdf/issues/37>`_)
- Support for reading lz4 compressed block data as produced by the Python asdf.
  (`#88 <https://github.com/asdf-format/asdf/issues/88>`_)


General
-------

- Changed version scheme to follow PEP-440 particularly for pre-release tags.
- Fixed missing CMake files in release tarball.


libasdf 0.1.0-alpha1 (2025-11-12)
=================================

Misc
----

- Fixed building with CMake


libasdf 0.1.0-alpha0 (2025-11-12)
=================================

General
-------

- Preview alpha release of libasdf! Read support (only) for ASDF files; write
  support will be in a later version.

  Supports much of the ASDF 1.6.0 standard, albeit with the following key
  features still missing:

  * Reading compressed block data
  * ndarrays with inline data
  * ndarray masks
  * ndarrays with compound datatypes
  * External block data for ndarray is not supported yet

    * Some preliminary support exists for parsing compound datatypes, but no
      routines exist yet for reading records/columns out of ndarrays.

  * YAML anchors in the tree
  * Not all of the core schemas are implemented yet (complex, externalarray,
    etc.)
  * ...and likely other less common minor features.

  All of these will be addressed in future releases, likely with priority based
  on demand.


Misc
----

- This release also contains a preview of the libasdf-gwcs extension, which
  partially supports reading `GWCS <https://github.com/spacetelescope/gwcs>`_
  objects into C-native datastructures (not actually evaluating the GWCS
  transforms, however).

  This will later be moved to a separate libasdf extension plugin, but for this
  release it is included in the main library for ease of evaluation.
