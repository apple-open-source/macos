dnl
dnl $Id: config.m4,v 1.1.1.3 2003/03/11 01:09:28 zarzycki Exp $
dnl

if test "$PHP_OPENSSL" != "no"; then
  PHP_NEW_EXTENSION(openssl, openssl.c, $ext_openssl_shared)
  OPENSSL_SHARED_LIBADD="-lcrypto -lssl"
  PHP_SUBST(OPENSSL_SHARED_LIBADD)
  AC_DEFINE(HAVE_OPENSSL_EXT,1,[ ])
fi
