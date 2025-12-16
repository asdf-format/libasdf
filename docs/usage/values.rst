.. _values:

Reading values out of the ASDF tree
===================================

Given an :ref:`open <opening>` `asdf_file_t`, the simplest way to read a value
out of the tree and into a C-native data type is the ``asdf_get_<type>`` family
of functions, such as `asdf_get_string0` which returns a string value as a
null-terminated C string.

Each ``asdf_get_<type>`` function has a return value of `asdf_value_err_t`,
and takes at least three arguments: the `asdf_file_t *`, a *path* to the value,
and a typed pointer to where the value should be returned upon success.  For
example `asdf_get_string0` takes here a `char **`, whereas `asdf_get_int64`
takes an `int64_t *`.

Example
-------

.. code:: c

   char **author = NULL;
   asdf_value_err_t err = asdf_get_string0(file, "metadata/author", &author);

   switch (err):
      case ASDF_VALUE_OK:
         printf("the file's author is: %s\n", author);
      case ASDF_VALUE_ERR_NOT_FOUND:
         fprintf(stderr, "the expected value metadata/author was not found\n");
      case ASDF_VALUE_ERR_TYPE_MISMATCH:
         fprintf(stderr,
                 "the value metadata/author is expected to be a string\n");
      default:
         fprintf(stderr,
                 "an unknown error occurred reading metadata/author\n");
   }

   double h_index = 0.0;
   err = asdf_get_float64(file, "metadata/h_index", &h_index);
   // And so on

Other possible return values exist for `asdf_value_err_t` but these are the
most common.  `ASDF_VALUE_OK` is always returned in the case of success so it's
best to handle this case, and treat other errors as required by your
application.

If any value *other* than `ASDF_VALUE_OK` the value returned into the output
argument is undefined.


Value paths
-----------

Paths to values are always expressed using a simple `JSON Pointer`_-like
referred to informally as "YAML Pointer".

For example: ``"metadata/principle_inspectors/0/name"``.  The term before the
first ``/`` is a key in the mapping.  If the value at that key is itself a
mapping, it can then be drilled into with subsequent ``/<key>`` component
appended, much like traversing directories in a filesystem.  If the value is
a *sequence* it is also possible to address specific items in that sequence by
providing an integer as the path component.

Like in a filesystem the "root" of the ASDF tree can be addressed as ``/``,
though in all cases the leading ``/`` may be omitted.  That is, the
``asdf_get_<type>`` functions always address a value relative to the root
(we'll see in a later section how to retrieve values relative to another
value).

This is generally intuitive for common cases, but there are some details
about escaping and corner-cases to be aware of, explained next.

.. _yaml-pointer:

YAML Pointer
^^^^^^^^^^^^

The YAML Pointer syntax is the native path lookup format used by default by
`libfyaml`_, which provides libasdf's YAML parsing.  It is, however, a little
underspecified in libfyaml's documentation so we provide here a more complete
(if informal) description of the syntax.

As shown in the previous example, each path component is separated by a slash
``/``.  Each path component can be either a non-numeric string like ``xyz``,
which always indicates a key in a mapping.  Or it can be a numeric string like
``10`` in which case the meaning depends on the context:

* If the parent path element was a mapping, this is still treated as a key in
  that mapping

* If the parent path element was a sequence, this is treated as an index into
  the sequence

  * Added bonus: for sequence indices, negative values are also allowed,
    indicating reverse indexing.  For example given the document:

    .. code:: yaml

      a: [b, c]

    the path ``a/-1`` will return the value ``c`` (the last value in the
    sequence at ``a``).

Two other special bracketing syntaxes can be used to further reduce ambiguity:

* If the path element begins and ends with ``[]`` brackets, the brackets
  must contain an integer, and refers always to a sequence index (using this
  to look up a mapping key will fail).  For example: ``a/[-1]``.

* The path element may also be quoted with balanced single- or double-quotes
  like ``'a'`` or ``'0'``: ``/'a'/'0'``.  If this path were passed to the
  previous example document the lookup would fail.  ``/'a'`` succeeds because
  the root of the document is a mapping with the key ``a``.  But the subsequent
  ``/'0'`` would fail because it represents the *string* ``0`` not the integer.
  That is, requires the value at ``/'a'`` to be a mapping.

Unlike JSON Pointer, this syntax does not use ``~`` for escapes, and instead
supports backslashes.  Characters that must be escaped are ``/{}[].&*\\``.
However, escaping of these characters is not necessary if inside quotes.


Value types
-----------

libasdf supports reading scalar values from the YAML Core Schema into C-native
datatypes.  The set of support types is in `asdf_value_type_t`.

Strings
^^^^^^^

