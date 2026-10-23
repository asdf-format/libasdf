"""
This file contains code for generating many of the test files in this
repository.

The files are checked in to the repository for now as they as small, though
this could also be used if we want to generate some larger test files later.

It is stored here for reference, and requires the asdf Python library, numpy,
etc.
"""

import argparse
import sys
from pathlib import Path

import asdf
import numpy as np


def make_255_asdf():
    f = asdf.AsdfFile()
    f['data'] = np.arange(256, dtype=np.dtype('>u1'))
    f.add_history_entry(
        'test file containing integers from 0 to 255 in the block '
        'data, for simple tests against known data')
    return f


def make_byteorder_asdf():
    f = asdf.AsdfFile()
    data = np.arange(8)
    f['uint16-big'] = data.astype('>u2')
    f['uint16-little'] = data.astype('<u2')
    f['uint32-big'] = data.astype('>u4')
    f['uint32-little'] = data.astype('<u4')
    f['uint64-big'] = data.astype('>u8')
    f['uint64-little'] = data.astype('<u8')
    return f


PAGE_SIZE = 4096


def make_paged_array(num_pages=100, page_size=PAGE_SIZE):
    """Each page starts with its page number, rest repeating pattern."""
    arr = np.empty(num_pages * page_size, dtype=np.uint8)
    for idx in range(num_pages):
        page_start = idx * page_size
        arr[page_start:page_start + page_size] = np.arange(page_size) % 256
        arr[page_start] = idx % 256

    return arr


def make_compressed_asdf():
    f = asdf.AsdfFile()
    f['bzp2'] = make_paged_array()
    f['zlib'] = make_paged_array()
    f['lz4'] = make_paged_array()
    f.set_array_compression(f['bzp2'], 'bzp2')
    f.set_array_compression(f['zlib'], 'zlib')
    f.set_array_compression(f['lz4'], 'lz4')
    return f


LZ4_BLOCKS_SIZE = 10 * PAGE_SIZE + 123


def make_lz4_blocks_asdf():
    """
    LZ4-compressed arrays split into many LZ4 blocks whose boundaries do not
    line up with page boundaries.

    ``small_blocks`` has blocks smaller than a page and ``large_blocks`` has
    blocks larger than a page but not a multiple of it, so decompressing one
    page (or chunk of pages) requires stitching together the end of one LZ4
    block and the start of the next.  Neither array's size is a multiple of
    the page size or of its block size, so the last page and last block are
    both partial.
    """
    f = asdf.AsdfFile()
    # Copy so that only the arrays' own data is written, not their bases
    f['small_blocks'] = make_paged_array(num_pages=11)[:LZ4_BLOCKS_SIZE].copy()
    f['large_blocks'] = make_paged_array(num_pages=11)[:LZ4_BLOCKS_SIZE].copy()
    f.set_array_compression(f['small_blocks'], 'lz4', compression_block_size=1000)
    f.set_array_compression(f['large_blocks'], 'lz4', compression_block_size=6000)
    return f


def make_cube_asdf():
    f = asdf.AsdfFile()
    f['cube'] = np.arange(10 ** 3, dtype=np.dtype('>u1')).reshape((10, 10, 10))
    f.add_history_entry('A very small data cube for testing')
    return f


def make_datatypes_asdf():
    """
    Make an ASDF file contaning an array with a complicated multi-field datatype

    Interestingly, although the ASDF format supports "tuple" datatypes like
    ``[int32, int32]`` there is no obvious way in the `asdf` Python library to
    write such a datatype as NumPy structured dtypes always have named fields
    (even if the name is an empty string).

    We don't even have any reference files for this format.

    Nevertheless, the libasdf datatype parsing treats these two cases
    equivalently, the difference is just that anonymous fields don't have
    a name.

    For now the array itself in this test file is empty; this is only for
    testing the datatype parsing.  Maybe later we change that.
    """

    f = asdf.AsdfFile()
    dt = np.dtype({
        'names': ['string', 'unicode', 'int', 'matrix'],
        'formats': ['S4', 'U4', '>i2', '(3,3)f4']
    })
    arr = np.array([], dtype=dt)
    f['structured'] = arr
    return f


# Test file with multiple binary blocks all the same size but containing different data patterns
def make_multi_block_asdf():
    f = asdf.AsdfFile()

    for n in range(1, 5):
        f[str(n)] = np.arange(0, 128, dtype=np.uint8) // n

    return f


