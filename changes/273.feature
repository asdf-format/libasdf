Added support for the ndarray ``offset`` property.  Arrays that are views at an
offset into a larger binary block (as commonly written by earlier versions of
the Python asdf library when saving views of a base array) are now read
correctly: ``asdf_ndarray_data`` returns a pointer to the start of the array's
data, and all the ``asdf_ndarray_read_*`` functions honor the offset.  For
compressed blocks the offset is applied to the decompressed data.
