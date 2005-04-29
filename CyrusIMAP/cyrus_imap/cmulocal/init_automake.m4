dnl init_automake.m4--cmulocal automake setup macro
dnl Rob Earhart
dnl $Id: init_automake.m4,v 1.5 2005/03/05 00:36:09 dasenbro Exp $

AC_DEFUN([CMU_INIT_AUTOMAKE], [
	AC_REQUIRE([AM_INIT_AUTOMAKE])
	ACLOCAL="$ACLOCAL -I \$(top_srcdir)/cmulocal"
	])