def make_numeric_asdf():
    f = asdf.AsdfFile()

    # Map asdf datatypes to numpy dtypes; later we will also ensure
    # little-endian and big-endian variants
    datatypes = {
        'int8': np.dtype('i1'),
        'uint8': np.dtype('u1'),
        'int16': np.dtype('i2'),
        'uint16': np.dtype('u2'),
        'int32': np.dtype('i4'),
        'uint32': np.dtype('u4'),
        'int64': np.dtype('i8'),
        'uint64': np.dtype('u8'),
        'float16': np.dtype('f2'),
        'float32': np.dtype('f4'),
        'float64': np.dtype('f8')
    }

    def test_values_for_dtype(dt):
        """Test values for each dtype, -1/0, 0, 1 and boundary values"""
        if np.issubdtype(dt, np.integer):
            info = np.iinfo(dt)
            values = [
                info.min,
                -1 if info.min < 0 else 0,
                0,
                1,
                info.max,
            ]
            return np.array(values, dtype=dt)

        elif np.issubdtype(dt, np.floating):
            info = np.finfo(dt)
            values = [
                info.min, -1.0, -info.tiny, 0.0, info.tiny, 1.0, info.max,
                np.nan, np.inf, -np.inf
            ]
            return np.array(values, dtype=dt)

        else:
            raise TypeError(f"unsupported dtype {dt}")

    for datatype, dtype in datatypes.items():
        for endian in ('<', '>'):
            name = datatype + endian
            dtype = dtype.newbyteorder(endian)
            array = test_values_for_dtype(dtype)
            f[name] = array

    return f


def save_array_bases(f):
    """
    Write the base arrays of any views in the tree, rather than just the
    views' own data.

    Recent versions of the asdf library default to writing only the view's
    own data (``default_array_save_base = False``), so saving the base array
    is requested explicitly, giving the views ``offset`` and/or ``strides``.
    """
    for view in f.tree.values():
        if isinstance(view, np.ndarray):
            f.set_array_save_base(view, True)


def make_offset_asdf():
    """
    Arrays that are contiguous views into a larger base array, so that
    the asdf library writes the base array's block and gives each view an
    ``offset``.

    The compressed views use an offset a few pages into the (uncompressed)
    data, so that reading them depends on decompressing past page/chunk
    boundaries.
    """
    f = asdf.AsdfFile()
    f['u1'] = np.arange(256, dtype='u1')[16:]
    f['i4_2d'] = np.arange(100, dtype='<i4').reshape((10, 10))[3:]
    f['u2_be'] = np.arange(64, dtype='>u2')[5:]

    for comp in ('bzp2', 'zlib', 'lz4'):
        f[comp] = make_paged_array()[3 * PAGE_SIZE + 17:]
        f.set_array_compression(f[comp], comp)

    save_array_bases(f)
    return f


def make_strides_asdf():
    """
    Arrays that are non-contiguous views into a base array, so that they are
    written with ``strides``.

    The strides are in bytes, and include cases where the innermost
    dimension is not contiguous, where the inner stride is larger than the
    outer stride (Fortran order / transposes), and length-1 dimensions.
    """
    f = asdf.AsdfFile()
    f['every_other'] = np.arange(32, dtype='<u2')[::2]
    f['fortran'] = np.asfortranarray(np.arange(12, dtype='<i4').reshape((3, 4)))
    f['transpose'] = np.arange(12, dtype='u1').reshape((3, 4)).T
    f['cols'] = np.arange(20, dtype='<i4').reshape((4, 5))[:, :2]
    f['cube_perm'] = np.arange(60, dtype='>u2').reshape((3, 4, 5)).transpose(2, 0, 1)
    f['unit_dim'] = np.arange(10, dtype='u1').reshape((10, 1))[::3]
    save_array_bases(f)
    return f


def make_tiles_asdf():
    f = asdf.AsdfFile()

    # Mix of different data types for each array
    dtypes = [
        np.dtype('float64'),
        np.dtype('>u1'),
        np.dtype('<u2'),
        np.dtype('<i4')
    ]

    f['0d'] = np.array([], dtype=dtypes[0])

    for ndim in range(1, len(dtypes)):
        shape = (4,) * ndim
        idx = np.indices(shape)
        factors = 10 ** np.arange(
            len(shape) - 1, -1, -1,
        ).reshape(-1, *((1,) * len(shape)))
        array = np.sum((idx + 1) * factors, axis=0, dtype=dtypes[ndim])
        f[f'{ndim}d'] = array

    return f


TEST_FILES = {
    '255.asdf': make_255_asdf,
    'byteorder.asdf': make_byteorder_asdf,
    'compressed.asdf': make_compressed_asdf,
    'cube.asdf': make_cube_asdf,
    'datatypes.asdf': make_datatypes_asdf,
    'lz4-blocks.asdf': make_lz4_blocks_asdf,
    'multi-block.asdf': make_multi_block_asdf,
    'numeric.asdf': make_numeric_asdf,
    'offset.asdf': make_offset_asdf,
    'strides.asdf': make_strides_asdf,
    'tiles.asdf': make_tiles_asdf,
}
"""Maps the names of test files to the function that generates it"""


def main(argv=None):
    parser = argparse.ArgumentParser(description='Generate test fixture files')
    parser.add_argument('filename', nargs='?', type=str,
                        help='The name of the test file to generate; if '
                             'omitted lists the names of the file this script '
                             'can generate')
    parser.add_argument('--fixtures-dir', type=Path,
                        default=Path('../fixtures'),
                        help='Path to the test fixtures directory')
    args = parser.parse_args(argv)

    if not args.filename:
        for filename in TEST_FILES:
            print(filename)

        return 0

    if args.filename not in TEST_FILES:
        parser.error(f'unknown test file: {args.filename}')

    asdf_file = TEST_FILES[args.filename]()
    asdf_file.write_to(args.fixtures_dir / args.filename)
    return 0


if __name__ == '__main__':
    sys.exit(main())
