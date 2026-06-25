.. _examples:

Additional usage examples
=========================

Miscellaneous usage examples collected from the libasdf documentation and tests, as well as from
the community.  Submissions welcome!

The following more complete example demonstrates how to read different metadata out of
the ASDF tree, as well as extract block data.  Inline comments provide further explanation:

.. code:: c
   :test: test-read-metadata-ndarray
   :fixture: cube.asdf

   #include <stdio.h>
   #include <stdlib.h>
   #include <asdf.h>
   
   int main(int argc, char **argv) {
       if (argc < 2) {
           fprintf(stderr, "Usage: %s filename\n", argv[0]);
           return 1;
       }
       const char *filename = argv[1];
   
       // The mode string "r" is required and is the only currently-supported mode
       asdf_file_t *file = asdf_open(filename, "r");
   
       if (file == NULL) {
           fprintf(stderr, "error opening the ASDF file\n");
           return 1;
       }
   
       // The simplest way to read metadata from the file is with the
       // `asdf_get_<type>*` family of functions
       // They all return a value by pointer argument and return an
       // `asdf_value_error_t`
       // For example you can read a string from the metadata like:
   
       const char *software = NULL;
       // Returns a 0-terminated string into *software.
       asdf_value_err_t err = asdf_get_string0(file, "asdf_library/author", &software);
   
       if (err == ASDF_VALUE_OK) {
           printf("Software: %s\n", software);
       }
   
       // Other errors could be e.g. ASDF_VALUE_ERR_NOT_FOUND if the key doesn't
       // exist, or ASDF_VALUE_ERR_TYPE_MISMATCH if it's not a string.
   
       // There are also extensions registered for some (not all yet) of the
       // core schemas.  Objects defined by extension schemas (identified by
       // their YAML tags) also have corresponding asdf_get_<type> functions:
       asdf_meta_t *meta = NULL;
   
       // This reads the top-level core/asdf-1.0.0 schema
       err = asdf_get_meta(file, "/", &meta);
       if (err == ASDF_VALUE_OK) {
           if (meta->history.entries && meta->history.entries[0]) {
               // This is a NULL-terminated array of asdf_history_entry_t*
               printf("First history entry: %s\n", meta->history.entries[0]->description);
           } else {
               printf("File does not contain any history entries\n");
           }
       }
   
       // Functions like `asdf_get_meta` that return into a double-pointer to a
       // struct allocate memory for that structure automatically.
       // The all have a corresponding `asdf_<type>_destroy` function.
       // The plan is to track these on the file object (issue #34) to make
       // memory management easier and cleaner, but for now you have to free
       // them manually when you're done with them. This is good practice in any
       // case.
       asdf_meta_destroy(meta);
   
       // Find the first ndarray in the file, if any
       asdf_value_t *root = asdf_get_value(file, "");
       asdf_value_t *value = asdf_value_find(root, asdf_value_is_ndarray);
   
       if (!value) {
           fprintf(stderr, "no ndarray found in the file\n");
           return 1;
       }
   
       // Generic values can be *cast* to a specific type with the
       // `asdf_value_as_` API, for example to cast to an ndarray and check that
       // the cast succeeded:
       asdf_ndarray_t *ndarray = NULL;
       err = asdf_value_as_ndarray(value, &ndarray);
       if (err != ASDF_VALUE_OK) {
           fprintf(stderr, "error reading ndarray metadata: %d\n", err);
           return 1;
       }
   
       printf("Using ndarray at: %s\n", asdf_value_path(value));
       printf("Number of data dimensions: %d\n", ndarray->ndim);
   
       // The generic value wrappers are no longer needed and should be freed.
       asdf_value_destroy(value);
       asdf_value_destroy(root);
   
       // Get just a raw pointer to the ndarray data block (if uncompressed).
       // Optionally returns the size in bytes as well
       size_t size = 0;
       const void *data = asdf_ndarray_data(ndarray, &size);
   
       if (data == NULL) {
           fprintf(stderr, "error reading ndarray data\n");
           return 1;
       }
   
       // The asdf_ndarray_read_tile_ functions copy a rectangular cutout of
       // the array into a buffer, converting datatype and endianness as needed.
       // If you don't pass your own buffer one is allocated for you; either way
       // you are responsible for freeing it.
   
       // origin and shape must have one entry per array dimension, so we size
       // them to the array we found.  Here we take a cutout of up to 5 elements
       // along each axis, clamped to the array's actual size.
       uint64_t *origin = calloc(ndarray->ndim, sizeof(uint64_t));
       uint64_t *shape = calloc(ndarray->ndim, sizeof(uint64_t));

       if (origin == NULL || shape == NULL) {
           fprintf(stderr, "out of memory\n");
           return 1;
       }
   
       uint64_t tile_nelem = 1;
       for (uint32_t dim = 0; dim < ndarray->ndim; dim++) {
           shape[dim] = ndarray->shape[dim] < 5 ? ndarray->shape[dim] : 5;
           tile_nelem *= shape[dim];
       }
   
       // Read the cutout, converting to double regardless of the source type
       double *tile = NULL;
       asdf_ndarray_err_t array_err = asdf_ndarray_read_tile_ndim(
           ndarray,
           origin,
           shape,
           ASDF_DATATYPE_FLOAT64,
           (void **)&tile
       );
   
       free(origin);
       free(shape);
   
       if (array_err != ASDF_NDARRAY_OK) {
           fprintf(stderr, "error reading ndarray: %d\n", array_err);
           return 1;
       }
   
       printf("Value at center of cutout: %g\n", tile[tile_nelem / 2]);
   
       free(tile);
       asdf_ndarray_destroy(ndarray);
       asdf_close(file);
       return 0;
    }


With libasdf installed on your system you can compile and run this test like:

.. code:: sh

   $ gcc asdf-test.c -o asdf-test -lasdf
   $ ./asdf-test tests/fixtures/cube.asdf

Which outputs::

   Software: The ASDF Developers
   First history entry: A very small data cube for testing
   Using ndarray at: /cube
   Number of data dimensions: 3
   Value at center of cutout: 222

In this case the test is run on the file
`cube.asdf <https://github.com/asdf-format/libasdf/blob/main/tests/fixtures/cube.asdf>`__
in this repository's test fixtures, though the program should work on any ASDF
file containing a non-empty ndarray.
