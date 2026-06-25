High-level overview
###################

  What does this library actually *do*?

To answer that, it helps to understand what an ASDF file is.

At its core, an ASDF file is a human-readable **YAML document** with optional
binary data blocks attached. The YAML portion--called the
:ref:`tree <tree-in-depth>`--describes the structure and metadata of the file.
The binary blocks that may follow typically store large numerical arrays,
images, or other data that would be inefficient to represent directly in YAML.

Together, the YAML tree and the binary blocks form a single coherent data model:
the tree provides the structure and metadata, while the blocks hold the raw
data. The tree itself usually follows a particular *schema* that defines the
expected keys, value types, and overall organization.

All data in an ASDF file--whether metadata or large arrays--is accessed through
this tree. Conceptually, you can think of the file as one big nested mapping
structure (like a Python :py:obj:`dict`) containing values of many different
kinds.

The basic building blocks
=========================

A YAML document contains three main types of values:

* Mappings--collection of key/value pairs (aka hash maps, dictionaries, etc.)
* Sequences--ordered collections (aka arrays, lists, etc.)
* Scalars--simple values such as numbers, strings, or booleans

.. note::

  YAML itself does not prescribe strict types for scalar values--it treats them
  effectively as plain strings. However, the `YAML Core Schema`_ defines a
  common set of interpretations (e.g., numbers, booleans, and null), and most
  high-level languages such as Python, JavaScript, etc. that implement YAML
  parsers adhere to this convention.

  libasdf adheres to this schema as well when reading and writing scalars,
  though it is also :c:func:`possible <asdf_get_scalar>` to access values as
  raw scalars.

Beyond the core types: tags
===========================

In addition to these core YAML types, ASDF supports values that represent
complex or domain-specific objects. This is achieved using **YAML tags**, which
associate a value with a particular type definition known to the software.

Tags allow arbitrary objects--such as coordinate systems, physical units, or
n-dimensional arrays--to be serialized and deserialized in a structured way.
For example, a tag might tell libasdf that a particular mapping should be
interpreted as an ``ndarray`` instead of a plain dictionary.

.. code-block:: yaml
  :caption: A mapping tagged as an ndarray

  data: !core/ndarray-1.1.0
     source: 0
     datatype: int64
     byteorder: little
     shape: [1024, 1024]

This mechanism is similar to how YAML-based serializers in Python can store and
restore instances of custom classes.

ASDF was originally designed around a Python reference implementation, and while
the format itself is language-independent, it retains this spirit of structured
object serialization. One of the most common tagged objects is the **ndarray**,
which provides efficient storage for numerical array data--described next.

.. _ndarrays:

ndarrays (N-dimension typed arrays)
===================================

One of the most important and widely used types in ASDF is the **ndarray**.
The concept originates from `NumPy`_, the Python library for efficient
numerical computing with *n*-dimensional arrays.  In the
:external:doc:`Python ASDF implementation <asdf/arrays>`, NumPy arrays are
serialized under the tag
:ref:`core/ndarray <stsci.edu/asdf/core/ndarray-1.1.0>`.
Although the tag name and conventions come from Python, the underlying idea is
language-independent: an ``ndarray`` represents a typed, multi-dimensional
array of (typically) numerical values.

When an ``ndarray`` is stored in an ASDF file, the actual numeric data is not
written directly into the YAML document.  Instead, it is stored in a separate
**binary block**, and the ``ndarray`` node in the YAML tree contains only the
metadata needed to interpret that block.  This metadata includes information such
as the array's shape, data type, byte order, and the reference (or "source") of
the binary data.

From the point of view of the ASDF file format, a binary block is just a
contiguous sequence of bytes with no intrinsic meaning or structure.  It is
the corresponding ``ndarray`` metadata in the tree that gives those bytes their
shape and semantic content--turning them into a structured numerical array.

This separation between structure (in YAML) and data (in binary blocks) is one
of the key design principles of ASDF.  It allows the format to combine human
readability and flexibility in metadata with efficient storage and access for
large numerical datasets.

To summarize
============

So to come back to the question at the top of this page: What does libasdf
*do*?  It simply reads values of different types from (and eventually writes
to) the tree structure of an ASDF file.

In addition to standard mapping, sequence, and scalar types it also supports
the core ASDF data types as well as custom data types through an extension
mechanism allowing them to be read into C-native datastructures like
`asdf_ndarray_t`.  Additionally it includes a few convenience functions for
working with standard data types, such as for reading ndarray data by
:c:func:`tiles <asdf_ndarray_read_tile_ndim>`, with more to be added as common
use cases are discovered.
