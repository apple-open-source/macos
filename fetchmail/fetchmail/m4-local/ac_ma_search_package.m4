dnl @synopsis AC_ma_SEARCH_PACKAGE(PACKAGE, FUNCTION, PREFIX LIST, LIBRARY LIST, HEADERFILE [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]])
dnl based on AC_caolan_SEARCH_PACKAGE
dnl
dnl Provides --with-PACKAGE, --with-PACKAGE-include and --with-PACKAGE-libdir
dnl options to configure. Supports the now standard --with-PACKAGE=DIR
dnl approach where the package's include dir and lib dir are underneath DIR,
dnl but also allows the include and lib directories to be specified seperately
dnl
dnl PREFIX LIST can be a list of directories to search for the package
dnl if set to "no", the package must be enabled with --with-PACKAGE
dnl otherwise it is enabled unless overridden with --without-PACKAGE
dnl
dnl adds the extra -Ipath to CFLAGS if needed
dnl adds extra -Lpath to LD_FLAGS if needed
dnl searches for the FUNCTION in each of the LIBRARY LIST with
dnl AC_SEARCH_LIBRARY and thus adds the lib to LIBS
dnl
dnl defines HAVE_PKG_PACKAGE if it is found, (where PACKAGE in the
dnl HAVE_PKG_PACKAGE is replaced with the actual first parameter passed)
dnl note that autoheader will complain of not having the HAVE_PKG_PACKAGE and you
dnl will have to add it to acconfig.h manually
dnl
dnl @version $Id: ac_caolan_search_package.m4,v 1.3 2003/10/29 02:13:06 guidod Exp $
dnl @author Caolan McNamara <caolan@skynet.ie>
dnl
dnl with fixes from...
dnl Alexandre Duret-Lutz <duret_g@lrde.epita.fr>
dnl Matthew Mueller <donut@azstarnet.com>
dnl Matthias Andree <matthias.andree@gmx.de>

AC_PREREQ(2.59)dnl oldest tested version

AC_DEFUN([AC_ma_SEARCH_PACKAGE],
[

search="$3"
AC_ARG_WITH($1,
AS_HELP_STRING([--without-$1],[disables $1 usage completely])
AS_HELP_STRING([--with-$1[=DIR]],[root directory of $1 installation]),
if test "${with_$1}" != yes; then
	search="$withval"
	$1_winclude="$withval/include"
	$1_wlibdir="$withval/lib"
fi
)

AC_ARG_WITH($1-include,
AS_HELP_STRING([--with-$1-include=DIR],[specify exact include dir for $1 headers]),
$1_winclude="$withval")

AC_ARG_WITH($1-libdir,
AS_HELP_STRING([--with-$1-libdir=DIR],[specify exact library dir for $1 library]),
$1_wlibdir="$withval")

if test "${with_$1}" != no ; then
    for i in $search ; do
	if test "$search" = "${with_$1}" ; then
	    $1_include="${$1_winclude}"
	    $1_libdir="${$1_wlibdir}"
	else
	    $1_include=$i/include
	    $1_libdir=$i/lib
	fi

	if test ! -f "${$1_include}/$5" -o ! -d "${$1_libdir}" ; then
	    continue
	fi

	OLD_LIBS=$LIBS
	OLD_LDFLAGS=$LDFLAGS
	OLD_CFLAGS=$CFLAGS
	OLD_CPPFLAGS=$CPPFLAGS

	if test -n "${$1_libdir}" -a "${$1_libdir}" != /usr/lib ; then
		LDFLAGS="$LDFLAGS -L${$1_libdir}"
	fi
	if test -n "${$1_include}" -a "${$1_include}" != /usr/include ; then
		CPPFLAGS="$CPPFLAGS -I${$1_include}"
	fi

	success=no
	AC_SEARCH_LIBS($2,$4,success=yes)
	AC_CHECK_HEADERS($5,,success=no)
	if test "$success" = yes; then
dnl	fixed
		ifelse([$6], , , [$6])
		AC_DEFINE(HAVE_PKG_$1,1,[Define to 1 if you have the '$1' package.])
		break
	else
		LIBS=$OLD_LIBS
		LDFLAGS=$OLD_LDFLAGS
		CPPFLAGS=$OLD_CPPFLAGS
		CFLAGS=$OLD_CFLAGS
	fi
    done
    if test "$success" = no ; then
dnl	broken
    ifelse([$7], , , [$7])
	LIBS=$OLD_LIBS
	LDFLAGS=$OLD_LDFLAGS
	CPPFLAGS=$OLD_CPPFLAGS
	CFLAGS=$OLD_CFLAGS
    fi
fi

])
