dnl $Id: config.m4,v 1.1.1.1 2000/08/10 02:08:36 wsanchez Exp $

PHP_ARG_ENABLE(sysvshm,whether to enable System V shared memory support,
[  --enable-sysvshm        Enable the System V shared memory support])

if test "$PHP_SYSVSHM" != "no"; then
  AC_DEFINE(HAVE_SYSVSHM, 1, [ ])
  PHP_EXTENSION(sysvshm, $ext_shared)
fi
