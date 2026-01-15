dnl -- Check for sphinx installation and required dependencies to build the docs
AC_DEFUN([_ASDF_DOCS_DISABLE_OR_ERROR], [
  AS_IF([test "x$enable_docs" = "xyes"], [
    AC_MSG_ERROR([$1, needed to build docs])
  ], [
    AC_MSG_NOTICE([$1, docs disabled])
    enable_docs=no
    AM_CONDITIONAL([BUILD_DOCS],[false])
    AC_SUBST([SPHINXBUILD],[:])
    AC_SUBST([HAWKMOTH_AVAILABLE],[:])
    AC_SUBST([CLANG_PY_AVAILABLE],[:])
  ])
])


AC_DEFUN([ASDF_CHECK_SPHINX], [
  AC_ARG_ENABLE([docs],
    AS_HELP_STRING([--enable-docs],[build documentation with Sphinx]),
    [enable_docs=$enableval],
    [enable_docs=auto])

  AS_IF([test "x$enable_docs" != "xno"], [
    AM_PATH_PYTHON([3.0],, [:])
    AS_IF([test "x$PYTHON" = "x:"], [
      AC_MSG_RESULT([no])
      _ASDF_DOCS_DISABLE_OR_ERROR([Python >= 3.9 not found])
    ])
  ])

  AS_IF([test "x$enable_docs" != "xno"], [
    AC_PATH_PROG([SPHINXBUILD],[sphinx-build],[:])
    AS_IF([test "x$SPHINXBUILD" = "x:"], [
      _ASDF_DOCS_DISABLE_OR_ERROR([sphinx-build not found])
    ])
  ])

  AS_IF([test "x$enable_docs" != "xno"], [
    AC_MSG_CHECKING([for Hawkmoth Python package])
    $PYTHON -c "import hawkmoth" 2>/dev/null
    AS_IF([test $? -eq 0], [
      AC_MSG_RESULT([yes])
      HAWKMOTH_AVAILABLE=yes
    ], [
      AC_MSG_RESULT([no])
      _ASDF_DOCS_DISABLE_OR_ERROR([Hawkmoth not found])
    ])
  ])

  AS_IF([test "x$enable_docs" != "xno"], [
    AC_MSG_CHECKING([for Python libclang bindings])
    $PYTHON -c "from clang import cindex; cindex.Config().get_cindex_library()" 2>/dev/null
    AS_IF([test $? -eq 0], [
      AC_MSG_RESULT([yes])
      CLANG_PY_AVAILABLE=yes
    ], [
      AC_MSG_RESULT([no])
      _ASDF_DOCS_DISABLE_OR_ERROR([libclang Python bindings not found])
    ])
  ])

  AM_CONDITIONAL([BUILD_DOCS], [test "x$enable_docs" != "xno"])
  AC_SUBST([SPHINXBUILD])
  AC_SUBST([HAWKMOTH_AVAILABLE])
  AC_SUBST([CLANG_PY_AVAILABLE])
])
