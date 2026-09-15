Added ``asdf_file_find`` and ``asdf_file_find_ex`` functions, corresponding to
``asdf_value_find`` and ``asdf_value_find_ex``, the only difference being that
they take the ``asdf_file_t *`` as their first argument as a shortcut for
searching from the root of the tree.

Also adds convenience macros ``asdf_find`` and ``asdf_find_ex`` which are
generic in the first argument (can be either a file or a value).
