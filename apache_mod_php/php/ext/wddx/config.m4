dnl $Id: config.m4,v 1.1.1.1 2000/08/10 02:08:37 wsanchez Exp $
dnl config.m4 for extension wddx

PHP_ARG_ENABLE(wddx,whether to enable WDDX support,
[  --enable-wddx           Enable WDDX support])

if test "$PHP_WDDX" = "yes"; then
  if test "$enable_xml" = "no"; then
    AC_MSG_WARN(Activating XML)
    enable_xml=yes
  fi
  AC_DEFINE(HAVE_WDDX, 1, [ ])
  PHP_EXTENSION(wddx)
fi
