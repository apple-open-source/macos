dnl $Id: config.m4,v 1.1.1.1 2001/01/25 05:00:23 wsanchez Exp $
dnl config.m4 for extension zziplib

PHP_ARG_WITH(zziplib,whether to include zziplib support, 
[  --with-zziplib[=DIR]    Include zziplib support (requires zziplib >= 0.10.6).
                          DIR is the zziplib install directory,
                          default is /usr/local.])


if test "$PHP_ZZIPLIB" != "no"; then
  PHP_EXTENSION(zziplib, $ext_shared)
  for i in $PHP_ZZIPLIB /usr/local /usr ; do
    if test -f $i/include/zzlib/zziplib.h; then
      ZZIPLIB_DIR=$i
      ZZIPLIB_INCDIR=$i/include/zzlib
    elif test -f $i/include/zziplib.h; then
      ZZIPLIB_DIR=$i
      ZZIPLIB_INCDIR=$i/include
    fi
  done

  if test -z "$ZZIPLIB_DIR"; then
    AC_MSG_ERROR(Cannot find libzzip)
  fi

  ZZIPLIB_LIBDIR=$ZZIPLIB_DIR/lib

  AC_TEMP_LDFLAGS(-L$ZZIPLIB_LIBDIR,[
  AC_CHECK_LIB(zzip, zzip_open, [AC_DEFINE(HAVE_ZZIPLIB,1,[ ])],
    [AC_MSG_ERROR(zziplib module requires zzlib >= 0.10.6.)])
  ])

  PHP_SUBST(ZZIPLIB_SHARED_LIBADD)
  AC_ADD_LIBRARY_WITH_PATH(zzip, $ZZIPLIB_LIBDIR, ZZIPLIB_SHARED_LIBADD)
  
  AC_ADD_INCLUDE($ZZIPLIB_INCDIR)

  PHP_FOPENCOOKIE
fi
