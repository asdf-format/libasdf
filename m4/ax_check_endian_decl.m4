AC_DEFUN([AX_CHECK_ENDIAN_DECL], [
  AC_CHECK_HEADERS([endian.h machine/endian.h sys/endian.h])
  AC_CHECK_DECLS([$1], [], [], [
    #ifdef HAVE_ENDIAN_H
    #  include <endian.h>
    #endif
    #ifdef HAVE_MACHINE_ENDIAN_H
    #  include <machine/endian.h>
    #endif
    #ifdef HAVE_SYS_ENDIAN_H
    #  include <sys/endian.h>
    #endif
  ])
])
