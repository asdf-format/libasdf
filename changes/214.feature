- Added support for the float16 ndarray datatype when reading arrays and
  converting datatypes

- Change the semantics of the ``asdf_ndarray_read_*`` functions in the case
  where converting between datatypes is not defined: previously this would
  make an effort to copy the full array into the destination buffer anyways;
  however, this proved to be too dangerous especially in cases where the
  source datatype is wider than the destination datatype, resulting in buffer
  overflows.  Rather than resort to poorly-defined and possibly erroneous
  behavior, no data is transfered and `ASDF_NDARRAY_ERR_CONVERSION` is
  returned.
