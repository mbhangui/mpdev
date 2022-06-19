#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])
AC_INIT([mpdev],[m4_normalize(m4_include(conf-version))],[m4_normalize(m4_include(conf-email))],[mpdev],[https://github.com/mbhangui/mpdev])
AC_CONFIG_SRCDIR([mpdev.c])
AC_CONFIG_HEADERS([config.h])
AC_CANONICAL_HOST
AC_DEFINE_UNQUOTED(PACKAGE_BUGREPORT, "indimail-support@lists.sourceforge.net")
PACKAGE=$PACKAGE_NAME
VERSION=$PACKAGE_VERSION
AM_INIT_AUTOMAKE([foreign])

# Checks for programs.
AC_PROG_CC
LT_INIT
AC_PROG_LN_S
AC_PROG_AWK
AC_PROG_INSTALL
AC_CONFIG_MACRO_DIR([m4])
AC_PROG_MAKE_SET

case "$host" in
*-*-sunos4.1.1*)
	CPPFLAGS="$CPPFLAGS -DSUNOS4"
	CFLAGS="$CFLAGS -O4 -Wall -fPIC"
	CXXFLAGS="$CXXFLAGS -O4 -Wall -fPIC"
	;;
*-*-solaris*)
	CPPFLAGS="$CPPFLAGS -DSOLARIS"
	CFLAGS="$CFLAGS -O4 -Wall -fPIC"
	CXXFLAGS="$CXXFLAGS -O4 -Wall -fPIC"
	;;
*-*-linux*)
	CPPFLAGS="$CPPFLAGS -DLINUX"
	CFLAGS="$CFLAGS -O4 -Wall -fPIC"
	CXXFLAGS="$CXXFLAGS -DLINUX -O4 -Wno-delete-non-virtual-dtor -Wno-reorder -Wall -fPIC"
	LDFLAGS="$LDFLAGS -pie"
	;;
*-*-freebsd*)
	CPPFLAGS="$CPPFLAGS -DFREEBSD -I/usr/local/include"
	CFLAGS="$CFLAGS -I/usr/local/include -I/usr/local/include/qmail -Wall"
	CXXFLAGS="$CXXFLAGS -DFREEBSD -I/usr/local/include -Wall -fPIC"
	CXXFLAGS="$CXXFLAGS -Wno-delete-non-virtual-dtor -Wno-deprecated-register -Wno-reorder"
	CXXFLAGS="$CXXFLAGS -Wno-delete-non-abstract-non-virtual-dtor"
	LDFLAGS="$LDFLAGS -L/usr/local/lib"
	;;
*-*-darwin*)
	CPPFLAGS="$CPPFLAGS -DDARWIN -I/opt/local/include"
	CFLAGS="$CFLAGS -I/opt/local/include -I/usr/local/include -I/opt/local/include/qmail -Wall"
	CXXFLAGS="$CXXFLAGS -DDARWIN -Wall -fPIC"
	CXXFLAGS="$CXXFLAGS -DBIND_8_COMPAT -I/opt/local/include -Wno-c++11-extensions"
	CXXFLAGS="$CXXFLAGS -Wno-delete-non-virtual-dtor -Wno-reorder-ctor -Wno-reorder"
	LDFLAGS="$LDFLAGS -L/opt/local/lib -L/usr/local/lib"
	;;
*)
	CFLAGS="$CFLAGS -O4 -Wall -fPIC"
	CXXFLAGS="$CXXFLAGS -O4 -Wall -fPIC"
	;;
esac

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
AC_CONFIG_FILES([mpdev-release:mpdev-release.in],[], [])
AC_OUTPUT
