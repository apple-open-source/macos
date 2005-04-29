dnl
dnl "$Id: cups-network.m4,v 1.8 2005/01/04 22:10:38 jlovell Exp $"
dnl
dnl   Networking stuff for the Common UNIX Printing System (CUPS).
dnl
dnl   Copyright 1997-2005 by Easy Software Products, all rights reserved.
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
dnl       Hollywood, Maryland 20636 USA
dnl
dnl       Voice: (301) 373-9600
dnl       EMail: cups-info@cups.org
dnl         WWW: http://www.cups.org
dnl

NETLIBS=""

if test "$uname" != "IRIX"; then
	AC_CHECK_LIB(socket,socket,NETLIBS="-lsocket")
	AC_CHECK_LIB(nsl,gethostbyaddr,NETLIBS="$NETLIBS -lnsl")
fi

AC_CHECK_FUNCS(rresvport getifaddrs hstrerror)

AC_CHECK_MEMBER(struct sockaddr.sa_len,,,[#include <sys/socket.h>])
AC_CHECK_HEADER(sys/sockio.h,AC_DEFINE(HAVE_SYS_SOCKIO_H))

AC_SUBST(NETLIBS)

if test "$uname" = "SunOS"; then
	case "$uversion" in
		55* | 56*)
			maxfiles=1024
			;;
		*)
			maxfiles=4096
			;;
	esac
else
	maxfiles=4096
fi

AC_ARG_WITH(maxfiles, [  --with-maxfiles=N       set maximum number of file descriptors for scheduler ],
	maxfiles=$withval)

AC_DEFINE_UNQUOTED(CUPS_MAX_FDS, $maxfiles)


dnl Check for unix domain socket support...
AC_ARG_ENABLE(domainsockets, [  --enable-domainsockets       enable unix domain socket support, default=no])

if test x$enable_domainsockets = xyes; then
	AC_MSG_CHECKING(for unix domain socket support)
	AC_TRY_COMPILE([#include <sys/types.h>
		#include <sys/socket.h>
		#include <sys/un.h>],
		[struct sockaddr_un sa; sa.sun_family = AF_LOCAL;],
		cups_cv_domainsockets=yes, cups_cv_domainsockets=no)

	if test x"$cups_cv_domainsockets" = x"yes"; then
		AC_DEFINE(HAVE_DOMAINSOCKETS)
		AC_MSG_RESULT(yes)
	else
		AC_MSG_RESULT(no)
	fi
fi


CUPS_DEFAULT_DOMAINSOCKET=""
AC_ARG_WITH(default_domainsocket, [  --with-domainsocket        set unix domain socket name],default_domainsocket="$withval",default_domainsocket="")
if test x$default_domainsocket = x; then

	case "$uname" in
		Darwin*)
			# Darwin and MacOS X...
			CUPS_DEFAULT_DOMAINSOCKET="$localstatedir/run/cupsd"
			AC_DEFINE_UNQUOTED(CUPS_DEFAULT_DOMAINSOCKET, "$localstatedir/run/cupsd")
			;;
		*)
			# All others...
			CUPS_DEFAULT_DOMAINSOCKET="$localstatedir/run/cupsd"
			AC_DEFINE_UNQUOTED(CUPS_DEFAULT_DOMAINSOCKET, "$localstatedir/run/cupsd")
			;;
	esac
fi
AC_SUBST(CUPS_DEFAULT_DOMAINSOCKET)


dnl
dnl End of "$Id: cups-network.m4,v 1.8 2005/01/04 22:10:38 jlovell Exp $".
dnl
