#!/bin/sh
#
# Regression test: libasdf.so must not export any symbols outside its own
# namespace (asdf_ prefixed).
#
# libasdf statically links a vendored copy of STC (third_party/STC).  If that
# copy is not built with -fvisibility=hidden, every STC symbol (cstr_*,
# csview_*, cspan_*, utf8_*, ...) lands in libasdf's dynamic symbol table,
# where it can be clobbered by, or clobber, the STC copy vendored by
# another library in the same process (this was found the hard way with
# libasdf-gwcs).  See third_party/Makefile.am and third_party/CMakeLists.txt.
#
# Exit 77 (automake's SKIP code) if there is nothing we can check.

if [ -z "${top_builddir}" ]; then
  top_builddir=".."
fi

# Locate the shared library; the automake and CMake builds put it in
# different places, and the extension is platform-dependent.
lib=""
for candidate in \
    "${top_builddir}"/.libs/libasdf.so \
    "${top_builddir}"/.libs/libasdf.dylib \
    "${top_builddir}"/src/libasdf.so \
    "${top_builddir}"/src/libasdf.dylib; do
  if [ -f "${candidate}" ]; then
    lib="${candidate}"
    break
  fi
done

if [ -z "${lib}" ]; then
  echo "no shared libasdf found under ${top_builddir}; skipping"
  exit 77
fi

if ! command -v nm > /dev/null 2>&1; then
  echo "nm not available; skipping"
  exit 77
fi

# Dynamic symbols defined by the library.  Fall back to plain `nm -g` for
# non-ELF platforms where `nm -D` is not supported.
symbols=$(nm -D --defined-only "${lib}" 2>/dev/null | awk '{print $NF}')

if [ -z "${symbols}" ]; then
  symbols=$(nm -g -U "${lib}" 2>/dev/null | awk '{print $NF}')
fi

if [ -z "${symbols}" ]; then
  echo "could not read symbols from ${lib}; skipping"
  exit 77
fi

# Normalize away decorations that are not part of the symbol name itself:
# - Mach-O prefixes every C symbol with an underscore
# - ASan emits __odr_asan.<name> aliases for globals
leaked=$(echo "${symbols}" \
  | sed -e 's/^__odr_asan\.//' \
  | sed -e 's/^_\(asdf_\|ASDF_\|libasdf_\)/\1/' \
  | grep -vE '^(asdf_|ASDF_|libasdf_)' \
  | sort -u)

if [ -n "${leaked}" ]; then
  echo "Test failed: ${lib} exports symbols outside the asdf_ namespace:"
  echo "${leaked}" | sed -e 's/^/    /'
  echo ""
  echo "If these are STC symbols, third_party is not being built with"
  echo "-fvisibility=hidden; see third_party/Makefile.am."
  exit 1
fi

echo "Test passed: ${lib} exports only asdf_ symbols"
exit 0
