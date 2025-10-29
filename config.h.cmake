#ifndef CONFIG_H
#define CONFIG_H

/* Enable color output in logs */
#define ASDF_LOG_COLOR @ASDF_LOG_COLOR@

/* Default runtime log level */
#define ASDF_LOG_DEFAULT_LEVEL ASDF_LOG_@ASDF_LOG_MIN_LEVEL@

/* Enable logging */
#define ASDF_LOG_ENABLED @ASDF_LOG_ENABLED@

/* Compile-time minimum log level */
#define ASDF_LOG_MIN_LEVEL ASDF_LOG_@ASDF_LOG_MIN_LEVEL@

/* Name of package */
#define PACKAGE "@PACKAGE_NAME@"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "@PACKAGE_NAME@"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "@PACKAGE_NAME@ @PACKAGE_VERSION@"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "@PACKAGE_VERSION@"

/* Version number of package */
#define VERSION "@PACKAGE_VERSION@"

#cmakedefine HAVE_ENDIAN_H
#cmakedefine HAVE_MACHINE_ENDIAN_H
#cmakedefine HAVE_SYS_ENDIAN_H
#cmakedefine HAVE_STRPTIME
#cmakedefine01 HAVE_DECL_BE64TOH
#cmakedefine HAVE_BZIP2
#cmakedefine HAVE_LZ4
#cmakedefine HAVE_ZLIB

#endif
