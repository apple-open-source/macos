dnl
dnl "$Id: cups-dnssd.m4,v 1.1 2005/02/16 17:58:01 jlovell Exp $"
dnl
dnl   DNS Service Discovery (aka Bonjour) stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   http://www.dns-sd.org
dnl   http://www.multicastdns.org/
dnl   http://developer.apple.com/macosx/bonjour
dnl
dnl   Copyright 1997-2002 by Easy Software Products, all rights reserved.
dnl
dnl   These coded instructions, statements, and computer programs are the
dnl   property of Easy Software Products and are protected by Federal
dnl   copyright law.  Distribution and use rights are outlined in the file
dnl   "LICENSE.txt" which should have been included with this file.  If this
dnl   file is missing or damaged please contact Easy Software Products
dnl   at:
dnl
dnl       Attn: CUPS Licensing Information
dnl       Easy Software Products
dnl       44141 Airport View Drive, Suite 204
dnl       Hollywood, Maryland 20636-3111 USA
dnl
dnl       Voice: (301) 373-9603
dnl       EMail: cups-info@cups.org
dnl         WWW: http://www.cups.org
dnl

AC_ARG_ENABLE(dnssd, [  --enable-dnssd            turn on Multicast DNS support, default=yes])
AC_ARG_WITH(dnssd-libs, [  --with-dnssd-libs        set directory for Multicast DNS library],
    LDFLAGS="-L$withval $LDFLAGS"
    DSOFLAGS="-L$withval $DSOFLAGS",)
AC_ARG_WITH(dnssd-includes, [  --with-dnssd-includes    set directory for Multicast DNS includes],
    CFLAGS="-I$withval $CFLAGS"
    CXXFLAGS="-I$withval $CXXFLAGS"
    CPPFLAGS="-I$withval $CPPFLAGS",)

DNSSDLIBS=""

if test x$enable_dnssd != xno; then
    AC_CHECK_HEADER(dns_sd.h, 
            AC_DEFINE(HAVE_DNSSD)
	    DNSSDLIBS="-framework CoreFoundation -framework SystemConfiguration")
fi

if test "x$DNSSDLIBS" = x; then
	CUPS_DEFAULT_BROWSELOCALPROTOCOLS="cups"
else
	CUPS_DEFAULT_BROWSELOCALPROTOCOLS="cups dnssd"
fi

AC_SUBST(DNSSDLIBS)
AC_SUBST(CUPS_DEFAULT_BROWSELOCALPROTOCOLS)

dnl
dnl End of "$Id: cups-dnssd.m4,v 1.1 2005/02/16 17:58:01 jlovell Exp $".
dnl
