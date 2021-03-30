#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([mpdev],m4_normalize(m4_include([conf-version])),m4_normalize(m4_include([conf-email])),mpdev,[https://github.com/mbhangui/mpdev])
AC_CONFIG_SRCDIR([mpdev.c])
AC_CONFIG_HEADERS([config.h])
AC_DEFINE_UNQUOTED(PACKAGE_BUGREPORT, "indimail-support@lists.sourceforge.net")
PACKAGE=$PACKAGE_NAME
VERSION=$PACKAGE_VERSION
AM_INIT_AUTOMAKE([foreign])

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL
AC_PROG_LN_S
AC_PROG_AWK
AC_PROG_INSTALL
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_MAKE_SET

# Checks for libraries.
AC_CHECK_LIB(qmail, substdio_fdbuf, [AC_SUBST([LIB_QMAIL], ["-lqmail"]) AC_DEFINE([HAVE_QMAIL], [1],[qmail Library])],noqmail=t,)
if test " $noqmail" = " t"
then
  AC_MSG_ERROR(Could not find qmail library.)
fi
AC_CHECK_LIB(sqlite3, sqlite3_libversion, [AC_SUBST([LIB_SQLITE3], ["-lsqlite3"]) AC_DEFINE([HAVE_SQLITE3], [1],[sqlite3 Library])],nosqlite3=t,)
if test " $nosqlite3" = " t"
then
  AC_MSG_ERROR(Could not find sqlite3 library.)
fi

# Checks for header files.
AC_CHECK_HEADERS([string.h unistd.h errno.h fcntl.h time.h sys/time.h stdlib.h sys/types.h sys/stat.h])
AC_CHECK_HEADERS([limits.h stdint.h stdbool.h sys/param.h dlfcn.h link.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_SSIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([alarm])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
