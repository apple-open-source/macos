dnl $Id: config.m4,v 1.1.1.3 2001/07/19 00:20:05 zarzycki Exp $
dnl config.m4 for extension sockets

PHP_ARG_ENABLE(sockets, whether to enable sockets support,
[  --enable-sockets        Enable sockets support])

if test "$PHP_SOCKETS" != "no"; then

  AC_CHECK_HEADERS(netdb.h netinet/tcp.h sys/un.h errno.h)
  AC_CHECK_FUNCS(hstrerror)
  AC_DEFINE(HAVE_SOCKETS, 1, [ ])
  PHP_EXTENSION(sockets, $ext_shared)
fi
