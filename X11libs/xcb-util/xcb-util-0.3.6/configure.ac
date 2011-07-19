dnl AC_TYPE_SSIZE_T is defined from 2.59c
AC_PREREQ(2.59c)
AC_INIT([xcb-util],0.3.6,[xcb@lists.freedesktop.org])
AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE([foreign dist-bzip2])

AC_CHECK_PROGS(M4, [m4 gm4], [no])
if test $M4 != "no" ; then
	AC_MSG_CHECKING([if $M4 supports -I])
	if $M4 -I. /dev/null > /dev/null 2>&1 ; then
		AC_MSG_RESULT([yes])
	else
		AC_MSG_RESULT([no])
		# Try finding the gnu version
		AC_CHECK_PROGS(GM4, gm4, [no])
		if test $GM4 = "no" ; then
			AC_PATH_PROGS(GNUM4, m4, [no], [/usr/gnu/bin])
			M4="$GNUM4"
		else
			M4="$GM4"
		fi
	fi
fi
if test $M4 = "no"; then
	AC_MSG_ERROR([Can't find usable m4, please install it and try again])
fi
AC_CHECK_PROGS(GPERF, gperf, [no])
if test $GPERF = "no"; then
	AC_MSG_ERROR([Can't find gperf, please install it and try again])
fi
AC_PROG_CC
AC_PROG_LIBTOOL

if  test "x$GCC" = xyes ; then
    CWARNFLAGS="-Wall"
else
    AC_CHECK_DECL([__SUNPRO_C], [SUNCC="yes"], [SUNCC="no"])
    if test "x$SUNCC" = "xyes"; then
	CWARNFLAGS="-v"
    fi
fi
AC_SUBST(CWARNFLAGS)
AC_CHECK_FUNCS_ONCE(vasprintf)

dnl Checking whether ssize_t is defined (xcb-util-common.h)
AC_TYPE_SSIZE_T

xcbincludedir='${includedir}/xcb'
AC_SUBST(xcbincludedir)

pkgconfigdir='${libdir}/pkgconfig'
AC_SUBST(pkgconfigdir)

PKG_CHECK_MODULES(XCB, xcb >= 1.4)

######
# Check version of xcb-proto that xcb was compiled against
######
xcbproto_required=1.5

# Moved from AX_COMPARE_VERSION to maintain proper output
AC_PROG_AWK

AC_MSG_CHECKING([whether libxcb was compiled against xcb-proto >= $xcbproto_required])
xcbproto_version=`$PKG_CONFIG --variable=xcbproto_version xcb`
AX_COMPARE_VERSION([$xcbproto_version],[ge],[$xcbproto_required], xcbproto_ok="yes", xcbproto_ok="no")
AC_MSG_RESULT([$xcbproto_ok])

if test $xcbproto_ok = no; then
	AC_MSG_ERROR([libxcb was compiled against xcb-proto $xcbproto_version; it needs to be compiled against version $xcbproto_required or higher])
fi


PKG_CHECK_MODULES(XCB_SHM, xcb-shm)
PKG_CHECK_MODULES(XCB_RENDER, xcb-render)
PKG_CHECK_MODULES(XPROTO, xproto >= 7.0.8)

XCB_AUX_CFLAGS='-I$(top_srcdir)/aux'
XCB_AUX_LIBS='$(top_builddir)/aux/libxcb-aux.la'
XCB_ATOM_CFLAGS='-I$(top_srcdir)/atom -I$(top_builddir)/atom'
XCB_ATOM_LIBS='$(top_builddir)/atom/libxcb-atom.la'
XCB_EVENT_CFLAGS='-I$(top_srcdir)/event'
XCB_EVENT_LIBS='$(top_builddir)/event/libxcb-event.la'
XCB_PROPERTY_CFLAGS='-I$(top_srcdir)/property'
XCB_PROPERTY_LIBS='$(top_builddir)/property/libxcb-property.la'
XCB_ICCCM_CFLAGS='-I$(top_srcdir)/icccm'
XCB_ICCCM_LIBS='$(top_builddir)/icccm/libxcb-icccm.la'
AC_SUBST(XCB_AUX_CFLAGS)
AC_SUBST(XCB_AUX_LIBS)
AC_SUBST(XCB_ATOM_CFLAGS)
AC_SUBST(XCB_ATOM_LIBS)
AC_SUBST(XCB_EVENT_CFLAGS)
AC_SUBST(XCB_EVENT_LIBS)
AC_SUBST(XCB_PROPERTY_CFLAGS)
AC_SUBST(XCB_PROPERTY_LIBS)
AC_SUBST(XCB_ICCCM_CFLAGS)
AC_SUBST(XCB_ICCCM_LIBS)

AC_OUTPUT([Makefile
	aux/Makefile aux/xcb-aux.pc
	reply/Makefile reply/xcb-reply.pc
	image/Makefile image/xcb-image.pc
	atom/Makefile atom/xcb-atom.pc
	event/Makefile event/xcb-event.pc
	keysyms/Makefile keysyms/xcb-keysyms.pc
	property/Makefile property/xcb-property.pc
	icccm/Makefile icccm/xcb-icccm.pc
	renderutil/Makefile renderutil/xcb-renderutil.pc
	xcb_util_intro
	])
