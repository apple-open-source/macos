dnl
dnl $Id: config.m4,v 1.5 2002/03/12 16:27:22 sas Exp $
dnl

PHP_ARG_ENABLE(overload,whether to enable user-space object overloading support,
[  --disable-overload      Disable user-space object overloading support.], yes)

if test "$PHP_OVERLOAD" != "no"; then
	AC_DEFINE(HAVE_OVERLOAD, 1, [ ])
	PHP_NEW_EXTENSION(overload, overload.c, $ext_shared)
fi
