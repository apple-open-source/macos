dnl $Id: config.m4,v 1.1.1.1 2000/08/10 02:08:26 wsanchez Exp $

PHP_ARG_ENABLE(calendar,whether to enable calendar conversion support,
[  --enable-calendar       Enable support for calendar conversion])

if test "$PHP_CALENDAR" = "yes"; then
  AC_DEFINE(HAVE_CALENDAR,1,[ ])
  PHP_EXTENSION(calendar, $ext_shared)
fi
