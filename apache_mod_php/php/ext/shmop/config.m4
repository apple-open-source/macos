dnl $Id: config.m4,v 1.1.1.3 2003/03/11 01:09:32 zarzycki Exp $
PHP_ARG_ENABLE(shmop, whether to enable shmop support, 
[  --enable-shmop          Enable shmop support])

if test "$PHP_SHMOP" != "no"; then
  AC_DEFINE(HAVE_SHMOP, 1, [ ])
  PHP_NEW_EXTENSION(shmop, shmop.c, $ext_shared)
fi
