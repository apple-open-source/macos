dnl init_automake.m4--cmulocal automake setup macro
dnl Rob Earhart
dnl $Id: init_automake.m4,v 1.1 2004/03/31 18:08:37 dasenbro Exp $

AC_DEFUN([CMU_INIT_AUTOMAKE], [
	AC_REQUIRE([AM_INIT_AUTOMAKE])
	ACLOCAL="$ACLOCAL -I \$(top_srcdir)/cmulocal"
	])
