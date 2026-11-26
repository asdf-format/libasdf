.. _libasdf-for-fits-developers:

libasdf for FITS developers
===========================

This page is for developers who already know how to read FITS files from C or
C++, typically with `CFITSIO`_ or `CCfits`_, and who now need their software
to read ASDF files too.

It is a companion to the general :ref:`ASDF for FITS users
<asdf-website:asdf_fits>` page on the ASDF website, which explains the
conceptual differences between the two formats (data models, file structure,
data types, versioning).  Please read that page first if you are not familiar
with ASDF principles in general; this page focuses more on the code and
software engineering side of the equation: how familiar CFITSIO calls map onto
libasdf, and, more importantly, how the *design* of an application that reads
ASDF files differs from one that reads FITS files.

Code examples are given in C.  CCfits users should find the mapping
straightforward, since CCfits is itself a thin object-oriented layer over the
same FITS concepts.

Several of the recommendations here are lessons learned from two real-world
efforts to add ASDF support (through libasdf) to established FITS-based
astronomy software: work in progress on `SourceXtractor++`_, and a prototype
of ASDF support for `SAOImage DS9`_.  Neither is released as of this writing,
so details of their user interfaces may still change; they are cited here to
provide examples of the design problems they had to solve rather than as
finished references.

.. todo::

   Do make sure to update the above document once updates to those programs
   with ASDF support *are* released.


A different mental model
------------------------

