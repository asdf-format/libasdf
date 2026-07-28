libasdf 0.1.0rc1 (2026-07-28)
=============================

Bugfix
------

- Detection of MD5 support on macOS (via Homebrew) is fixed, and included
  workaround for symbol clashes with macOS's libSystem built-in MD5 functions.
- Fix UFFD support (for lazy decompression) when UFFD_USER_MODE_ONLY flag is
  not defined in the kernel headers.

  This can be the case e.g. when building in Conda, where an older, more
  conservative set of headers is available.  But whether this flag actually
  works depends on the runtime kernel and has non-trivial behavior--if the flag
  exists in the kernel it must be passed in to allowed userspace fault handlers
  without extra kernel VM-level permissions set.

  Also refactored the runtime detection code a bit and handled TODO item of
  caching the result.


Documentation
-------------

- Document dependency of libmd specifically instead of libbsd more broadly.

  Most platforms seem to package libmd independently with libbsd, with libbsd,
  if it has a package at all, listing it as a dependency.  Targeting libmd
  specifically should make the documentation clearer across supported
  platforms.
- Fix the example code in ``README.rst`` just use the portable ``PRI<T>``
  macros.


Misc
----

- libfyaml parser log messages are now routed through the libasdf logging
  system.

  This allows formatting and filtering libfyaml's log messages through the same
  mechanisms--it also allows us to capture and filter certain spurious libfyaml
  log messages that occur in some cases.


libasdf 0.1.0rc0 (2026-07-24)
=============================

General
-------

- Reworked the iterator interfaces (``asdf_mapping_iter_t``,
  ``asdf_sequence_iter_t``, ``asdf_container_iter_t``, and
  ``asdf_find_iter_t``) to be simpler, more consistent, and better documented.
  (`#73 <https://github.com/asdf-format/asdf/issues/73>`_)
- The extension vtab's ``.dealloc`` method is renamed ``.deinit`` and no longer
  frees the extension object itself, only its (possibly nested) fields; a
  shallow object needs no ``.deinit`` at all.  ``asdf_<ext>_destroy()`` is
  unchanged (it de-initializes and frees the object), and a new generated
  ``asdf_<ext>_deinit()`` de-initializes an object without freeing it, for use
  with objects that are embedded in another object, stack-allocated, etc.
- `asdf_ndarray_data_raw()` is renamed to just `asdf_ndarray_data()`; the
  former still exists as well but returns raw compressed data in the case of
  compressed arrays.


Feature
-------

- Basic support for reading YAML aliases. (`#35
  <https://github.com/asdf-format/asdf/issues/35>`_)
- Extensions can now be registered for more than one tag, so a single extension
  can read multiple versions of the same schema.  ``ASDF_REGISTER_EXTENSION``
  takes one or more tags as its trailing arguments; the first tag listed is the
  one written when serializing a newly created or replaced object of that type.
  Values read from a file and left unmodified keep their original tag.

  The core ``ndarray`` extension now also reads ``ndarray-1.0.0`` in addition
  to ``ndarray-1.1.0``, and the core ``asdf`` extension reads ``asdf-1.0.0`` in
  addition to ``asdf-1.1.0``.

  As part of this the extension methods (``serialize``, ``deserialize``,
  ``copy``, and ``deinit``) moved out of ``asdf_extension_t`` and into a new
  ``asdf_extension_vtab_t``, a pointer to which is passed to
  ``ASDF_REGISTER_EXTENSION`` in place of the four individual methods.  The
  ``asdf_extension_vtab_t`` also reserves space for additional methods to be
  added in the future without breaking ABI compatibility. (`#42
  <https://github.com/asdf-format/asdf/issues/42>`_)
- Support for inline ndarray data: inline data is parsed when reading the
  ndarray data (e.g. with `asdf_ndarray_data`) and can also be set to be
  written inline with `asdf_ndarray_storage_set(ndarray,
  ASDF_ARRAY_STORAGE_INLINE)`. (`#62
  <https://github.com/asdf-format/asdf/issues/62>`_)
