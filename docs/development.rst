.. _development-resources:

Development resources
#####################

This page covers building libasdf from a git checkout, the conventions the
project follows, and how releases are made.  If you only want to *use* the
library, the build instructions in the README *should* be sufficient.


.. _build-systems:

The two build systems
=====================

libasdf ships two parallel build systems, and both are kept working at all
times:

**Autotools** (``configure.ac`` / ``Makefile.am``) is the primary build system
for development, and the one used to produce release tarballs.  ``make dist``
and ``make distcheck`` give a canonical, self-contained source archive that
builds without any of the tooling used to generate it--no autotools, no
Sphinx, no git checkout.  ``make distcheck`` also verifies that the tarball
builds *out of tree*, that everything needed was actually distributed, and that
it uninstalls cleanly, which is what makes it trustworthy as a release
artifact.

**CMake** (``CMakeLists.txt``) exists for consumers, not for producing
releases.  A great deal of C and C++ tooling assumes CMake: it drops into
projects that use ``FetchContent`` or ``find_package``, into meta-build systems
and package managers, and into IDEs that understand CMake projects natively.
Requiring downstream users to deal with autotools in those settings would be a
real obstacle.

The two are not independent: ``make distcheck`` runs the CMake build *from the
distribution tarball* (the ``distcheck-cmake`` target in the top-level
``Makefile.am``), so a change that breaks CMake, or that forgets to distribute
a file CMake needs, fails the autotools release check.  Both are also built and
tested in CI, by the ``Build`` and ``CMake Build`` workflows respectively.

The practical consequence: **when you add a source file, add it to both build
systems.**

CMake is generally more permissive and picks up most new source files
automatically, whereas automake tends to require anything you want built to be
listed explicitly.


Building with autotools
=======================

A git checkout has no ``configure`` script; generate it first with
``autogen.sh`` (a one-line wrapper around ``autoreconf --install``).  This is
normally only needed once, or again after editing ``configure.ac`` or any
``Makefile.am``, though the generated makefiles normally re-run the necessary
steps by themselves:

.. code:: console

    $ git clone --recurse-submodules https://github.com/asdf-format/libasdf.git
    $ cd libasdf
    $ ./autogen.sh
    $ ./configure
    $ make
    $ make check

The ``asdf-standard`` submodule provides reference files used by the test
suite; if you cloned without ``--recurse-submodules``, run ``git submodule
update --init`` before building (this is also run automatically by the
build system, but sometimes it can be flaky).

Useful ``configure`` options:

``--enable-debug``
    Debug build: ``CFLAGS=-g -O0``, and ``DEBUG`` is defined.

``--with-asan``
    Build with AddressSanitizer.

``--with-ubsan``
    Build with UndefinedBehaviorSanitizer.

``--disable-tool``
    Skip building the ``asdf`` command-line tool.  Useful if ``argp`` is
    unavailable.

``--enable-docs``
    Build the Sphinx documentation.  The default is ``auto``: docs are built if
    Sphinx and its extensions are found, and silently skipped otherwise.

``--enable-valgrind``
    Enable the ``check-valgrind`` targets.

``--enable-logging``, ``--enable-log-color``, ``--with-log-default=LEVEL``, ``--with-log-min=LEVEL``
    Compile-time logging configuration; see :ref:`logging`.

Autotools fully supports out-of-tree builds, and keeping several configurations
side by side is the recommended way to work, since a change should pass under
more than one of them:

.. code:: console

    $ mkdir build-asan && cd build-asan
    $ ../configure --with-asan
    $ make && make check

Anything that touches library code should pass ``make check`` in both an
AddressSanitizer build and a debug build before being committed.


Building with CMake
===================

The CMake build is documented in the README and is reproduced here for
convenience:

.. code:: console

    $ mkdir build && cd build
    $ cmake .. \
          -D CMAKE_BUILD_TYPE=RelWithDebInfo \
          -D ENABLE_TESTING=YES \
          -D ENABLE_ASAN=[YES/NO] \
          -D ENABLE_TOOL=[YES/NO]
    $ make
    $ ctest --output-on-failure

Other options of note:

``-D ENABLE_TESTING_ALL=YES``
    Enable every test target, including the shell-based integration tests.
    This is what CI uses.

``-D ENABLE_DOCS=YES``
    Build the Sphinx documentation (``make docs``).

