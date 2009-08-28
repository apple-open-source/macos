AC_PREREQ(2.57)
AC_INIT([xcb-util],0.3.3,[xcb@lists.freedesktop.org])
AC_CONFIG_SRCDIR([Makefile.am])
AM_INIT_AUTOMAKE([foreign dist-bzip2])

AC_CHECK_PROGS(M4, m4, [no])
if test $M4 = "no"; then
	AC_MSG_ERROR([Can't find m4, please install it and try again])
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

xcbincludedir='${includedir}/xcb'
AC_SUBST(xcbincludedir)

pkgconfigdir='${libdir}/pkgconfig'
AC_SUBST(pkgconfigdir)

PKG_CHECK_MODULES(XCB, xcb >= 1.0)
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
	wm/Makefile wm/xcb-wm.pc
	renderutil/Makefile renderutil/xcb-renderutil.pc
	xcb_util_intro
	])
