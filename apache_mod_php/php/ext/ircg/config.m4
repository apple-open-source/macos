dnl
dnl $Id: config.m4,v 1.14.4.2 2003/01/21 05:09:33 sniper Exp $
dnl

PHP_ARG_WITH(ircg, for IRCG support,
[  --with-ircg             Include IRCG support])

AC_ARG_WITH(ircg-config,
[  --with-ircg-config=PATH   IRCG: Path to the ircg-config script],
[ IRCG_CONFIG=$withval ],
[
if test "$with_ircg" != "yes" && test "$with_ircg" != "no"; then
  IRCG_CONFIG=$with_ircg/bin/ircg-config
else
  IRCG_CONFIG=ircg-config
fi
])

if test "$PHP_IRCG" != "no"; then

  IRCG_PREFIX=`$IRCG_CONFIG --prefix`
  
  if test -z "$IRCG_PREFIX"; then
    AC_MSG_ERROR([I cannot run the ircg-config script which should have been installed by IRCG. Please ensure that the script is in your PATH or point --with-ircg-config to the path of the script.])
  fi
  
  PHP_EVAL_LIBLINE(`$IRCG_CONFIG --ldflags`)
  PHP_EVAL_INCLINE(`$IRCG_CONFIG --cppflags`)
  PHP_ADD_LIBRARY_WITH_PATH(ircg, $PHP_IRCG/lib)
  PHP_ADD_INCLUDE($PHP_IRCG/include)
  if test "$PHP_SAPI" = "thttpd"; then
    AC_DEFINE(IRCG_WITH_THTTPD, 1, [Whether thttpd is available])
    PHP_DISABLE_CLI
  fi
  AC_DEFINE(HAVE_IRCG, 1, [Whether you want IRCG support])
  PHP_NEW_EXTENSION(ircg, ircg.c ircg_scanner.c, $ext_shared)
  PHP_ADD_MAKEFILE_FRAGMENT
fi
