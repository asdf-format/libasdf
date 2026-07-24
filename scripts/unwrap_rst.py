#!/usr/bin/python3
"""
Unwrap hard-wrapped paragraphs in towncrier news fragments.

towncrier re-wraps its output (``wrap = true`` in towncrier.toml), but it
wraps each *input* line independently rather than reflowing whole paragraphs.
A fragment that is already hard-wrapped at some other column therefore comes
out ragged, with stub lines::

    New API features are being added to support adding content to files opened
    for
    writing, documented in some of the following changelog entries.

Running this over the fragments first, so each paragraph is a single long line,
lets towncrier do the wrapping it was going to do anyway, cleanly.

Newlines within a paragraph become spaces.  Blank lines, section underlines,
explicit markup (``.. directive::``) and indented literal blocks are all left
alone.  Hopefully that covers most of the common cases.

Lists are deliberately left alone too: towncrier indents every wrapped line of
a bullet to the paragraph indent rather than to a hanging indent.
Hand-wrapped list items already come through towncrier intact, so there is
nothing to fix there anyway.
"""

import argparse
import difflib
import pathlib
import sys

import re


LIST_ITEM_RE = re.compile(r'^\s*(?:[-*+]|\(?[0-9]+[.)]|#\.)\s')
""""Starts a new list item: "- ", "* ", "+ ", "1. ", "1) ", "(1) ", "#. """

ADORNMENT_RE = re.compile(r'^\s*([=\-~^"\'`#*+:.<>_])\1+\s*$')
"""A section title underline/overline, e.g. "====" or "----\""""

EXPLICIT_MARKUP_RE = re.compile(r'^\s*\.\.(\s|$)')
"""Explicit markup: directives, comments, hyperlink targets, footnotes"""

FIELD_RE = re.compile(r'^\s*:[^:\s][^:]*:\s')
"""A field list or definition-ish line, e.g. ":param foo:\""""


def indent_of(line: str) -> int:
    return len(line) - len(line.lstrip())


def is_boundary(line: str, prev: str) -> bool:
    """
    Should ``line`` start a new logical line rather than be joined onto
    ``prev``?
    """

    return bool(
        LIST_ITEM_RE.match(line)
        or FIELD_RE.match(line)
        or EXPLICIT_MARKUP_RE.match(line)
        or ADORNMENT_RE.match(line)
        or ADORNMENT_RE.match(prev)
    )


def unwrap_paragraph(lines: list[str]) -> list[str]:
    """
    Join the wrapped lines of a single paragraph, respecting item boundaries
    """

    logical: list[str] = []  # logical lines

    for line in lines:
        if not logical or is_boundary(line, logical[-1]):
            logical.append(line.rstrip())
        else:
            logical[-1] = logical[-1] + ' ' + line.strip()

    return logical


def paragraphs(lines: list[str]):
    """
    Yield ``(is_blank, group)`` runs of consecutive blank / non-blank lines
    """

    group: list[str] = []
    blank = None

    for line in lines:
        line_blank = not line.strip()

        if blank is None or line_blank == blank:
            group.append(line)
        else:
            yield blank, group
            group = [line]

        blank = line_blank

    if group:
        yield blank, group


def unwrap(text: str) -> str:
    """Unwrap every reflowable paragraph in an reST fragment"""

    trailing_newline = text.endswith('\n')
    result: list[str] = []
    # Indent that an ongoing literal block / directive body sits inside of,
    # or None
    literal_indent = None
    prev_indent = 0
    prev_is_literal_intro = False

    for blank, group in paragraphs(text.splitlines()):
        if blank:
            result.extend(group)
            continue

        base = min(indent_of(line) for line in group)

        if literal_indent is not None and base > literal_indent:
            # Still inside the literal block / directive body opened earlier
            verbatim = True
        elif prev_is_literal_intro and base > prev_indent:
            # Indented block introduced by a paragraph ending in "::"
            verbatim = True
            literal_indent = prev_indent
        elif EXPLICIT_MARKUP_RE.match(group[0]):
            verbatim = True
            literal_indent = base
        elif LIST_ITEM_RE.match(group[0]):
            # See the module docstring: towncrier cannot re-wrap a long list
            # item without breaking the hanging indent, so leave lists as-is
            verbatim = True
            literal_indent = None
        else:
            verbatim = False
            literal_indent = None

        result.extend(group if verbatim else unwrap_paragraph(group))
        prev_is_literal_intro = result[-1].rstrip().endswith('::')
        prev_indent = base

    out = '\n'.join(result)
    if trailing_newline and not out.endswith('\n'):
        out += '\n'
    return out


def fragment_paths(paths: list[pathlib.Path]) -> list[pathlib.Path]:
    """Expand any directories into the news fragments they contain."""

    expanded = []
    for path in paths:
        if path.is_dir():
            # Skip dotfiles such as the .gitkeep towncrier needs
            expanded.extend(sorted(
                p for p in path.iterdir()
                if p.is_file() and not p.name.startswith('.')
            ))
        else:
            expanded.append(path)
    return expanded


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('paths', type=pathlib.Path, nargs='+',
                        help='news fragments, or directories containing them')
    parser.add_argument('--diff', action='store_true',
                        help='show what would change without rewriting')
    args = parser.parse_args(argv)

    changed = 0
    for path in fragment_paths(args.paths):
        try:
            original = path.read_text()
        except OSError as exc:
            print(f'error: {exc}', file=sys.stderr)
            return 1

        unwrapped = unwrap(original)

        if unwrapped == original:
            continue

        changed += 1
        if args.diff:
            sys.stdout.writelines(difflib.unified_diff(
                original.splitlines(keepends=True),
                unwrapped.splitlines(keepends=True),
                fromfile=str(path), tofile=str(path)))
        else:
            path.write_text(unwrapped)
            print(f'- unwrapped {path}')

    if not changed:
        print('no fragments needed unwrapping')

    return 0


if __name__ == '__main__':
    sys.exit(main())
