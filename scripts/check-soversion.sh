#!/bin/sh
#
# Check that a built shared libasdf carries the SONAME implied by libtool's
# -version-info, i.e. by LIBASDF_VERSION_INFO in configure.ac.
#
# See the section "Shared library versioning" in docs/development.rst for more
# details.
#
# This is only supported on ELF platforms, and is skipped otherwise
# (macOS, Windows).
#
# Usage: check-soversion.sh CURRENT:REVISION:AGE LIBDIR
#
# Exit 77 (automake's SKIP code) where there is nothing meaningful to check.

if [ $# -ne 2 ]; then
  echo "usage: $0 CURRENT:REVISION:AGE LIBDIR" >&2
  exit 2
fi

version_info=$1
libdir=$2

# Split CURRENT:REVISION:AGE.
current=$(echo "${version_info}" | cut -d: -f1)
revision=$(echo "${version_info}" | cut -d: -f2)
age=$(echo "${version_info}" | cut -d: -f3)

# Verify each field
for field in "${current}" "${revision}" "${age}"; do
  case "${field}" in
    "" | *[!0-9]*)
      echo "malformed version-info '${version_info}'; expected CURRENT:REVISION:AGE" >&2
      exit 2
      ;;
  esac
done

if [ "${age}" -gt "${current}" ]; then
  echo "malformed version-info '${version_info}': age exceeds current" >&2
  exit 2
fi

# How libtool maps the triple onto ELF SONAMEs and real file names
major=$((current - age))
expected_soname="libasdf.so.${major}"
expected_real="libasdf.so.${major}.${age}.${revision}"

if [ ! -d "${libdir}" ]; then
  echo "no such directory ${libdir}; skipping"
  exit 77
fi

# Only ELF platforms name libraries this way; macOS and Windows do not
# For now just go by filename--later check also with readelf.
if [ -z "$(find "${libdir}" -maxdepth 1 -name 'libasdf.so.*' 2>/dev/null)" ]; then
  echo "no ELF shared libasdf under ${libdir}; skipping"
  exit 77
fi

status=0
missing_real=0

if [ ! -f "${libdir}/${expected_real}" ]; then
  echo "Test failed: expected the real library to be named ${expected_real}"
  echo "  in ${libdir}, but found:"
  find "${libdir}" -maxdepth 1 -name 'libasdf.so*' | sed -e 's/^/    /'
  status=1
  missing_real=1
fi

if [ ! -e "${libdir}/${expected_soname}" ]; then
  echo "Test failed: expected a ${expected_soname} symlink in ${libdir}"
  status=1
fi

# Read DT_SONAME from the real file, when there is one to read.  A missing
# tool degrades this to a filename-only check rather than skipping the whole
# test: the names alone are enough to catch the drift this guards against,
# and skipping here would mask a failure already found above.
if [ "${missing_real}" -eq 0 ]; then
  soname=""
  if command -v readelf > /dev/null 2>&1; then
    soname=$(readelf -d "${libdir}/${expected_real}" 2>/dev/null \
      | sed -n 's/.*SONAME.*\[\(.*\)\].*/\1/p')
  elif command -v objdump > /dev/null 2>&1; then
    soname=$(objdump -p "${libdir}/${expected_real}" 2>/dev/null \
      | sed -n 's/^[[:space:]]*SONAME[[:space:]]*\(.*\)$/\1/p')
  fi

  if [ -z "${soname}" ]; then
    echo "note: could not read DT_SONAME from ${expected_real};"
    echo "  checked filenames only"
  elif [ "${soname}" != "${expected_soname}" ]; then
    echo "Test failed: ${libdir}/${expected_real} has SONAME '${soname}',"
    echo "  but LIBASDF_VERSION_INFO=${version_info} implies '${expected_soname}'"
    status=1
  fi
fi

if [ "${status}" -ne 0 ]; then
  echo ""
  echo "The two build systems describe the same interface version by"
  echo "different means, and they have drifted apart. Please reconcile them in:"
  echo ""
  echo "    configure.ac:     LIBASDF_VERSION_INFO=${version_info}"
  echo "    CMakeLists.txt:   set(PROJECT_SOVERSION ${major})"
  echo "                      set(PROJECT_LIBVERSION ${major}.${age}.${revision})"
  echo ""
  echo "See the \"Shared library versioning\" section of docs/development.rst."
  exit 1
fi

echo "Test passed: ${expected_real} has SONAME ${expected_soname}, per ${version_info}"
exit 0
