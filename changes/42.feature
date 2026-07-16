Extensions can now be registered for more than one tag, so a single extension
can read multiple versions of the same schema.  ``ASDF_REGISTER_EXTENSION``
takes one or more tags as its trailing arguments; the first tag listed is the
one written when serializing a newly created or replaced object of that type.
Values read from a file and left unmodified keep their original tag.

The core ``ndarray`` extension now also reads ``ndarray-1.0.0`` in addition to
``ndarray-1.1.0``, and the core ``asdf`` extension reads ``asdf-1.0.0`` in
addition to ``asdf-1.1.0``.

As part of this the extension methods (``serialize``, ``deserialize``,
``copy``, and ``deinit``) moved out of ``asdf_extension_t`` and into a new
``asdf_extension_vtab_t``, a pointer to which is passed to
``ASDF_REGISTER_EXTENSION`` in place of the four individual methods.  The
``asdf_extension_vtab_t`` also reserves space for additional methods to be added
in the future without breaking ABI compatibility.
