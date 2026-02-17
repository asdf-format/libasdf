``asdf_open`` and ``asdf_open_ex`` are now macros that dispatch the correct file opening strategy depending on the arguments.  ``asdf_open(NULL)`` can be called to create a new file from scratch.
