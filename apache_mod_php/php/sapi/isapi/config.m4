dnl
dnl $Id: config.m4,v 1.14.4.1 2003/10/03 05:25:46 sniper Exp $
dnl

RESULT=no
AC_MSG_CHECKING(for Zeus ISAPI support)
AC_ARG_WITH(isapi,
[  --with-isapi[=DIR]      Build PHP as an ISAPI module for use with Zeus.],
[
	if test "$withval" = "yes"; then
		ZEUSPATH=/usr/local/zeus # the default
	else
		ZEUSPATH=$withval
	fi
	test -f "$ZEUSPATH/web/include/httpext.h" || AC_MSG_ERROR(Unable to find httpext.h in $ZEUSPATH/web/include)
	PHP_BUILD_THREAD_SAFE
	AC_DEFINE(WITH_ZEUS,1,[ ])
	PHP_ADD_INCLUDE($ZEUSPATH/web/include)
    PHP_SELECT_SAPI(isapi, shared, php4isapi.c)
	INSTALL_IT="\$(SHELL) \$(srcdir)/install-sh -m 0755 $SAPI_SHARED \$(INSTALL_ROOT)$ZEUSPATH/web/bin/"
	RESULT=yes
])
AC_MSG_RESULT($RESULT)

dnl ## Local Variables:
dnl ## tab-width: 4
dnl ## End:
