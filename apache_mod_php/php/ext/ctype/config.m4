dnl
dnl $Id: config.m4,v 1.1.1.4 2003/03/11 01:09:17 zarzycki Exp $
dnl

PHP_ARG_ENABLE(ctype, whether to enable ctype functions,
[  --disable-ctype         Disable ctype functions], yes)

if test "$PHP_CTYPE" != "no"; then
  AC_DEFINE(HAVE_CTYPE, 1, [ ])
  PHP_NEW_EXTENSION(ctype, ctype.c, $ext_shared)
fi
