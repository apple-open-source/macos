dnl $Id: config.m4,v 1.1.1.1 2000/08/10 02:08:27 wsanchez Exp $

AC_ARG_WITH(dbase,[],[enable_dbase=$withval])

PHP_ARG_ENABLE(dbase,whether to enable the bundled dbase library,
[  --enable-dbase          Enable the bundled dbase library])

if test "$PHP_DBASE" = "yes"; then
  AC_DEFINE(DBASE,1,[ ])
  PHP_EXTENSION(dbase, $ext_shared)
fi
