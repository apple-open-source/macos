dnl
dnl $Id: config.m4,v 1.1.1.3 2003/03/11 01:09:39 zarzycki Exp $
dnl

PHP_TUX=no

AC_ARG_WITH(tux,
[  --with-tux=MODULEDIR    Build PHP as a TUX module (Linux only)],[
  INSTALL_IT="\$(INSTALL) -m 0755 $SAPI_SHARED $withval/php4.tux.so"
  AC_CHECK_HEADERS(tuxmodule.h,[:],[AC_MSG_ERROR([Cannot find tuxmodule.h])])
  PHP_SELECT_SAPI(tux, shared, php_tux.c)
  PHP_TUX=yes
])

AC_MSG_CHECKING(for TUX)
AC_MSG_RESULT($PHP_TUX)

unset PHP_TUX
