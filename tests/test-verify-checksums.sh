#!/bin/sh

if [ -z "${srcdir}" ]; then
  srcdir="."
fi

if [ -z "${top_srcdir}" ]; then
  top_srcdir=".."
fi

"${srcdir}"/shell-test.sh verify-checksums --verbose $@ \
  "${srcdir}"/fixtures/255-2-blocks.asdf \
  "${srcdir}"/fixtures/255-invalid-checksum.asdf
