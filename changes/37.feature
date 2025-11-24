Support for reading compressed block data with zlib or bzip2.

Includes experimental "lazy decompression" (Linux only at the moment) that can
transparently decompress blocks sequentially on an as-needed basis (e.g. it's
possible to read just the first few pages of a block without full
decompression).
