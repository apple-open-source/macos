dnl
dnl $Id: config.m4,v 1.1.1.3 2003/03/11 01:09:19 zarzycki Exp $
dnl

PHP_ARG_ENABLE(ftp,whether to enable FTP support,
[  --enable-ftp            Enable FTP support])

if test "$PHP_FTP" = "yes"; then
  AC_DEFINE(HAVE_FTP,1,[Whether you want FTP support])
  PHP_NEW_EXTENSION(ftp, php_ftp.c ftp.c, $ext_shared)
fi
