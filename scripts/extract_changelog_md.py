#!/usr/bin/python3
"""
Extracts the first (most recent) changelog section and converts it to markdown

Requires:

- docutils
- pypandoc
"""

import argparse
import pathlib
import sys

import docutils.core
import docutils.nodes
import pypandoc


def extract_first_section(rst_text: str) -> str:
    """
    Use docutils' doctree to extract the text of the first section

    docutils' doctree section nodes have a line-number attached, but it's actually the (1-indexed)
    line of the the section marker (e.g. =========).

    It would be signifcantly nicer if docutils could just dump the text of the section node
    directly but it doesn't seem to preserve that, so iterating through the sections and doing
    line number math seems to be the best way.
    """

    doc = docutils.core.publish_doctree(rst_text)
    sections = [node for node in doc.children if isinstance(node, docutils.nodes.section)]
    assert sections, 'no top-level sections in the file'
    lines = rst_text.splitlines()
    first_section_line = sections[0].line - 2
    assert first_section_line == 0  # Should always be the 0-th line

    if len(sections) > 1:
        next_section_line = sections[1].line - 2
    else:
        next_section_line = None

    first_section_lines = lines[first_section_line:next_section_line]
    return '\n'.join(first_section_lines)


def rst_to_md(rst_text: str) -> str:
    return pypandoc.convert_text(
        rst_text,
        'md',
        format='rst',
        extra_args=['--wrap=none']
    )


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(usage=__doc__)
    parser.add_argument('filename', type=pathlib.Path,
                        help='path to the changelog.rst file to process')
    args = parser.parse_args(argv)
    try:
        rst_text = args.filename.read_text()
        first_section = extract_first_section(rst_text)
        md = rst_to_md(first_section)
    except Exception as exc:
        print(f'error: {exc}', file=sys.stderr)
        return 1

    print(md)


if __name__ == '__main__':
    sys.exit(main())