``-D FYAML_NO_PKGCONFIG=YES``, ``-D ARGP_NO_PKGCONFIG=YES``
    Locate libfyaml or argp by explicit path rather than ``pkg-config``; pair
    with ``-D FYAML_LIBDIR=``/``-D FYAML_INCLUDEDIR=`` and the ``ARGP_``
    equivalents.

``make package_source`` and ``make package`` produce CPack archives.  Note that
these are *not* what is published for a release--the release tarball comes
from ``make dist`` under autotools.

.. note::

    On systems with a libasdf already installed under, say, ``~/.local/lib``,
    be aware that CMake links test binaries with ``DT_RUNPATH`` rather than
    ``DT_RPATH``, and the dynamic loader searches ``LD_LIBRARY_PATH`` *before*
    ``DT_RUNPATH``.  An installed copy can therefore shadow the freshly built
    one and cause confusing test failures.  Unset ``LD_LIBRARY_PATH`` before
    running ``ctest`` if you hit this.


Running the tests
=================

The test suite uses the `µnit <https://nemequ.github.io/munit/>`__ framework,
with some custom wrappers around it (helper macros) defined in
``tests/munit.h``.

Test binaries are named ``test-<name>.unit`` and live in the ``tests/``
directory of the build tree.  Some of them compile a subset of the sources
directly, rather than linking against the library, so that internal components
can be tested in isolation.

From a build directory:

.. code:: console

    $ make check                      # everything
    $ tests/test-block.unit           # one binary directly
    $ tests/test-ndarray.unit --help  # munit options

munit accepts a test path to run a single case, and other useful flags:

.. code:: console

    $ tests/test-ndarray.unit /test_ndarray/test_read_all
    $ tests/test-ndarray.unit --seed 0x1234  # reproduce a specific ordering
    $ tests/test-ndarray.unit --no-fork      # keep the debugger attached

In particular, the ``--no-fork`` option is always enabled by default for debug
builds, as it's extremely helpful if you want to run the tests under gdb (or
your debugger of choice).

Shell-based integration tests (``tests/test-events.sh``, ``tests/test-info.sh``
and friends) exercise the ``asdf`` command-line tool against golden output
files under ``tests/fixtures/``.  When a deliberate change alters that output,
regenerate the golden files with ``make update-fixtures`` from the build
directory, and check the diff carefully before committing it.

Some test data comes from the ``asdf-standard`` submodule's
``reference_files/`` directory.

Two more targets, both requiring the corresponding ``configure`` option:

- ``make check-valgrind`` (``--enable-valgrind``)
- ``make check-code-coverage`` (``--enable-code-coverage``)


Code style
==========

Formatting is enforced by ``clang-format``; the rules live in
``.clang-format``.  Run:

.. code:: console

    $ make format

from a build directory before committing.  It rewrites all library sources and
headers in place.

There is also a ``make tidy`` target that runs ``clang-tidy``, though its
output is not currently clean and it is not enforced.

To have formatting applied automatically, it is recommended to install the
`pre-commit <https://pre-commit.com/>`__ hook shipped in
``.pre-commit-config.yaml``:

.. code:: console

    $ pip install pre-commit      # or: pipx install pre-commit
    $ pre-commit install

That registers a git hook which runs ``clang-format -i`` over any staged files
under ``src/`` or ``include/``.


.. note::

    The hook uses ``language: system``, so it runs *your* ``clang-format``;
    a different major version may format differently from the rest of the tree.
    To check the whole tree at any time::

        pre-commit run --all-files


Documentation
=============

The documentation is built with Sphinx.  API reference pages are generated from
the doc comments in the public headers under ``include/asdf/`` using `Hawkmoth
<https://hawkmoth.readthedocs.io/>`__, which extracts ``/** ... */`` comments
and feeds them to Sphinx as reStructuredText.  Because of that, doc comments in
public headers are written in reST, using field lists (``:param foo:``,
``:return:``) rather than a Doxygen-style syntax.

Build them with:

.. code:: console

    $ ./configure --enable-docs
    $ make docs

The rendered output lands in ``docs/_build/html`` under the build directory.
CI builds the docs with ``-W``, so warnings are errors; if you add a page,
make sure it is referenced from a ``toctree`` and that every cross-reference
resolves.

