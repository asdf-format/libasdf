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


def make_compressed_asdf():
    num_pages = 100
    page_size = 4096
    total_size = num_pages * page_size

    def make_array():
        """Each page starts with its page number, rest repeating pattern."""
        arr = np.empty(total_size, dtype=np.uint8)
        for idx in range(num_pages):
            page_start = idx * page_size
            arr[page_start:page_start + page_size] = np.arange(page_size) % 256
            arr[page_start] = idx % 256

        return arr

    f = asdf.AsdfFile()
    f['bzp2'] = make_array()
    f['zlib'] = make_array()
    f.set_array_compression(f['bzp2'], 'bzp2')
    f.set_array_compression(f['zlib'], 'zlib')
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
    f['record'] = arr
    return f


def make_numeric():
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
    'numeric.asdf': make_numeric,
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