A FITS file is a sequence of HDUs.  Each HDU has a header of keyword
"cards"[#]_ and (optionally) one array or table.  Because this structure is
fixed by the FITS standard itself, a program can, to an extent answer the
question "what images are in this file?" by looking at the file alone: walk the
HDUs, and the IMAGE HDUs with ``NAXIS >= 2`` are the images\ [#]_.  Most FITS
files make this even simpler by having a single image in the primary HDU or the
first extension.

An ASDF file instead contains a *tree* (a YAML document) of arbitrarily
nested mappings, sequences, and scalars, some of which may be tagged objects
such as :ref:`ndarrays <ndarrays>`.  There may be zero, one, or hundreds of
ndarrays anywhere in that tree.  Whether a given ndarray is "the science
image", its error array, a data-quality mask, a coefficient matrix inside a
WCS transform, or a column of a table is not something the file format says.
That meaning comes from the *data model* (schema) the file was written to,
which is typically defined per mission, observatory, or project--for example
the `Roman Attribute Dictionary`_ for the Nancy Grace Roman Space Telescope.
libasdf reads any ASDF file but, by design, does not interpret any such
mission-specific data model\ [#]_.

This leaves application authors with two broad cases:

* **Mission- or instrument-specific software** can be written against a known
  data model, and can simply hardcode (or read from configuration) the paths
  where it expects to find its data, e.g. ``/roman/data`` and
  ``/roman/meta/exposure``.

* **General-purpose software** such as image viewers or source extraction
  tools cannot assume any particular data model.  It must give users a way
  to say *where* in the tree the data they want lives, and should help them
  discover what is there.

Most of this page is about that second case, starting with the design
questions it raises.


.. _fits-finding-things:

Finding things in the tree: designing general-purpose tools
------------------------------------------------------------

This is where the biggest change of mindset is needed.  FITS-reading tools have
often gotten away with "open the file and read the first image".  A
general-purpose ASDF-reading tool needs to answer, often with the user's help,
three questions that are usually just assumed with FITS:

1. Which array(s) in the file should be operated on?
2. Where is the metadata (exposure time, gain, saturation level, ...)
   associated with those arrays (esp. for images)?
3. Which coordinate system (WCS), if any, applies to each array?

The rest of this section collects patterns for answering these questions.


.. _fits-naming-arrays:

Letting users name an array
^^^^^^^^^^^^^^^^^^^^^^^^^^^

Whatever the user interface, the natural way for users to identify an array
is by its path in the tree, such as ``/roman/data``.  With libasdf these
paths can be passed straight through to functions like `asdf_get_ndarray` or
`asdf_get_value`; see :ref:`values` and :ref:`yaml-pointer` for the path
syntax.

.. note::

  Spelling out the leading ``/`` is optional to libasdf, but it can
  help make the path visually distinct.

For command-line interfaces and configuration files, a few conventions have
been used in practice:

* **A suffix on the filename**, in the spirit of CFITSIO's IRAF-style extended
  filename syntax (``image.fits[SCI]``, ``image.fits[1]``):

  * Square brackets, ``image.asdf[/roman/data]``, as used by
    SourceXtractor++.  This has the advantage of looking just like the FITS
    syntax users already know, so ``image.fits[SCI]`` and
    ``image.asdf[/roman/data]`` can be accepted by the same code path.

  * A colon separator, ``image.asdf:roman/data``, as used by the DS9
    prototype (following an earlier ASDF prototype for DS9).  If you use this
    form, take care with Windows drive letters (``C:\...``); the DS9
    prototype only splits at the last colon if what precedes it names an
    existing file.

* **Separate options** that say where to look, independent of the filename.
  SourceXtractor++, for example, adds CLI options such as
  ``--detection-image-name``, ``--weight-image-name`` and
  ``--detection-image-wcs-name``, with equivalents in its Python
  configuration API.  These accept either a FITS ``EXTNAME``\ [#]_ or an ASDF
  tree path, so a single option serves both formats; an explicit filename
  suffix, if given, takes precedence.  SourceXtractor++ supports the same
  options in its configuration file format.  This is especially useful for
  users working regularly with data from the same source.  For example, users
  working primarily with Roman data sets can set::

    detection-image-name = /roman/data

  once in their default configuration file, and that image path will be reused
  across files.


  .. note::

    On naming such options: these were originally called ``--*-path``, but
    "path" was too easily read as a *filesystem* path, while "key" would
    suggest a FITS header keyword.  "Name" turned out to be the least
    surprising term for something that can be either an ``EXTNAME`` or a tree
    path.  Other naming ideas are welcome.


If you also accept integers for ASDF files (e.g. ``image.asdf[0]`` for "the
first candidate image", see :ref:`fits-discovering-arrays`), be careful to keep
that meaning distinct from FITS's.  In FITS ``[N]`` conventionally means "the
HDU at absolute index N" (including non-image HDUs), which is not the same
thing as "the Nth image".  FITS, furthermore, has always assumed 1-based
indexing when using this format.  Generalizing FITS code to multiple formats
can silently change which of the two an integer means; SourceXtractor++ hit
exactly this regression and resolved it by keeping "format-native index" and
"Nth image" as separate operations internally.


.. _fits-discovering-arrays:

Discovering candidate arrays
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To show users what they can choose from--in an error message, a ``--list``
option, or a GUI--enumerate the ndarrays in the tree with
`asdf_find_iter_init` and the ``asdf_value_is_ndarray`` predicate, and report
each one's path with `asdf_value_path`.  Then filter the results to arrays
your application can actually use (see :ref:`fits-arrays` below for more on
the `asdf_ndarray_t` structure).  For an imaging application, for example:

.. code:: c

   #include <inttypes.h>
   #include <stdio.h>
   #include <asdf.h>

   static void list_images(asdf_file_t *file) {
       asdf_value_t *root = asdf_get_value(file, "");
       asdf_find_iter_t *iter = asdf_find_iter_init(root, asdf_value_is_ndarray);

       while (asdf_value_find_iter_next(&iter)) {
           asdf_ndarray_t *ndarray = NULL;

           if (asdf_value_as_ndarray(iter->value, &ndarray) != ASDF_VALUE_OK)
               continue;

           /* Only 2-D images or 3-D cubes whose data is in a binary block;
            * this logic could also be folded into a custom predicate function
            * for asdf_find_iter_init */
           if (ndarray->ndim >= 2 && ndarray->ndim <= 3 &&
               asdf_ndarray_storage(ndarray) == ASDF_ARRAY_STORAGE_INTERNAL) {
               printf("%s  %s  [", asdf_value_path(iter->value),
                      asdf_scalar_datatype_to_string(ndarray->datatype.type));

               for (uint32_t idx = 0; idx < ndarray->ndim; idx++)
                   printf(idx ? ", %" PRIu64 : "%" PRIu64, ndarray->shape[idx]);

               puts("]");
           }

           asdf_ndarray_destroy(ndarray);
       }

       asdf_value_destroy(root);
   }

Run on a small ASDF file converted from a multi-extension FITS file, this
prints something like::

   /IMAGE  float64  [1, 1]
   /IMAGE2  float64  [1, 1]

(the converted primary HDU and binary table are filtered out).  A real
filter would likely also check that the datatype is one the application
supports.

Some lessons learned about enumeration:

* **Expect many arrays that are not images.**  A Roman calibrated image, for
  example contains dozens of ndarrays: the science array, its error and
  data-quality arrays, other ancillary arrays, and many small coefficient
  matrices nested deep inside the WCS transform.  A naïve "use every 2-D array"
  policy is wrong; in one early SourceXtractor++ test it ran detection on an
  ancillary bitmask array and reported tens of thousands of spurious "sources".

* **Breadth-first order is usually what you want.**  `asdf_find_iter_init`
  traverses `breadth-first`_, which tends to put top-level data arrays (e.g.
  ``/roman/data``) before arrays buried in nested structures.  A depth-first
  walk of a Roman file can return dozens of table columns and WCS matrices
  first.  For finer control, `asdf_find_iter_init_ex` accepts a
  ``descend_pred`` that can skip whole subtrees, such as WCS objects or tables,
  and a maximum depth.

* **Sort and filter for presentation.**  Sorting candidates by size (largest
  first) pushes small auxiliary arrays to the bottom of a list.

* **YAML aliases can repeat arrays.**  If the same ndarray is referenced from
  more than one place in the tree via a YAML anchor and alias, the iterator
  yields it once per reference, each with its own path.  If you need each
  underlying array only once, de-duplicate (e.g. by the
  :c:member:`source <asdf_ndarray_t.source>` block index of
  internally-stored arrays).

  .. note::

    libasdf still lacks sufficient APIs for checking whether or not a given
    value was resolved from an alias.  This will be improved in a future
    version.


Refuse to guess
^^^^^^^^^^^^^^^

FITS-centric software commonly falls back on "the primary HDU, or else the
first image extension" when the user does not say which image to use.  The
closest ASDF equivalent, "the first ndarray in the tree", is far less likely to
be right.  A pattern that has worked well:

* If, after filtering, there is exactly **one** candidate array, use it
  without requiring the user to name it.

* If there are **several**, do not pick one.  Fail with an error that lists
  the candidates and tells the user how to choose--for example, in the
  style of SourceXtractor++::

     The ASDF file image.asdf holds 7 arrays that could be images, and
     nothing in the file says which one is meant.  Name it explicitly,
     either as image.asdf[<name>] or with the corresponding
     --*-image-name option, choosing from:
         [0]  /roman/data
         [1]  /roman/context
         ...

* Optionally, an application that is commonly used with a particular data
  model may document a default path for it (the DS9 prototype defaults to
  ``/roman/data``, for example), as long as it is explicit and overridable.


Graphical interfaces
^^^^^^^^^^^^^^^^^^^^

GUI applications can present the result of the enumeration above as a tree
view mirroring the structure of the ASDF tree, showing only the branches that
lead to arrays the application can load, with columns for shape and
datatype.  If only one array is loadable it can be selected automatically.
This is the approach taken by the DS9 prototype's array picker.

.. figure:: /images/fits/ds9-1.png
   :alt: sample DS9 array picker for ASDF files

   Prototype image array picker shown when opening an ASDF file (in this case
   a Roman WFI L2 calibration file) in DS9.

Similarly, the FITS "header viewer" found in many tools does not translate
directly to ASDF; a collapsible view of the YAML tree (or simply the
tree's raw text) is a more natural replacement.

.. figure:: /images/fits/ds9-2.png
   :alt: sample DS9 header viewer for ASDF files

   Prototype DS9 header viewer for the same ASDF file as above.


.. _fits-associating-metadata:

Associating metadata and WCS with an array
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In FITS, the metadata and WCS for an image live in the same HDU header as
the image itself.  In ASDF there is no format-level link between an array and
"its" metadata or WCS; that relationship, too, is defined by the data model.
In the Roman data model, for example, ``/roman/data`` and ``/roman/meta/wcs``
are siblings by convention, but nothing in ASDF itself says that the WCS
applies to that particular array--and other arrays in the same file (such as
reference-pixel arrays) have different shapes and are *not* described by it.

Strategies that have been used:

* **Let the user say explicitly.**  SourceXtractor++'s ``--*-wcs-name``
  options take the path to a WCS object, or a list of ``array:wcs`` pairs
  mapping specific arrays to specific WCSs.

* **Fall back to a search, with a warning.**  When no WCS is named, using
  the first WCS object found in the tree (e.g. with `asdf_value_find`) is a
  reasonable default, as long as the user is told which one was chosen.

* **Sanity-check before trusting it.**  The DS9 prototype attaches a WCS to
  an array only if the array's shape matches the WCS's pixel grid.
  Otherwise it would display coordinates for, say, a reference-pixel array
  that look plausible but are wrong.

Reading ASDF's generalized WCS (GWCS) objects is outside the scope of libasdf
itself, but is provided by the separate `libasdf-gwcs`_ extension, which
defines an `asdf_value_is_gwcs` predicate suitable for use with
`asdf_value_find` and `asdf_find_iter_init`.

The same principle applies to individual metadata values that FITS software
traditionally reads from well-known keywords such as ``GAIN``, ``SATURATE``
or ``EXPTIME``.  Rather than looking those keyword names up in the ASDF tree
(where they will not exist), let such configuration values be given either
as a literal value *or* as a path to a value in the tree, e.g.
``--gain=/path/to/gain`` versus ``--gain=4.5``.


.. _fits-metadata:

Reading metadata: keywords versus tree values
---------------------------------------------

In CFITSIO, metadata is read from header keywords in the current HDU:

.. code:: c

   fitsfile *fptr = NULL;
   int status = 0;
   double exptime = 0.0;

   fits_open_file(&fptr, "image.fits", READONLY, &status);
   fits_read_key(fptr, TDOUBLE, "EXPTIME", &exptime, NULL, &status);

   if (status)
       fits_report_error(stderr, status);

In libasdf, metadata is any scalar value in the tree, addressed by its path
(see :ref:`values` and :ref:`yaml-pointer`).  The equivalent of the above,
for a file following the Roman data model, is:

.. code:: c

   asdf_file_t *file = asdf_open("image.asdf", "r");
   double exptime = 0.0;
   asdf_value_err_t err = asdf_get_double(
       file, "roman/meta/exposure/exposure_time", &exptime);

   if (err != ASDF_VALUE_OK)
       fprintf(stderr, "could not read exposure_time (%s)\n", asdf_error(file));

Some differences to keep in mind:

* **Keys are paths, not keywords.**  There is no 8-character limit, no
  ``HIERARCH`` convention, and no ``CONTINUE`` cards for long strings.  The
  price is that there is also no flat namespace: the same concept may live at
  different paths in files written to different data models.  Never look up
  a FITS keyword name like ``"GAIN"`` in an ASDF tree and expect to find it;
  see :ref:`fits-associating-metadata`.

* **Types come from the YAML Core Schema**, not from the syntax of a header
  card.  Each ``asdf_get_<type>`` function checks that the value is
  compatible with the requested type and returns
  `ASDF_VALUE_ERR_TYPE_MISMATCH` or `ASDF_VALUE_ERR_OVERFLOW` rather than
  silently converting.  This is stricter than CFITSIO, which will happily
  convert between numeric types and even parse numbers out of strings.

* **Do not probe types by trial and error.**  For example, `asdf_get_bool`
  deliberately accepts the integers ``0`` and ``1``, so code that tries
  ``asdf_value_as_bool``, then ``asdf_value_as_int64``, and so on, will
  misreport small integers as booleans.  If you need to probe for a value's
  type, ask for it with `asdf_value_get_type` and dispatch on the result.

* **Metadata can be structured.**  Values may be nested mappings and
  sequences, or tagged objects (e.g. times, via :ref:`extension types
  <extension-types>`) that have no direct FITS equivalent.

* **History is not a keyword.**  FITS ``HISTORY`` and ``COMMENT`` cards have
  no direct equivalent; ASDF files carry structured ``history`` entries under
  the root of the tree, and YAML comments are not part of the data at all.

There is no fixed list of "header keywords" to iterate over, but an
analogue of a ``fits_read_keyn`` loop is to walk the tree and print every
plain scalar along with its path.  This example skips the contents of
ndarrays and other tagged objects:

.. code:: c

   static bool is_plain_scalar(asdf_value_t *value) {
       return asdf_value_is_scalar(value) &&
              asdf_value_get_type(value) != ASDF_VALUE_EXTENSION;
   }

   static bool not_ndarray(asdf_value_t *value) {
       return !asdf_value_is_ndarray(value);
   }

   static void dump_metadata(asdf_file_t *file) {
       asdf_value_t *root = asdf_get_value(file, "");
       asdf_find_iter_t *iter = asdf_find_iter_init_ex(
           root, is_plain_scalar, ASDF_DEPTH_FIRST, not_ndarray, -1);

       while (asdf_value_find_iter_next(&iter)) {
           const char *raw = NULL;

           if (asdf_value_as_scalar0(iter->value, &raw) == ASDF_VALUE_OK)
               printf("%s = %s\n", asdf_value_path(iter->value), raw);
       }

       asdf_value_destroy(root);
   }

In practice, the :ref:`asdf info <cli-info>` command-line tool is a quick way
to look at a file's tree, much as one would use ``fitsheader`` or ``fv`` on a
FITS file.  Alternatively, by design, most text editors will open ASDF files
just fine.  Some, with the right configuration, can even provide syntax
highlighting of the YAML portion, ignoring the binary portions of the file.


.. _fits-arrays:

Reading array data
------------------

Reading an image with CFITSIO typically looks something like this:

.. code:: c

   int bitpix = 0, naxis = 0;
   long naxes[2] = {0, 0};
   long fpixel[2] = {1, 1};

   fits_get_img_param(fptr, 2, &bitpix, &naxis, naxes, &status);

   float *pixels = malloc(naxes[0] * naxes[1] * sizeof(float));
   fits_read_pix(fptr, TFLOAT, fpixel, naxes[0] * naxes[1], NULL, pixels,
                 NULL, &status);

   /* pixel (x, y), 0-based, is pixels[y * naxes[0] + x] */

The libasdf equivalent, given the path to the array (see
:ref:`ndarray-reading`), may be done like:

.. code:: c

   asdf_ndarray_t *ndarray = NULL;
   float *pixels = NULL;

   if (asdf_get_ndarray(file, "roman/data", &ndarray) != ASDF_VALUE_OK) {
       fprintf(stderr, "no ndarray at /roman/data\n");
       return 1;
   }

   if (asdf_ndarray_read_all(ndarray, ASDF_DATATYPE_FLOAT32,
                             (void **)&pixels) != ASDF_NDARRAY_OK) {
       fprintf(stderr, "error reading /roman/data\n");
       asdf_ndarray_destroy(ndarray);
       return 1;
   }

   /* pixel (x, y), 0-based, is pixels[y * width + x] */

   asdf_free(pixels);
   asdf_ndarray_destroy(ndarray);

Things that differ from FITS:

* **Axis order.**  FITS lists axes fastest-varying first: ``NAXIS1`` is the
  length of a row.  ASDF (like C, Python, NumPy, etc.) lists them
  slowest-varying first, so ``NAXIS1`` corresponds to ``shape[ndim - 1]`` and
  ``NAXIS2`` to ``shape[ndim - 2]``.  The memory layout of a C-contiguous 2-D
  array is the same in both cases; only the order in which the dimensions are
  *listed* is reversed.  Both projects mentioned above had to account for this
  wherever FITS-shaped code consumed array dimensions.

* **0-based indexing.**  CFITSIO pixel coordinates (``fpixel``, ``lpixel``)
  are 1-based and inclusive.  The origins passed to
  `asdf_ndarray_read_tile_2d` and `asdf_ndarray_read_tile_ndim` are 0-based,
  and tiles are given by an origin and a size.

* **Datatypes.**  FITS has a small, fixed set of ``BITPIX`` values, and
  represents unsigned integers with the ``BZERO`` offset convention.  ASDF
  ndarrays may use any of the types in `asdf_scalar_datatype_t`, including
  native unsigned integers, 8-bit signed integers, and ``float16``.  As with
  CFITSIO, you can ask libasdf to convert to the type your code works with by
  passing it as the target datatype (or pass `ASDF_DATATYPE_SOURCE` to keep
  the stored type; see :ref:`ndarray-read-convert`).  If your software's
  internals are built around the ``BITPIX`` types, decide on an explicit
  policy for the rest--for example, the DS9 prototype widens ``float16`` to
  ``float32``, ``int8`` to ``int16`` and ``uint32`` to ``int64``, and refuses
  ``uint64`` rather than silently losing precision.

* **Byte order.**  FITS data is always stored big-endian.  ASDF arrays may be
  stored in either byte order; libasdf's ndarray read functions always return
  data in the host system's native byte order.

* **No BSCALE or BZERO, and no BLANK as such.**  There is no convention for
  scaled integers: an ndarray's datatype is simply the type of its values.
  For missing values, the ``ndarray-1.1.0`` schema does define an optional
  ``mask`` property--either a scalar value marking missing elements, much
  like ``BLANK``, or a boolean mask array--but in practice it is little used,
  and its use is not encouraged\ [#]_.  More often, data-quality arrays and
  fill-value conventions are part of the data model (for example a separate
  data-quality array next to the science array), and an application has to
  know (or be told) where to find them.

* **Compression is transparent.**  ASDF binary blocks may be compressed
  (see :ref:`compression`), but this is invisible to the read functions,
  including tile reads; there is no separate API like CFITSIO's tiled image
  compression routines.

* **Not every ndarray has its data in a binary block.**  Small arrays may be
  stored inline in the YAML tree, and the ASDF standard also allows external
  arrays in other files.  `asdf_ndarray_storage` reports which kind you
  have; see :ref:`ndarray-storage-modes`.

.. note::

   This page only covers plain numeric arrays, the analogue of FITS IMAGE
   HDUs.  The ASDF analogue of a FITS binary table is an ndarray with a
   *structured* (record) datatype, or a table object built from several
   column arrays.  libasdf can parse structured datatypes but cannot yet fully
   read their data (see the note in :ref:`working-with-ndarrays`), so there is
   not yet an equivalent of ``fits_read_col``.  This is planned for a future
   release.

.. todo::

   Add a comparison of reading FITS binary table columns with
   ``fits_read_col`` versus reading structured ndarrays once libasdf fully
   supports reading structured datatypes.


.. _fits-quick-reference:

CFITSIO to libasdf at a glance
------------------------------

For quick reference, the following table is a rough "Rosetta stone" mapping
common CFITSIO operations to their closest libasdf equivalents.  The mapping
is not always one to one; the notes and the preceding sections explain the
differences.

.. list-table::
   :header-rows: 1
   :widths: 30 35 35

   * - CFITSIO
     - libasdf
     - Notes
   * - ``fits_open_file`` / ``fits_close_file``
     - `asdf_open` / `asdf_close`
     - See :ref:`opening`.
   * - ``int *status`` argument; ``fits_report_error``
     - Per-call return codes (`asdf_value_err_t`, `asdf_ndarray_err_t`);
       `asdf_error` for the message
     - There is no "inherited status": each call reports its own result.
       See :ref:`error-handling`.
   * - ``fits_movabs_hdu`` / ``fits_movnam_hdu``
     - `asdf_get_value` with a tree path
     - There is no "current HDU"; every lookup takes a path from the root.
   * - ``fits_get_num_hdus`` + ``fits_get_hdu_type``
     - `asdf_find_iter_init` with ``asdf_value_is_ndarray``
     - Enumerates ndarrays anywhere in the tree; see
       :ref:`fits-discovering-arrays`.
   * - ``fits_read_key`` with ``TSTRING``, ``TLONG``, ``TDOUBLE``,
       ``TLOGICAL``
     - `asdf_get_string0`, `asdf_get_int64`, `asdf_get_double`,
       `asdf_get_bool`
     - Keys are tree paths, not 8-character keywords.  See
       :ref:`fits-metadata`.
   * - ``fits_read_card`` / ``fits_read_keyword`` (unparsed value)
     - `asdf_get_scalar0`
     - Returns the raw scalar text without type coercion.
   * - ``fits_get_hdrspace`` + ``fits_read_keyn`` loop
     - `asdf_find_iter_init_ex` with ``asdf_value_is_scalar``, plus
       `asdf_value_path`
     - See the example in :ref:`fits-metadata`.
   * - ``fits_get_img_param`` (``BITPIX``, ``NAXIS``, ``NAXISn``)
     - The ``datatype``, ``ndim``, and ``shape`` fields of `asdf_ndarray_t`
     - Note the reversed axis order; see :ref:`fits-arrays`.
   * - ``fits_read_pix`` / ``fits_read_img`` with a target datatype
     - `asdf_ndarray_read_all` with a target `asdf_scalar_datatype_t`
     - Both convert to the requested type on read.
   * - ``fits_read_subset``
     - `asdf_ndarray_read_tile_2d` / `asdf_ndarray_read_tile_ndim`
     - libasdf tile origins are 0-based.
   * - Extended filename syntax, e.g. ``image.fits[SCI]``
     - No library-level equivalent
     - Applications choose their own convention; see
       :ref:`fits-naming-arrays`.
   * - ``fits_read_col`` (binary tables)
     - Planned, but not yet supported
     - See the note in :ref:`fits-arrays`.


.. _fits-data-model-vs-format:

Appendix: separating the data model from the file format
--------------------------------------------------------

This appendix is a short, opinionated recommendation for how to design
software around file formats, based on the author's experience with FITS and
ASDF, among other formats.

For nearly fifty years FITS has been *the* file format of astronomy, and that
ubiquity has been an enormous benefit: one format to support, one reference
library, and data that remains readable decades after it was written.  But it
has also had a quieter cost.  Because FITS was the only format most
astronomy software ever needed to read, a great deal of that software was
designed directly around FITS's data model: images are HDUs, metadata is a
flat list of keywords of at most eight characters, pixel types are
``BITPIX`` values, and missing data is ``BLANK``.  Those assumptions, among
others, are spread throughout our community's code--both library and
application code, as well as countless thousands of lines of individual
researchers' code--not just in the parts that read files.

None of this is a criticism of FITS, which has served the community
remarkably well (though it does have well-documented limitations; see
`Thomas et al. 2015`_ and `Greenfield et al. 2015`_).  The point is that when
software's internals mirror one file format's data model, supporting any
other format--HDF5, ASDF, or whatever comes next--requires untangling those
assumptions first.  That makes new formats expensive to adopt, which in turn
reinforces the dominance of the existing one.  The same coupling also leaks
into user-facing behavior: users writing data out often need to know FITS's
keyword and datatype restrictions in order to use the software at all.
Researchers sometimes end up spending time dealing with the particularities
of FITS that would be better spent on science.

We suggest designing the core of data-processing software around a more
abstract model--roughly, "a file contains one or more named arrays, plus
metadata, plus (in the case of science data) optionally a coordinate system
representation for each array"--with format-specific adapters at the edges that
translate FITS HDUs and keywords, or ASDF tree paths and ndarrays, into that
model.  The specifics of this model are debateable.  And this does not mean
pretending all file formats are the same: format-specific options, defaults and
code paths are sometimes unavoidable, as the previous sections show.  But it
keeps those differences at the edges.

Adding ASDF support to SourceXtractor++ illustrates both the cost and the
payoff.  Most of the work was not reading ASDF at all, but introducing
format-neutral abstractions for "a file containing images", "how the user
selects an image", "image metadata" and "coordinate system", and moving
FITS-specific behavior (FITS headers, FITS WCS serialization) behind them.
Several helpers originally written specifically for ASDF turned out to work
unchanged on FITS files once those abstractions were in place.  Having done
that work once, we believe that supporting a third file format in the future
should require significantly less work.

.. rubric:: Footnotes

.. [#] A seemingly unusual term by modern standards, originating from the fact
   that FITS metadata--keyword/value pairs--were designed to fit on 80 column
   punch cards. This is also the origin for why FITS keyword and value length
   are strictly restricted.

.. [#] This is a slight oversimplification.  Even an ``IMAGE`` HDU in FITS is
   not necessarily an "image", semantically.  HST MEF files, for example, have
   at least three ``IMAGE`` HDUs distinguished by their ``EXTNAME``, though in
   a standard order: ``SCI``, ``ERR``, ``DQ``.  Only ``SCI`` is an actual
   "image" with ``ERR`` giving pixel uncertainty weights and ``DQ`` giving
   additional data quality flags.  This, just as any ASDF schema, is a
   *convention*.  But it is very commonly assumed and typically easier to suss
   out that either the primary HDU or the first image HDU is "the image".

.. [#] Indeed, nothing about ASDF's core format is specific to astronomy at
   all.  The same can be said about FITS's basic on-disk layout--a sequence of
   headers and data units--which could in principle hold almost any array
   data.  But the FITS *standard* is unmistakably an astronomy standard: it is
   maintained by the IAU FITS Working Group, and defines reserved keywords with
   specifically astronomical meanings such as ``TELESCOP``, ``OBSERVER``,
   ``EQUINOX`` and ``DATE-OBS``, along with the world coordinate system
   conventions (``CTYPEn``, ``CRVALn``, ...) that most astronomy software
   relies on.  The ASDF standard, by contrast, defines only the core file
   format and a small set of general-purpose schemas (ndarrays, software and
   history metadata, and so on), leaving domain-specific data models,
   including astronomical ones, to separately maintained extensions.

   Accordingly, ASDF has found users outside of astronomy, such as the
   `WelDX`_ project for welding research data, which defines its own data
   model and schemas on top of ASDF.  Notably, WelDX's user documentation is
   written almost entirely in terms of that data model--measurements,
   coordinate systems, welding processes--with ASDF itself appearing mainly in
   its API reference.  The file format is the foundation, but not the center of
   attention, in contrast to the central place FITS has long occupied in
   astronomy software.  This is exactly in line with ASDF's design philosophy.

.. [#] An opinion, offered in passing: Use of ``EXTNAME`` is often
   under-emphasized when teaching astronomers how to work with FITS files.
   Where a FITS file provides extension names, prefer addressing HDUs by name
   (``image.fits[SCI]``) over numeric index (``image.fits[1]``), in your own
   tools and in their documentation.  Names survive reordering of HDUs, they
   say what the data *is*, and they map naturally onto ASDF tree paths.  Of
   course not all FITS files set ``EXTNAME``, so numeric indices remain
   necessary as a fallback.

.. [#] The ``mask`` property of
   `ndarray-1.1.0 <https://www.asdf-format.org/projects/asdf-standard/en/latest/generated/stsci.edu/asdf/core/ndarray-1.1.0.html>`__
   allows either a number, used as the value of missing elements, or a
   ``bool8`` ndarray (broadcastable to the array's shape) in which non-zero
   elements mark missing values.  Parts of this definition are unclear or
   overly restrictive--for example, it leaves no valid way to give a
   per-field mask for an array with a structured datatype (see
   `asdf#1909 <https://github.com/asdf-format/asdf/issues/1909>`__)--and its
   use is not encouraged.  libasdf does not yet support the ``mask`` property,
   though support is planned (see
   `libasdf#48 <https://github.com/asdf-format/libasdf/issues/48>`__).  Until
   then, applications that need it can read the ``mask`` value directly from
   the tree at ``<path to array>/mask``, as the DS9 prototype does.
