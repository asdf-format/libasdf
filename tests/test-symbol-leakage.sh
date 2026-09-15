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

# Ignore decorations and symbols that are not libasdf's to begin with:
#
# - ASan emits various aliases for globals depending on the compiler version:
#   - __odr_asan.<name>
#   - __odr_asan_gen_<name>
#   - __start_asan_globals, __stop_asan_globals
#
# - Linker- and CRT-generated symbols (the section boundary markers the GNU
#   linker provides, plus _init/_fini from crti.o/crtn.o).  These are not
#   emitted by libasdf at all, and every shared object on the platform may
#   define its own, so they cannot collide in the way this test exists to
#   catch.  Current binutils and glibc keep them hidden, which is why this
#   never fires locally; the older toolchains used by conda-forge and by
#   Homebrew on Linux export them with default visibility.  Both carried
#   linker version-script workarounds to get past this test before the
#   exception below was added: conda-forge for _init/_fini, Homebrew for
#   __bss_start/_edata/_end.
#
# - Mach-O prefixes every C symbol with an underscore, hence the optional
#   leading _ in the asdf_ pattern below
leaked=$(echo "${symbols}" \
  | sed -e '/^__odr_asan/d' \
  | grep -vE '^__(start|stop)_asan_globals' \
  | grep -vxE '_init|_fini|_etext|_edata|_end|__bss_start|__data_start|data_start' \
  | grep -vE '^_?(asdf_|ASDF_|libasdf_)' \
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
