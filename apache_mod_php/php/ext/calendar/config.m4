dnl
dnl $Id: config.m4,v 1.1.1.3 2003/03/11 01:09:17 zarzycki Exp $
dnl

PHP_ARG_ENABLE(calendar,whether to enable calendar conversion support,
[  --enable-calendar       Enable support for calendar conversion])

if test "$PHP_CALENDAR" = "yes"; then
  AC_DEFINE(HAVE_CALENDAR,1,[ ])
  PHP_NEW_EXTENSION(calendar, calendar.c dow.c french.c gregor.c jewish.c julian.c easter.c cal_unix.c, $ext_shared)
fi