- Support for the ``stsci.edu/asdf/time/time`` schema via a new
  ``asdf/core/time.h`` API.  Time values can be read with ``asdf_get_time`` /
  ``asdf_value_as_time`` and written with ``asdf_set_time``, exposing an
  ``asdf_time_t`` with the original ``value`` string, the ``format`` and
  optional ``base_format``, the ``scale`` and ``location``, and a computed
  timestamp (``struct timespec`` and ``struct tm``) for the supported formats.

  Files written with any of the ``time-1.0.0`` through ``time-1.4.0`` tags are
  read; newly written time values use ``time-1.4.0``. (`#91
  <https://github.com/asdf-format/asdf/issues/91>`_)
- New ``asdf dd`` command in the command-line interface. (`#94
  <https://github.com/asdf-format/asdf/issues/94>`_)
- Basic support for writing new ASDF files from scratch. Files can be opened
  for writing with ``asdf_open(filename, "w")`` (write mode).

  New API features are being added to support adding content to files opened
  for writing, documented in some of the following changelog entries.

  This is a "write-only" mode.  Updating existing ASDF files is not yet
  supported by this feature. (`#101
  <https://github.com/asdf-format/asdf/issues/101>`_)
- ``asdf_open`` and ``asdf_open_ex`` are now macros that dispatch the correct
  file opening strategy depending on the arguments.  ``asdf_open(NULL)`` can be
  called to create a new file from scratch. (`#102
  <https://github.com/asdf-format/asdf/issues/102>`_)
- ``asdf_set_<type>`` family of functions for setting scalar values in the ASDF
  tree when open in write mode. (`#104
  <https://github.com/asdf-format/asdf/issues/104>`_)
- Optional support for binary block checksum writing on output and verification
  using ``asdf_block_checksum_verify`` (requires libbsd) (`#106
  <https://github.com/asdf-format/asdf/issues/106>`_)
- Added new types asdf_mapping_t and asdf_sequence_t; functions that
  specifically work on mappings and sequences takes these types respectively,
  rather than generic asdf_value_t.

  This may introduce slight incompatibility with previous alpha versions,
  though currently it's safe to cast an ``asdf_value_t *`` -> ``asdf_mapping_t
  *`` and vice-versa--same for sequences--so long as the value is checked to
  have the correct value type.  This should help reduce friction. (`#119
  <https://github.com/asdf-format/asdf/issues/119>`_)
- Adds previously missing ``asdf_set_value`` function for setting a generic
  ``asdf_value_t`` to a given path in the file.  Adds ``asdf_value_of_<type>``
  functions for instantiating new ``asdf_value_t`` from an existing native
  value.  For example, ``asdf_value_of_uint8(file, 123)`` wraps the integer
  ``123`` in a generic ``asdf_value_t``. (`#122
  <https://github.com/asdf-format/asdf/issues/122>`_)
- Initial support for serialization of custom extension objects.

  This updates ``ASDF_REGISTER_EXTENSION`` with a new argument for a serializer
  function that takes the extension's native object type and returns an
  ``asdf_value_t *`` for insertion into YAML tree. (`#130
  <https://github.com/asdf-format/asdf/issues/130>`_)
- New functions for creating mapping and sequence collections:

  - ``asdf_mapping_create` and `asdf_sequence_create`` functions for creating
    new mappings and sequences to add to new files.

  - ``asdf_set_mapping`` and ``asdf_set_sequence`` for inserting new
    mappings/sequences into a file.

  - ``asdf_mapping_set_<type>`` and ``asdf_sequence_append_<type>`` functions
    for appending new values into new mappings or sequences respectively.

  These allow building YAML documents containing primitive YAML types. (`#132
  <https://github.com/asdf-format/asdf/issues/132>`_)
- Support for serialization of the common core datatypes that are already
  supported by the library (including support for the new datatype-1.0.0 tag).
  (`#137 <https://github.com/asdf-format/asdf/issues/137>`_)
- ``asdf verify-checksums`` shell command for verifying binary block checksums.
  (`#145 <https://github.com/asdf-format/asdf/issues/145>`_)
- Basic support for setting compression mode on ndarrays (or raw binary blocks)
  and outputting compressed data. (`#153
  <https://github.com/asdf-format/asdf/issues/153>`_)
