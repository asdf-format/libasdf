.. _asdf:

****************************
libasdf - The ASDF C Library
****************************

.. include:: ../README.rst
  :start-after: begin-badges:


Usage
=====

.. toctree::
  :maxdepth: 2

  usage/overview
  usage/opening
  usage/values
  usage/writing
  usage/ndarrays
  usage/compression
  usage/extensions
  usage/examples


API documentation
=================

The API documentation is organized according to the header files from which each
documented member is included.

Most of the commonly used features of libasdf can be imported simply by
including ``asdf.h``.  This in turn includes the following headers:

.. toctree::
  :maxdepth: 2

  api/asdf/file.h
  api/asdf/value.h
  api/asdf/error.h
  api/asdf/core/ndarray.h
  api/asdf/core/datatype.h

Additional less commonly used APIs can be used by including the relevant
headers.

.. toctree::
  :maxdepth: 1

  api/asdf/emitter.h
  api/asdf/extension.h
  api/asdf/yaml.h


Beyond the high-level `asdf_file_t` interface, libasdf also exposes a
lower-level, event-based API for streaming through an ASDF file without
building the whole tree in memory.  It is built around the parser in
``asdf/parser.h``, which emits a sequence of `asdf_event_t` values (defined in
``asdf/event.h``), and the corresponding emitter in ``asdf/emitter.h`` for
writing.  These are intended for advanced use cases and are not yet fully
documented; most applications should prefer the high-level API described above.


Resources
=========

.. toctree::
  :maxdepth: 2

  changes


See also
========

- The :ref:`Advanced Scientific Data Format (ASDF) standard
  <asdf-standard:asdf-standard>`.

Index
=====

* :ref:`genindex`
