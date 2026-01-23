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
  usage/extensions


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
  api/asdf/core/ndarray.h
  api/asdf/core/datatype.h

Additional less commonly used APIs can be used by including the relevant
headers.

.. todo::

   Document lower-level APIs.

.. toctree::
  :maxdepth: 2

  api/asdf/extension.h


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
