dnl Process this file with autoconf to produce a configure script.
AC_PREREQ(2.5)
AC_INIT(calc.c)
AM_CONFIG_HEADER(calc.h)
AM_INIT_AUTOMAKE(calc, 1.1)

AC_PROG_CC
AC_PROG_INSTALL
#
# Look for various header files
#
AC_CHECK_HEADERS(stdlib.h)

#
# Look for various functions
#
AC_CHECK_FUNC(strcmp)

AM_MAINTAINER_MODE

#
# Output Makefile with substitutions
AC_SUBST(CC)
AC_OUTPUT(Makefile)
