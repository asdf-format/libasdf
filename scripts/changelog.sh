#!/bin/sh

set -eu

here=$(dirname "$0")

# Unwrap changelog snippets before passing through towncrier
# See unwrap_rst.py for more details
python3 "$here"/unwrap_rst.py changes

# Run towncrier to update changelog--this script is called automatically when
# running bumpver
towncrier build --yes
