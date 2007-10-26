dnl Process this file with autoconf to produce a configure script.

dnl Copyright (C) 2002, 2003, 2004, 2005 Simon Josefsson.
dnl
dnl This file is part of GNU Libidn.
dnl
dnl GNU Libidn is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU Lesser General Public License as
dnl published by the Free Software Foundation; either version 2, or
dnl (at your option) any later version.
dnl
dnl GNU Libidn is distributed in the hope that it will be useful, but
dnl WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with GNU Libidn; see the file COPYING.LIB.  If not, write to
dnl the Free Software Foundation, Inc., 51 Franklin Street, Fifth
dnl Floor, Boston, MA 02110-1301, USA.

AC_INIT([libidn], [0.6.0], [bug-libidn@gnu.org])

# Library code modified:                              REVISION++
# Interfaces changed/added/removed:   CURRENT++       REVISION=0
# Interfaces added:                             AGE++
# Interfaces removed:                           AGE=0
AC_SUBST(LT_CURRENT, 16)
AC_SUBST(LT_REVISION, 14)
AC_SUBST(LT_AGE, 5)

AC_PREREQ(2.59)
AM_INIT_AUTOMAKE([1.9 gnits])
AB_INIT
AC_CONFIG_HEADERS(config.h)

# Checks for programs.
AM_GNU_GETTEXT(external)
AM_GNU_GETTEXT_VERSION(0.14.1)
AC_PROG_LIBTOOL
AC_PROG_CC
gl_EARLY
AM_MISSING_PROG(PERL, perl, $missing_dir)
AM_MISSING_PROG(HELP2MAN, help2man, $missing_dir)
AM_ICONV
AM_PATH_LISPDIR

if test "$am_cv_func_iconv" != "yes"; then
  AC_MSG_NOTICE([
***

Your system does not have iconv().  The iconv() function is not
strictly required by the source code.  However, that may change in
the future, unless you report that this may be a problem for you.

Please report this to $PACKAGE_BUGREPORT.

***])
fi

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([locale.h])

# Checks for typedefs, structures, and compiler characteristics.
AX_CREATE_STDINT_H(lib/idn-int.h, sys/types.h)

# Checks for library functions.
AC_CHECK_FUNCS([setlocale])

# For gnulib stuff in gl/.
gl_INIT

# Check if locale stuff works
AM_LANGINFO_CODESET

# Make it possible to disable TLD code.
AC_ARG_ENABLE(tld, AC_HELP_STRING([--disable-tld], [disable TLD code]),
		enable_tld=$enableval, enable_tld=yes)
AM_CONDITIONAL(TLD, test "$enable_tld" != "no")
if test "$enable_tld" != "no"; then
	AC_DEFINE(WITH_TLD, 1, [Define to 1 if you want TLD code.])
fi
AC_MSG_CHECKING([if TLD API for specific TLD rules should be built])
AC_MSG_RESULT($enable_tld)

# Check for gtk-doc.
GTK_DOC_CHECK(1.1)

# Check for java
AC_ARG_ENABLE(java, AC_HELP_STRING([--enable-java], [enable Java API]),,
		enable_java=no)
if test "$enable_java" != "no"; then
	AC_PROG_JAVAC
	AC_PROG_JAR
	AM_MISSING_PROG(GJDOC, gjdoc, $missing_dir)
fi
AM_CONDITIONAL(JAVA, test "$enable_java" != "no")
AC_MSG_CHECKING([if implementation in Java should be built])
AC_MSG_RESULT($enable_java)

# Check for C#
AM_CONDITIONAL(CSHARP, test -n "$HAVE_CSHARPCOMP")
AC_MSG_CHECKING([[if implementation in C# should be built]])
if test -n "$HAVE_CSHARPCOMP"; then
  ac_result=yes
else
  ac_result=no
fi
AC_MSG_RESULT($ac_result)

AC_CONFIG_FILES([Makefile libidn.pc doc/reference/Makefile doc/java/Makefile \
		gl/Makefile lib/stringprep.h contrib/doxygen/Doxyfile \
		doc/Makefile doc/specifications/Makefile doc/tld/Makefile \
		lib/Makefile src/Makefile tests/Makefile examples/Makefile \
		java/Makefile java/gnu/Makefile java/gnu/inet/Makefile \
		java/gnu/inet/encoding/Makefile java/misc/Makefile \
		csharp/Makefile gl/csharpcomp.sh \
		po/Makefile.in])
AC_OUTPUT
