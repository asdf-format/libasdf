Basic support for writing new ASDF files from scratch.
Files can be opened for writing with ``asdf_open(filename, "w")`` (write mode).

New API features are being added to support adding content to files opened for
writing, documented in some of the following changelog entries.

This is a "write-only" mode.  Updating existing ASDF files is not yet supported
by this feature.
