dnl
dnl $Id: config.m4,v 1.12.4.2 2003/10/01 02:54:01 sniper Exp $
dnl

PHP_ARG_WITH(mhash, for mhash support,
[  --with-mhash[=DIR]      Include mhash support.])

if test "$PHP_MHASH" != "no"; then
  for i in $PHP_MHASH /usr/local /usr /opt/mhash; do
    test -f $i/include/mhash.h && MHASH_DIR=$i && break
  done

  if test -z "$MHASH_DIR"; then
    AC_MSG_ERROR(Please reinstall libmhash - I cannot find mhash.h)
  fi

  PHP_ADD_INCLUDE($MHASH_DIR/include)
  PHP_ADD_LIBRARY_WITH_PATH(mhash, $MHASH_DIR/lib, MHASH_SHARED_LIBADD)

  PHP_NEW_EXTENSION(mhash, mhash.c, $ext_shared)
  PHP_SUBST(MHASH_SHARED_LIBADD)
  AC_DEFINE(HAVE_LIBMHASH,1,[ ])
fi
