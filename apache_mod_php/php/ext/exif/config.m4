dnl
dnl $Id: config.m4,v 1.1.1.3 2003/03/11 01:09:18 zarzycki Exp $
dnl

PHP_ARG_ENABLE(exif, whether to enable EXIF (metadata from images) support,
[  --enable-exif           Enable EXIF (metadata from images) support])

if test "$PHP_EXIF" != "no"; then
  AC_DEFINE(HAVE_EXIF, 1, [Whether you want EXIF (metadata from images) support])
  PHP_NEW_EXTENSION(exif, exif.c, $ext_shared)
fi
