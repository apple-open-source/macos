dnl
dnl $Id: config.m4,v 1.1.1.3 2003/03/11 01:09:19 zarzycki Exp $
dnl

AC_ARG_WITH(filepro,[],[enable_filepro=$withval])

PHP_ARG_ENABLE(filepro,whether to enable the bundled filePro support,
[  --enable-filepro        Enable the bundled read-only filePro support.])

if test "$PHP_FILEPRO" = "yes"; then
  AC_DEFINE(HAVE_FILEPRO, 1, [ ])
  PHP_NEW_EXTENSION(filepro, filepro.c, $ext_shared)
fi