The ``asdf(1)`` man page is generated from ``docs/usage/cli.rst`` but is
**committed to the repository** (as ``docs/man/asdf.1``), so that building or
installing from a release tarball does not require Sphinx.  After changing
``usage/cli.rst``, regenerate and commit it:

.. code:: console

    $ make man-page

Adding a new documentation page also means adding it to ``EXTRA_DIST`` in
``docs/Makefile.am``, or it will be missing from the release tarball and the
documentation build inside ``make distcheck`` will fail.


Changelog entries
=================

The changelog is assembled by `towncrier
<https://towncrier.readthedocs.io/>`__ from individual *news fragments* in the
``changes/`` directory.  Each fragment is a small reStructuredText file named
for the issue or pull request it relates to, with the category as its
extension::

    changes/123.feature
    changes/456.bugfix

The available categories are ``general``, ``feature``, ``bugfix``, ``doc``,
``removal`` and ``misc``.  For a change with no associated issue number, use a
descriptive name prefixed with ``+``, for example
``changes/+cmake-build.misc``.

Write the entry for the reader of the release notes, not for the reviewer of
the diff.


.. _making-a-release:

Making a release
================

Despite not being a Python package, version numbers follow :pep:`440`
(because the author likes it) and are managed by
`bumpver <https://github.com/mbarkhau/bumpver>`__, configured in
``.bumpver.toml``.

A single ``bumpver`` invocation rewrites the version everywhere it appears
(``configure.ac``, ``CMakeLists.txt``, ``docs/conf.py``, ``towncrier.toml``,
and ``.bumpver.toml`` itself), builds the changelog, commits, tags and pushes.

Tags are the bare version string, with no ``v`` prefix--``0.1.0a2``, not
``v0.1.0a2``.

.. note::

   CMake does *not* support :pep:`440`-style version tags ("a1", "rc0", etc.),
   so the full version is written in ``CMakeLists.txt`` as a
   ``PACKAGE_VERSION`` variable (mirroring autoconf); the CMake standard
   variable ``PROJECT_VERSION`` is also managed by bumpver, but only contains
   the ``MAJOR.MINOR.PATCH`` portion of the version.

Signing the tag
---------------

Release tags should be signed with your GPG key (bumpver does this via
``git tag --annotate``).  That command *does* honour git's ``tag.gpgSign``
setting, so configure it once and release tags are signed automatically:

.. code:: console

    $ git config --local tag.gpgSign true

This should be configured before making a release; you can find more
information about generating a GPG key and registering it with GitHub
at `Telling Git about your signing key`_.

Cutting the release
-------------------

#. Make sure ``main`` is up to date, the working tree is clean, and CI is
   passing.

#. Check that every merged change has a news fragment in ``changes/``, and
   preview the assembled changelog:

   .. code:: console

       $ towncrier build --draft --version <new version>

#. Run bumpver with the appropriate increment.  Use ``--dry`` first to see
   exactly what will be rewritten:

   .. code:: console

       $ bumpver update --tag-num --dry     # 0.1.0a2 -> 0.1.0a3
       $ bumpver update --tag-num

   Other useful increments are ``--patch``, ``--minor``, ``--major``, and
   ``--tag beta`` / ``--tag final`` to move along the pre-release sequence.

   This runs ``scripts/changelog.sh`` as a pre-commit hook, which consumes
   every file in ``changes/`` and rewrites ``CHANGES.rst``, then commits, tags
   and pushes.

#. The rest is automated.  Pushing the tag triggers the ``Build`` workflow;
   once it succeeds on that tag the ``Release`` workflow creates a **draft**
   GitHub release, with the release notes converted from the new ``CHANGES.rst``
   section and the ``make dist`` tarball attached.

#. Review the draft release on GitHub and publish it.

The release workflow refuses to run unless the tag matches the version recorded
in ``.bumpver.toml`` and ``configure.ac``, and unless the top section of
``CHANGES.rst`` is the one for that version, so a mistagged or half-finished
release fails loudly rather than shipping.

If the ``Release`` workflow needs to be re-run against a tag that has already
built successfully, for instance after fixing something in the workflow
itself, it can be triggered by hand, e.g. with the GitHub CLI:

.. code:: console

    $ gh workflow run release.yml -f tag=<version>

Re-running updates the existing draft rather than failing.
