dnl $Id: config.m4,v 1.1.1.1 2000/08/10 02:08:28 wsanchez Exp $
dnl config.m4 for extension exif

PHP_ARG_ENABLE(exif, whether to enable exif support,
dnl Make sure that the comment is aligned:
[  --enable-exif           Enable exif support])

if test "$PHP_EXIF" != "no"; then
  AC_DEFINE(HAVE_EXIF, 1, [Whether you want exif support])
  PHP_EXTENSION(exif, $ext_shared)
fi
