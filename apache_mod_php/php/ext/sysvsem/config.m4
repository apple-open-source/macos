dnl
dnl $Id: config.m4,v 1.1.1.3 2003/03/11 01:09:34 zarzycki Exp $
dnl

PHP_ARG_ENABLE(sysvsem,whether to enable System V semaphore support,
[  --enable-sysvsem        Enable System V semaphore support.])

if test "$PHP_SYSVSEM" != "no"; then
 PHP_NEW_EXTENSION(sysvsem, sysvsem.c, $ext_shared)
 AC_DEFINE(HAVE_SYSVSEM, 1, [ ])
 AC_CACHE_CHECK(for union semun,php_cv_semun,
   AC_TRY_COMPILE([
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
   ],
   [union semun x;],
   [
     php_cv_semun=yes
   ],[
     php_cv_semun=no
   ])
 )
 if test "$php_cv_semun" = "yes"; then
   AC_DEFINE(HAVE_SEMUN, 1, [ ])
 else
   AC_DEFINE(HAVE_SEMUN, 0, [ ])
 fi
fi
