#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.57)
AC_INIT([xpyb],
        1.2,
        [xcb@lists.freedesktop.org])
AC_CONFIG_SRCDIR([xpyb.pc.in])
AM_INIT_AUTOMAKE([foreign dist-bzip2])

AM_PATH_PYTHON([2.5])
AC_MSG_CHECKING([for python include dir])
PYTHON_INCLUDE=`$PYTHON -c "from distutils import sysconfig; \
			    print sysconfig.get_python_inc()"`
if test -z "${PYTHON_INCLUDE}"; then
   AC_MSG_ERROR([Failed to find Python include directory.
		 Check your Python installation.])
fi

AC_MSG_RESULT([$PYTHON_INCLUDE])
AC_SUBST([PYTHON_INCLUDE])

AC_CONFIG_HEADERS([src/config.h])

AC_DISABLE_STATIC
AC_PROG_LIBTOOL
AC_PROG_CC

# Checks for pkg-config packages
PKG_CHECK_MODULES(XCBPROTO, xcb-proto >= 1.1)
PKG_CHECK_MODULES(LIBXCB, xcb >= 1.1)

# Find the xcb-proto protocol descriptions
AC_MSG_CHECKING([for xcb-proto include dir])
XCBPROTO_XCBINCLUDEDIR=`$PKG_CONFIG --variable=xcbincludedir xcb-proto`
AC_MSG_RESULT($XCBPROTO_XCBINCLUDEDIR)
AC_SUBST(XCBPROTO_XCBINCLUDEDIR)

# Find the xcbgen Python package
AC_MSG_CHECKING([for xcb-proto python dir])
XCBPROTO_XCBPYTHONDIR=`$PKG_CONFIG --variable=pythondir xcb-proto`
AC_MSG_RESULT($XCBPROTO_XCBPYTHONDIR)
AC_SUBST(XCBPROTO_XCBPYTHONDIR)

AC_HEADER_STDC
if  test "x$GCC" = xyes ; then
    CWARNFLAGS="-Wall -Wmissing-declarations"
else
    AC_CHECK_DECL([__SUNPRO_C], [SUNCC="yes"], [SUNCC="no"])
    if test "x$SUNCC" = "xyes"; then
	CWARNFLAGS="-v"
    fi
fi
AC_SUBST(CWARNFLAGS)