Strings can be read as null-terminated strings with `asdf_get_string0`, or
without null-termination with `asdf_get_string`, which also returns the length
of the string.

Booleans
^^^^^^^^

Boolean values can be read with `asdf_get_bool` and supports values like
"true/false" and also exceptionally treats the integers 0 and 1 as booleans
when read as bools.

Integers
^^^^^^^^

In the case of integers, libasdf supports reading into signed or unsigned sized
native integer types as defined in ``<stdint.h>``.  The largest integers
supported are up to ``UINT64_MAX``.  While YAML in principle supports
arbitrarily-sized integers, the *ASDF Standard* imposes this restriction for
interoperability purposes (but does define a separate big integer type via
tags).

If you try to read an integer into too-small of an integer type (e.g. you try
to read ``123456789`` with `asdf_get_uint8()`, or likewise if you try to read
a negative integer as an unsigned type, the respective
``asdf_get_<[u]int<n>>`` call returns `ASDF_VALUE_ERR_OVERFLOW`.

So unless you have a very specific application where some value is always
expected to be a small integer, your safest best is to just use
`asdf_get_int64`.  In fact, the general `asdf_get_int` is just an alias for
this.

Floating point numbers
^^^^^^^^^^^^^^^^^^^^^^

Floats work effectively the same as with integers, but only supports C 32-bit
floats or 64-bit doubles.  Here it is also the safest bet to just use
`asdf_get_double` unless you are expecting relatively low-precision values.

Nulls
^^^^^

There is no ``asdf_get_null`` as this would be fairly useless, but there is an
`asdf_is_null` to test if a value is null.  In YAML a value can be made
explicitly null with the scalar ``null``, or if a mapping key is only followed
by whitespace its value is also considered ``null``.

Trying to call any other ``asdf_get_<type>`` on a null value will return
`ASDF_VALUE_ERR_TYPE_MISMATCH`.

Mappings and sequences
^^^^^^^^^^^^^^^^^^^^^^

libasdf does not currently have explicit types for mappings and sequences
(though they might be added later for clarity's sake).  These are simply
represented through the generic `asdf_value_t`, discussed in the next section.

Raw scalars
^^^^^^^^^^^

Any scalar value can also be read raw (as a string) from the YAML document without
coercing it to any of the YAML Core Schema types using `asdf_get_scalar0` or
`asdf_get_scalar`.

.. _extension-types:

Extension types
^^^^^^^^^^^^^^^

libasdf also has an extension mechanism for defining custom types returned from
tagged values.  This is discussed more in the section on :ref:`extensions`.

Extension types also have automatically defined ``asdf_get_<type>`` functions
named after the extension.  Most extension values are represented as some kind
of struct.  So, similarly to `asdf_get_string0` these take a double-pointer to
where a pointer to the struct data will be written.  In most cases the
extension handles allocating memory for the struct.

For example, `asdf_get_ndarray` (which comes from the built-in
:ref:`ndarray <ndarrays>` extension) can be used like:

.. code:: c

   asdf_ndarray_t *ndarray = NULL;
   asdf_value_err_t err = asdf_get_ndarray(file, "data", &ndarray);

Upon success, ``ndarray`` will point to an initialized `asdf_ndarray_t` struct.

libasdf includes built-in extensions for many of the ASDF Standard core
schemas, with more to be added.


Generic values
--------------

Other than `asdf_file_t`, `asdf_value_t` is the most important type to know
about in libasdf.

`asdf_value_t` represents a *generic* value read out of the ASDF tree, either
from a mapping or a sequence.  This is an opaque struct that holds the value
until we try to coerce it to one of the supported C-native data types.  If we
try to get a value out of a mapping using functions like `asdf_mapping_get` or
`asdf_mapping_iter` (or likewise for sequences) it is returned as an
`asdf_value_t`.

We can then test its type using any of the ``asdf_value_is_<type>`` methods,
such as ``asdf_value_is_string``.  And we can coerce it to its respective
C-native type using ``asdf_value_as_<type>``.

Every ``asdf_get_<type>`` function has a corresponding ``asdf_value_as_<type>``
function (in fact, the former is just a wrapper around the latter).

The difference is that the ``asdf_value_as_<type>`` functions take as
arguments the `asdf_value_t` (not the file) and the output destination as a
a pointer.  Similarly, they return an `asdf_value_err_t`.

Important note about memory management
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

`asdf_value_t` are allocated automatically by functions that return them, but
it is the caller's responsibility to free them using `asdf_value_destroy`.

In typical use-cases, once a value has been read into a C-native type, the
`asdf_value_t` is no-longer needed and can be destroyed.  This does *not*
destroy the underlying C-native value, only the generic container that
wrapped it before being coerced.
