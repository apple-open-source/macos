dnl
dnl $Id: config.m4,v 1.1.1.2 2003/03/11 01:09:28 zarzycki Exp $
dnl

PHP_ARG_ENABLE(overload,whether to enable user-space object overloading support,
[  --disable-overload      Disable user-space object overloading support.], yes)

if test "$PHP_OVERLOAD" != "no"; then
	AC_DEFINE(HAVE_OVERLOAD, 1, [ ])
	PHP_NEW_EXTENSION(overload, overload.c, $ext_shared)
fi
