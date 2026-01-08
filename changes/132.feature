New functions for creating mapping and sequence collections:

- ``asdf_mapping_create` and `asdf_sequence_create`` functions for creating
  new mappings and sequences to add to new files.

- ``asdf_set_mapping`` and ``asdf_set_sequence`` for inserting new
  mappings/sequences into a file.

- ``asdf_mapping_set_<type>`` and ``asdf_sequence_append_<type>`` functions
  for appending new values into new mappings or sequences respectively.

These allow building YAML documents containing primitive YAML types.
