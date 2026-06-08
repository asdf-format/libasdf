#!/bin/sh

if [ -z "${srcdir}" ]; then
  srcdir="."
fi

if [ -z "${top_srcdir}" ]; then
  top_srcdir=".."
fi

"${srcdir}"/shell-test.sh info --blocks $@ \
  "${top_srcdir}"/asdf-standard/reference_files/1.6.0/*.asdf \
  "${srcdir}"/fixtures/roman_l2_wcs.asdf
