.. _extensions:

Extending libasdf with extension types
======================================

.. note::

   The extension mechanism in libasdf is not fully stable and subject to
   change, thought it is already possible to write third-party extensions.

The extension mechanism allows providing custom code for handling tagged
values in the ASDF tree, and converting it to a user-defined custom data
structure.

When reading a tagged value out of the tree, such as

.. code:: yaml

  %YAML 1.1
  %TAG ! tag:stsci.edu:asdf/
  --- !core/asdf-1.0.0
  data: !core/ndarray-1.1.0
    source: 0
    datatype: float64
    shape: [1024, 1024]
  ...

libasdf will read the tag.  If there is an extension registered under that
tag, libasdf will recognize it not just as a *mapping* (in this case) but
as an instance of some ``ndarray`` type.  Each extension type comes with
corresponding ``asdf_is_<type>``, ``asdf_get_<type>``, ``asdf_value_is_<type>``
and ``asdf_value_as_<type>`` functions, as well as ``asdf_<type>_destroy``.

For example, `asdf_get_ndarray` will return the above value as an
`asdf_ndarray_t` struct.  Trying to call this on any value *not* tagged as
``tag:stsci.edu/asdf/core/ndarray-1.1.0`` will result in
`ASDF_VALUE_ERR_TYPE_MISMATCH`, even if it happens to match the same
structure.  The tagged value cannot be read implicitly without the actual
tag in the YAML file.

.. note::

   There is some open discussion about allowing implicit type conversion as
   well, if the value can be successfully deserialized.  Though this is
   generally againt the spirit of ASDF and YAML, where the semantic meaning
   carried by tags is considered important.

libasdf includes builtin extension types for many of the core ASDF types,
including:

* :ref:`core/asdf <stsci.edu/asdf/core/asdf-1.1.0>`

  .. note::

    This schema represents the standard top-level metadata in most ASDF files.
    In libasdf the type representing this is called ``asdf_meta_t``, however, in
    order to avoid having some ``asdf_asdf_t``.

* :ref:`core/extension_metadata <stsci.edu/asdf/core/extension_metadata-1.0.0>`
* :ref:`core/history_entry <stsci.edu/asdf/core/history_entry-1.0.0>`
* :ref:`core/ndarray <stsci.edu/asdf/core/ndarray-1.1.0>`
* :ref:`core/software <stsci.edu/asdf/core/software-1.0.0>`

with more to be added.


Loading extensions from third-party libraries
---------------------------------------------

Extension types can be declared in stand-alone shared libraries--applications
that link a shared library defining the extension type (as well as libasdf
itself, which is needed anyways to declare the extension) will automatically
register that extension when the library is loaded.

libasdf does not formally have an interface for loading extensions from some
location as "plugins".  For the time being this is not strictly necessary,
because extension types are currently only useful in application code that
explicitly needs to read extension types (e.g. uses ``asdf_get_<type>`` for
some extension type).  So application code that uses an extension type must be
linked at build time to use the extension code.

That said, there may be use cases in the future for runtime linking, such as
allowing different extension plugins to provide these interfaces (e.g. a custom
plugin for handling ndarrays).  It may also become more useful when write
support is added.  So this may be added in a future version.


Writing an extension type
-------------------------

Here is a brief example of how to write and register an extension type.

This assumes we have some tag called ``ext/foo-1.0.0`` that can be applied to
scalars (though it could be on any other YAML type).  In this trivial example
we wrap the value tagged as a "foo" in a struct called ``asdf_foo_t``.  In a
more practical case, even with scalars, the scalar might be parsed somehow and
represented as some more complex structure:

.. code:: yaml

  #ASDF 1.0.0
  #ASDF_STANDARD 1.6.0
  %YAML 1.1
  %TAG ! tag:stsci.edu:asdf/
  --- !core/asdf-1.1.0
  foo: !tests/foo-1.0.0 foo
  ...

.. todo::

   The ``core/complex`` tag might be a useful example to point to here, but we
   haven't implemented it yet.

For this example we need to write a few pieces of code:

* A struct representing for our "foo" type (an extension object may also be
  any simple scalar as well, but it most cases it will be some struct)

* An ``asdf_software_t`` instance declaring metadata about the software
  providing the extension,

* An ``asdf_foo_deserialize`` function (this can be
  named anything, however; it does not have to use the ``asdf_`` prefix either,
  likewise for all the other ``foo`` methods)

* An ``asdf_foo_serialize`` function

* An ``asdf_foo_dealloc`` function

* An ``asdf_foo_copy`` function


.. code:: c

   typedef struct {
       const char *foo;
   } asdf_foo_t;


   static asdf_software_t asdf_foo_software = {
       .name = "foo",
       .author = "STScI",
       .homepage = "https://stsci.edu",
       .version = "1.0.0"
   };

The ``asdf_foo_deserialize`` function will be passed the raw value (as an
`asdf_value_t`) to which the tag was applied.  It also takes an optional
``void *userdata`` that can be used by the extension (this is not currently
used anywhere), and a `void **` to which it should return an ``asdf_foo_t *``
or ``NULL`` if parsing the value failed:

