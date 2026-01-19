Initial support for serialization of custom extension objects.

This updates ``ASDF_REGISTER_EXTENSION`` with a new argument for a
serializer function that takes the extension's native object type and
returns an ``asdf_value_t *`` for insertion into YAML tree.
