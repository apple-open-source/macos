dnl
dnl $Id: config.m4,v 1.1.1.2 2003/03/11 01:09:18 zarzycki Exp $
dnl 

PHP_ARG_ENABLE(dio, whether to enable direct I/O support,
[  --enable-dio            Enable direct I/O support])

if test "$PHP_DIO" != "no"; then
  PHP_NEW_EXTENSION(dio, dio.c, $ext_shared)
fi
