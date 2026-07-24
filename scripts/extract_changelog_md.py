#!/usr/bin/python3
"""
Extracts the first (most recent) changelog section and converts it to markdown

Requires:

- docutils
- pypandoc
"""

import argparse
import contextlib
import pathlib
import sys
import tempfile

import docutils.core
import docutils.nodes
import pypandoc


TITLE_REF_FILTER = """
function Span(el)
  if el.classes:includes("title-ref") then
    return pandoc.Code(pandoc.utils.stringify(el))
  end
end
"""
"""
Render single-backtick interpreted text as a code span.

Single-backtick interpreted text, e.g. `asdf_ndarray_data`, uses whatever
``default_role`` is configured (i.e. ``c:expr`` for our Sphinx docs) but pandoc
knows nothing about that and falls back to reST's stock title-reference role,
which it writes out as ``<span class="title-ref">``.

GitHub strips the class when sanitizing a release body, so the markup silently
vanishes and the text renders bare.

This runs as a filter rather than a fixup of pandoc's output so that pandoc
still handles escaping properly (trying to run it as a regexp didn't work).
"""


@contextlib.contextmanager
def lua_filter(source: str):
    """Write a Lua filter to a temporary file and yield its path."""

    with tempfile.NamedTemporaryFile('w', suffix='.lua') as filter_file:
        filter_file.write(source)
        filter_file.flush()
        yield filter_file.name


def extract_first_section(rst_text: str) -> str:
    """
    Use docutils' doctree to extract the text of the first section

    docutils' doctree section nodes have a line-number attached, but
    it's actually the (1-indexed) line of the the section marker
    (e.g. =========).

    It would be signifcantly nicer if docutils could just dump the text of the
    section node directly but it doesn't seem to preserve that, so iterating
    through the sections and doing line number math seems to be the best way.
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
    # 'gfm' (GitHub-Flavored Markdown): the output is destined for a
    # GitHub release body, and gfm avoids gratuitous backslash-escaping of
    # quotes
    with lua_filter(TITLE_REF_FILTER) as filter_path:
        return pypandoc.convert_text(
            rst_text,
            'gfm',
            format='rst',
            extra_args=['--wrap=none', f'--lua-filter={filter_path}']
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