AC_ARG_ENABLE(composite, AS_HELP_STRING([--enable-composite], [Build XCB Composite Extension (default: yes)]), [BUILD_COMPOSITE=$enableval], [BUILD_COMPOSITE=yes])
AM_CONDITIONAL(BUILD_COMPOSITE, [test "x$BUILD_COMPOSITE" = xyes])
AC_ARG_ENABLE(damage, AS_HELP_STRING([--enable-damage], [Build XCB Damage Extension (default: yes)]), [BUILD_DAMAGE=$enableval], [BUILD_DAMAGE=yes])
AM_CONDITIONAL(BUILD_DAMAGE, [test "x$BUILD_DAMAGE" = xyes])
AC_ARG_ENABLE(dpms, AS_HELP_STRING([--enable-dpms], [Build XCB DPMS Extension (default: yes)]), [BUILD_DPMS=$enableval], [BUILD_DPMS=yes])
AM_CONDITIONAL(BUILD_DPMS, [test "x$BUILD_DPMS" = xyes])
AC_ARG_ENABLE(glx, AS_HELP_STRING([--enable-glx], [Build XCB GLX Extension (default: yes)]), [BUILD_GLX=$enableval], [BUILD_GLX=yes])
AM_CONDITIONAL(BUILD_GLX, [test "x$BUILD_GLX" = xyes])
AC_ARG_ENABLE(randr, AS_HELP_STRING([--enable-randr], [Build XCB RandR Extension (default: yes)]), [BUILD_RANDR=$enableval], [BUILD_RANDR=yes])
AM_CONDITIONAL(BUILD_RANDR, [test "x$BUILD_RANDR" = xyes])
AC_ARG_ENABLE(record, AS_HELP_STRING([--enable-record], [Build XCB Record Extension (default: yes)]), [BUILD_RECORD=$enableval], [BUILD_RECORD=yes])
AM_CONDITIONAL(BUILD_RECORD, [test "x$BUILD_RECORD" = xyes])
AC_ARG_ENABLE(render, AS_HELP_STRING([--enable-render], [Build XCB Render Extension (default: yes)]), [BUILD_RENDER=$enableval], [BUILD_RENDER=yes])
AM_CONDITIONAL(BUILD_RENDER, [test "x$BUILD_RENDER" = xyes])
AC_ARG_ENABLE(res, AS_HELP_STRING([--enable-res], [Build XCB X-Resource Extension (default: yes)]), [BUILD_RES=$enableval], [BUILD_RES=yes])
AM_CONDITIONAL(BUILD_RES, [test "x$BUILD_RES" = xyes])
AC_ARG_ENABLE(screensaver, AS_HELP_STRING([--enable-screensaver], [Build XCB Screensaver Extension (default: yes)]), [BUILD_SCREENSAVER=$enableval], [BUILD_SCREENSAVER=yes])
AM_CONDITIONAL(BUILD_SCREENSAVER, [test "x$BUILD_SCREENSAVER" = xyes])
AC_ARG_ENABLE(shape, AS_HELP_STRING([--enable-shape], [Build XCB Shape Extension (default: yes)]), [BUILD_SHAPE=$enableval], [BUILD_SHAPE=yes])
AM_CONDITIONAL(BUILD_SHAPE, [test "x$BUILD_SHAPE" = xyes])
AC_ARG_ENABLE(shm, AS_HELP_STRING([--enable-shm], [Build XCB Shm Extension (default: yes)]), [BUILD_SHM=$enableval], [BUILD_SHM=yes])
AM_CONDITIONAL(BUILD_SHM, [test "x$BUILD_SHM" = xyes])
AC_ARG_ENABLE(sync, AS_HELP_STRING([--enable-sync], [Build XCB Sync Extension (default: yes)]), [BUILD_SYNC=$enableval], [BUILD_SYNC=yes])
AM_CONDITIONAL(BUILD_SYNC, [test "x$BUILD_SYNC" = xyes])
AC_ARG_ENABLE(xevie, AS_HELP_STRING([--enable-xevie], [Build XCB Xevie Extension (default: yes)]), [BUILD_XEVIE=$enableval], [BUILD_XEVIE=yes])
AM_CONDITIONAL(BUILD_XEVIE, [test "x$BUILD_XEVIE" = xyes])
AC_ARG_ENABLE(xf86dri, AS_HELP_STRING([--enable-xf86dri], [Build XCB XFree86-DRI Extension (default: yes)]), [BUILD_XF86DRI=$enableval], [BUILD_XF86DRI=yes])
AM_CONDITIONAL(BUILD_XF86DRI, [test "x$BUILD_XF86DRI" = xyes])
AC_ARG_ENABLE(xfixes, AS_HELP_STRING([--enable-xfixes], [Build XCB XFixes Extension (default: yes)]), [BUILD_XFIXES=$enableval], [BUILD_XFIXES=yes])
AM_CONDITIONAL(BUILD_XFIXES, [test "x$BUILD_XFIXES" = xyes])
AC_ARG_ENABLE(xinerama, AS_HELP_STRING([--enable-xinerama], [Build XCB Xinerama Extension (default: yes)]), [BUILD_XINERAMA=$enableval], [BUILD_XINERAMA=yes])
AM_CONDITIONAL(BUILD_XINERAMA, [test "x$BUILD_XINERAMA" = xyes])
AC_ARG_ENABLE(xinput, AS_HELP_STRING([--enable-xinput], [Build XCB XInput Extension (EXPERIMENTAL) (default: no)]), [BUILD_XINPUT=$enableval], [BUILD_XINPUT=no])
AM_CONDITIONAL(BUILD_XINPUT, [test "x$BUILD_XINPUT" = xyes])
AC_ARG_ENABLE(xprint, AS_HELP_STRING([--enable-xprint], [Build XCB Xprint Extension (default: yes)]), [BUILD_XPRINT=$enableval], [BUILD_XPRINT=yes])
AM_CONDITIONAL(BUILD_XPRINT, [test "x$BUILD_XPRINT" = xyes])
AC_ARG_ENABLE(xselinux, AS_HELP_STRING([--enable-xselinux], [Build XCB SELinux Extension (default: no)]), [BUILD_SELINUX=$enableval], [BUILD_SELINUX=no])
AM_CONDITIONAL(BUILD_SELINUX, [test "x$BUILD_SELINUX" = xyes])
AC_ARG_ENABLE(xtest, AS_HELP_STRING([--enable-xtest], [Build XCB XTEST Extension (default: yes)]), [BUILD_XTEST=$enableval], [BUILD_XTEST=yes])
AM_CONDITIONAL(BUILD_XTEST, [test "x$BUILD_XTEST" = xyes])
AC_ARG_ENABLE(xv, AS_HELP_STRING([--enable-xv], [Build XCB Xv Extension (default: yes)]), [BUILD_XV=$enableval], [BUILD_XV=yes])
AM_CONDITIONAL(BUILD_XV, [test "x$BUILD_XV" = xyes])
AC_ARG_ENABLE(xvmc, AS_HELP_STRING([--enable-xvmc], [Build XCB XvMC Extension (default: yes)]), [BUILD_XVMC=$enableval], [BUILD_XVMC=yes])
AM_CONDITIONAL(BUILD_XVMC, [test "x$BUILD_XVMC" = xyes])


AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile])
AC_CONFIG_FILES([xpyb.pc])

AC_OUTPUT

dnl Configuration output

echo ""
echo "  Package: ${PACKAGE_NAME} ${PACKAGE_VERSION}"
echo ""
echo "  Used CFLAGS:"
echo "    CPPFLAGS............: ${CPPFLAGS}"
echo "    CFLAGS..............: ${CFLAGS}"
echo "    Warning CFLAGS......: ${CWARNFLAGS}"
echo ""
echo "  Installation:"
echo "    Prefix..............: ${prefix}"
echo ""
