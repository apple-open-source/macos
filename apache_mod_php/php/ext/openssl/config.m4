dnl $Id: config.m4,v 1.1.1.1 2001/01/25 04:59:46 wsanchez Exp $
dnl config.m4 for extension OpenSSL

if test "$OPENSSL_DIR"; then
  PHP_EXTENSION(openssl, $ext_shared)
  AC_DEFINE(HAVE_OPENSSL_EXT,1,[ ])
fi