- The extension interface exports an ``asdf_<extension_name>_copy()`` function
  containing the necessary machinery to make a deep copy of an object returned
  by an extension, along with an ``asdf_<extension_name>_copy_into()`` variant
  that copies into a caller-provided destination (rather than a freshly
  allocated object).  This is still a work-in-progress--e.g.,
  ``asdf_ndarray_copy`` has not yet been implemented. (`#157
  <https://github.com/asdf-format/asdf/issues/157>`_)
- ndarray read improvements:

  - Added support for the float16 ndarray datatype when reading arrays and
    converting datatypes

  - Change the semantics of the ``asdf_ndarray_read_*`` functions in the case
    where converting between datatypes is not defined: previously this would
    make an effort to copy the full array into the destination buffer anyways;
    however, this proved to be too dangerous especially in cases where the
    source datatype is wider than the destination datatype, resulting in buffer
    overflows.  Rather than resort to poorly-defined and possibly erroneous
    behavior, no data is transfered and `ASDF_NDARRAY_ERR_CONVERSION` is
    returned. (`#214 <https://github.com/asdf-format/asdf/issues/214>`_)
- Added functions for reading a single ndarray element with datatype and byte
  order conversion: ``asdf_ndarray_read_at`` copies the element into a caller
  buffer, and the ``asdf_ndarray_read_<type>_at`` family returns it as a named
  C type.  For C the ``asdf_ndarray_at`` and ``asdf_ndarray_at_err`` macros
  pick the right one from the destination type.  These also read the element
  safely when the block data is not aligned for its type, which the pointer
  from ``asdf_ndarray_data`` may not be. (`#227
  <https://github.com/asdf-format/asdf/issues/227>`_)
- Added `asdf_container_size()` to return the size of any container type
  (mapping or sequence).
- Added `asdf_value_parent()` function for getting the parent value of an
  `asdf_value_t`.


Bugfix
------

- Fix bug where signed integers were sometimes parsed as unsigned integers, and
  could result in an overflow error. (`#133
  <https://github.com/asdf-format/asdf/issues/133>`_)
- Simplify parsing of floating point values in existing files to always assume
  double-precision floats; removing hacky attempts to assume intent in the
  reperesentation. (`#134 <https://github.com/asdf-format/asdf/issues/134>`_)
- Fixed ``--help`` output of the asdf CLI tool to show available sub-commands.
  (`#173 <https://github.com/asdf-format/asdf/issues/173>`_)
- Fixed minor bugs in parsing slightly pathlogical fields (empty files, padding
  between sections, etc.) (#141)
- Improved error handling when opening a non-existent/invalid file for reading.
- Miscellaneous minor memory allocation bug fixes.


Documentation
-------------

- Corrected documentation on how path expressions work. (`#124
  <https://github.com/asdf-format/asdf/issues/124>`_)
- General enhancements to the documentation:

  - Simpler, tutorial-driven examples
  - Expanded and reflowed narrative documentation
  - Expanded API documentation coverage


Misc
----

- Added new options for configuring logging for a single file, including
  logging to an alternate file and disabling specific log fields.
- Added some build fixes that improve building and running the tests in Conda
  environments.
- Re-added CMake build files to the source distribution.

  This was supposed to be fixed in 0.1.0a2 but was lost somehow in a mess-up of
  the release process.
- The experimental GWCS (Generalized World Coordinate System) extension has
  been moved to its own standalone plugin library, `libasdf-gwcs
  <https://github.com/asdf-format/libasdf-gwcs>`__.

  This release also adds new public APIs for extension authors, including
  ``ASDF_LOG()`` (via ``#include <asdf/log.h>``) for logging from extension
  code, and several other new APIs needed by libasdf-gwcs.


libasdf 0.1.0a2 (2025-12-05)
============================

General
-------

- Changed version scheme to follow PEP-440 particularly for pre-release tags.
- Fixed missing CMake files in release tarball.


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


Bugfix
------

- Fix bug in correctly identifying the number of binary blocks when the block
  index is missing or corrupt. (`#93
  <https://github.com/asdf-format/asdf/issues/93>`_)
- Fixed segfault when passing an unknown log level name to ASDF_LOG_LEVEL


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
