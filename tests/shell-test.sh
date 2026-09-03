#!/bin/sh

# Simple test driver for tests of the `asdf` shell command
#
# This is meant to be wrapped by test scripts for individual `asdf` sub-commands; for example:
#
# ./shell-test.sh info <list-of-input-reference-files>
#
# Running `./shell-test.sh info --upgrade <list-of-reference-files>` updates the reference files.
set -e

UPDATE=0
SUBCOMMAND=""
EXTRA_ARGS=""

# Parse arguments: allow --update before or after the subcommand
for arg in "$@"; do
  case "$arg" in
    --update)
      UPDATE=1
      shift
      ;;
    -*)
      EXTRA_ARGS="$EXTRA_ARGS $arg"
      shift
      ;;
    *)
      if [ -z "$SUBCOMMAND" ]; then
        SUBCOMMAND="$arg"
        shift
      elif [ ! -f ${arg} ]; then
        echo >&2 "error: unexpected argument: $arg"
        echo >&2 "Usage: $0 [--update] <subcommand>"
        exit 1
      fi
      ;;
  esac
done

fail=0

if [ -z "${srcdir}" ]; then
  srcdir="."
fi

if [ -z "${top_srcdir}" ]; then
  top_srcdir=".."
fi

if [ -z "${top_builddir}" ]; then
  top_builddir=".."
fi

fixtures_dir="${srcdir}/fixtures/${SUBCOMMAND}"

# Write output to the same per-run directory the unit tests use.  The helper
# joins the current run by process group; fall back to tmp/ if it is missing
# (e.g. running this script by hand from an unbuilt tree).
run_dir_prog="${top_builddir}/tests/run-dir"

if [ -x "${run_dir_prog}" ]; then
  run_dir=$("${run_dir_prog}")
else
  run_dir="$(pwd)/tmp"
fi

mkdir -p "${run_dir}"

for input in $@; do
  base=$(basename "$input" .asdf)
  expected="${fixtures_dir}/${base}.${SUBCOMMAND}.txt"
  actual="${run_dir}/${base}.${SUBCOMMAND}.out.txt"

  asdfprog="${top_builddir}/asdf"
  if [ "x${WITH_CMAKE}" != "x" ]; then
      asdfprog="${top_builddir}/src/asdf"
  fi
  # Capture stderr too; pin ASDF_LOG_LEVEL so the library's own (possibly
  # colorized) log output doesn't leak into the reference files
  ASDF_LOG_LEVEL=ERROR ${asdfprog} ${SUBCOMMAND} ${EXTRA_ARGS} "$input" > "$actual" 2>&1 || true

  if [ "$UPDATE" -eq 1 ]; then
    cp "$actual" "$expected"
    echo "🔄 Updated: $expected"
  else
    if ! diff -u "$expected" "$actual"; then
      echo "❌ Test failed: $base"
      fail=1
    else
      echo "✅ Test passed: $base"
      # Keep the output of failing tests for inspection; discard it otherwise,
      # matching the unit tests' teardown.
      if [ -z "${ASDF_TEST_KEEP_TEMP}" ]; then
        rm -f "$actual"
      fi
    fi
  fi
done

exit $fail
