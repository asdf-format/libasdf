Added new types asdf_mapping_t and asdf_sequence_t; functions that specifically work on mappings and sequences takes these types respectively, rather than generic asdf_value_t.

This may introduce slight incompatibility with previous alpha versions, though currently it's safe to cast an ``asdf_value_t *`` -> ``asdf_mapping_t *`` and vice-versa--same for sequences--so long as the value is checked to have the correct value type.  This should help reduce friction.