.. code:: c

  static asdf_value_err_t asdf_foo_deserialize(asdf_value_t *value, const void *userdata, void **out) {
      const char **foo_val = NULL;
      asdf_value_err_t err = asdf_value_as_string(value, &foo_val);

      if (ASDF_VALUE_OK != err)
          return err;

      asdf_foo_t *foo = malloc(sizeof(asdf_foo_t));

      if (!foo) {
          return ASDF_VALUE_ERR_OOM;
      }
      foo->foo = strdup(*foo_val);
      *out = foo;
      return ASDF_VALUE_OK;
  }


The ``asdf_foo_serialize`` function is used to serialize an ``asdf_foo_t``
struct back to the YAML tree.  It is passed the `asdf_file_t *` handle of the
file being written (this is required to build new values to put into the tree),
a `void *` to the extension value (an ``asdf_foo_t *`` in this case), and
the optional ``void *userdata`` from the extension).

If the serialization is successful it should return an `asdf_value_t *` to the
generic YAML value generated from the extension value:

.. code:: c

  static asdf_value_t *asdf_foo_serialize(asdf_file_t *file, const void *obj, const void *userdata) {
      if (!obj)
          return NULL;

      const asdf_foo_t *foo = obj;
      /* The "foo" extension reads a string tagged 'foo' from the file and adds the
       * prefix "foo:" to it.  That's all it is.  So if we receive an asdf_foo_t
       * it must store a string prefixed with "foo:"; when serializing it
       * as a string with the "foo:" prefix again removed */
      if (!foo->foo)
          return NULL;

      size_t prefix_len = strlen(foo_prefix);
      size_t len = strlen(foo->foo);

      if (len < prefix_len)
          return NULL;

      return asdf_value_of_string(file, foo->foo + prefix_len, len - prefix_len);
  }


The ``asdf_foo_dealloc`` function must be defined to free any memory allocated
for the ``asdf_foo_t`` structure and any data it contains.  It is passed a
`void *` to ``asdf_foo_t`` object:

.. code:: c

  static void asdf_foo_dealloc(void *value) {
      asdf_foo_t *foo = value;
      if (foo && foo->foo) {
          free((void *)foo->foo);
          foo->foo = NULL;
      }
      free(foo);
  }


And the ``asdf_foo_copy`` function is used to implement a user-level API
that will be generated by the extension called ``asdf_foo_clone``.  This is
needed for most extension values that contain nested data, so that a deep
copy of the object can be created correctly:

.. code:: c

  static void *asdf_foo_copy(const void *value) {
      if (!value)
          return NULL;

      const asdf_foo_t *foo = value;
      asdf_foo_t *copy = calloc(1, sizeof(asdf_foo_t));

      if (!copy)
          goto failure;

      if (foo->foo) {
          copy->foo = strdup(foo->foo);

          if (!copy->foo)
              goto failure;
      }

      return copy;
  failure:
      asdf_foo_dealloc(copy);
      return NULL;
  }


Finally, we register the extension by making the following call in the same
source file as where these functions were defined (or in a different file if
the functions have external linkage):

.. code:: c

  ASDF_REGISTER_EXTENSION(
      foo,
      "stsci.edu:asdf/ext/foo-1.0.0",
      asdf_foo_t,
      &asdf_foo_software,
      asdf_foo_serialize,
      asdf_foo_deserialize,
      asdf_foo_copy,
      asdf_foo_dealloc,
      NULL
  )

This is a macro which currently takes 9 arguments:

* The base name of the extension type--this is not necessarily the name of the
  C type returned by the extension (though it could be the same).  This defines
  the type name used in the generated ``asdf_get_<type>`` and related functions.
  For example, this defines ``asdf_get_foo``, ``asdf_is_foo``, and so on.

* The tag for which the extension should be registered.  Currently this only
  supports a single tag, though there are plans to change that, as in many
  cases the same extension code can support multiple tag versions.

  It is, however, perfectly possible to register multiple extensions under
  different tags but using the same ``asdf_foo_*`` functions.

* The C-native type returned by the extension--this is our ``asdf_foo_t``.

* An ``asdf_software_t *`` for the software metadata.

* The serialize function we defined

* The deserialize function we defined

* The clone function we defined

* The dealloc function we defined

* A pointer to optional userdata stored by the extension (this is not used yet
  but could be supplied, e.g. at runtime, to configure the extension).

Finally, if we wish to make our extension usable by external code, we provide
the following declaration in our ``foo.h`` header:

.. code:: c

   ASDF_DECLARE_EXTENSION(foo, asdf_foo_t)


Our extension type can now be used in code like:

.. code:: c

  asdf_file_t *file = asdf_open(path, "r");
  asdf_foo_t *foo = NULL;
  asdf_value_err_t err = asdf_get_foo(file, "foo", &foo);

  if (ASDF_VALUE_OK == err)
      printf("the foo: %s\n", foo->foo);
  else
      fprintf(stderr, "invalid foo value");
