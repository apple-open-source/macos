dnl $Id: config.m4,v 1.1.1.1 2000/08/10 02:08:29 wsanchez Exp $

PHP_ARG_WITH(icap,for ICAP support,
[  --with-icap[=DIR]       Include ICAP support.])

if test "$PHP_ICAP" != "no"; then
  ICAP_DIR=$PHP_ICAP
  test "$ICAP_DIR" = "yes" && ICAP_DIR=/usr/local
    
  AC_ADD_INCLUDE($ICAP_DIR)
  PHP_SUBST(ICAP_SHARED_LIBADD)
  AC_ADD_LIBRARY_WITH_PATH(icap, $ICAP_DIR, ICAP_SHARED_LIBADD)
  AC_DEFINE(HAVE_ICAP,1,[ ])
  PHP_EXTENSION(icap, $ext_shared)
fi
