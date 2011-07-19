# Copyright © 2009 Intel Corporation
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice (including the next
# paragraph) shall be included in all copies or substantial portions of the
# Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.
#
# Authors:
#    Eric Anholt <eric@anholt.net>

dnl Process this file with autoconf to create configure.

AC_PREREQ([2.59])
AC_INIT([mesa-demos], [8.0.1],
    [https://bugs.freedesktop.org/enter_bug.cgi?product=Mesa])
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([dist-bzip2])

dnl Check for progs
AC_PROG_CPP
AC_PROG_CC
AC_PROG_CXX
AC_PROG_LIBTOOL
AM_PROG_CC_C_O

# Enable quiet compiles on automake 1.11.
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Make sure the pkg-config macros are defined
m4_ifndef([PKG_PROG_PKG_CONFIG],
    [m4_fatal([Could not locate the pkg-config autoconf macros.
  These are usually located in /usr/share/aclocal/pkg.m4. If your macros
  are in a different location, try setting the environment variable
  ACLOCAL="aclocal -I/other/macro/dir" before running autoreconf.])])
PKG_PROG_PKG_CONFIG()

dnl Get the pkg-config definitions for libGL.  We include a fallback
dnl path for GL implementation that don't provide a .pc file
PKG_CHECK_MODULES(GL, [gl], [], [
		  AC_CHECK_HEADER([GL/gl.h],
		  		  [],
		  		  AC_MSG_ERROR([GL not found]))
		  AC_CHECK_LIB([GL],
			       [glBegin],
			       [GL_LIBS=-lGL],
			       AC_MSG_ERROR([GL required]))
		  ])

DEMO_CFLAGS="$DEMO_CFLAGS $GL_CFLAGS"
DEMO_LIBS="$DEMO_LIBS $GL_LIBS"

dnl Check for GLUT
GLUT_CFLAGS=""
GLUT_LIBS=-lglut
glut_enabled=yes
AC_ARG_WITH([glut],
	[AS_HELP_STRING([--with-glut=DIR],
			[glut install directory])],
	[GLUT_CFLAGS="-I$withval/include"
	 GLUT_LIBS="-L$withval/lib -lglut"])
AC_CHECK_HEADER([GL/glut.h],
		[],
		[glut_enabled=no])
AC_CHECK_LIB([glut],
		[glutInit],
		[],
		[glut_enabled=no])

GLEW_CFLAGS=""
GLEW_LIBS="-lGLEW"
dnl Include a fallback path for GLEW for the moment while not all distros
dnl have picked up the .pc file.
PKG_CHECK_MODULES(GLEW, [glew], [], [
		  AC_CHECK_HEADER([GL/glew.h],
		  [],
		  AC_MSG_ERROR([GLEW required]))
		  AC_CHECK_LIB([GLEW],
			       [glewInit],
			       [],
			       AC_MSG_ERROR([GLEW required]))
		  ])
DEMO_CFLAGS="$DEMO_CFLAGS $GLEW_CFLAGS"
DEMO_LIBS="$DEMO_LIBS $GLEW_LIBS"

# LIBS was set by AC_CHECK_LIB above
LIBS=""

PKG_CHECK_MODULES(GLU, [glu], [],
		  [AC_CHECK_HEADER([GL/glu.h],
				   [],
		  		   AC_MSG_ERROR([GLU not found]))
		   AC_CHECK_LIB([GLU],
				[gluBeginCurve],
				[GLU_LIBS=-lGLU],
				AC_MSG_ERROR([GLU required])) ])

DEMO_CFLAGS="$DEMO_CFLAGS $GLU_CFLAGS"
DEMO_LIBS="$DEMO_LIBS $GLU_LIBS"

PKG_CHECK_MODULES(EGL, [egl], [egl_enabled=yes], [egl_enabled=no])
PKG_CHECK_MODULES(GLESV1, [glesv1_cm], [glesv1_enabled=yes], [glesv1_enabled=no])
PKG_CHECK_MODULES(GLESV2, [glesv2], [glesv2_enabled=yes], [glesv2_enabled=no])
PKG_CHECK_MODULES(VG, [vg], [vg_enabled=yes], [vg_enabled=no])
PKG_CHECK_MODULES(OSMESA, [osmesa], [osmesa_enabled=yes], [osmesa_enabled=no])
dnl The OSMesa .pc uses OSMesa32, while we want to build with other versions
dnl too.
OSMESA32_LIBS=$OSMESA_LIBS
OSMESA16_LIBS=`echo $OSMESA_LIBS | sed 's|32|16|g'`
OSMESA_LIBS=`echo $OSMESA_LIBS | sed 's|32||g'`

dnl Compiler macros
case "$host_os" in
linux*|*-gnu*|gnu*)
    DEMO_CFLAGS="$DEMO_CFLAGS -D_GNU_SOURCE -DPTHREADS"
    ;;
solaris*)
    DEMO_CFLAGS="$DEFINES -DPTHREADS -DSVR4"
    ;;
cygwin*)
    DEMO_CFLAGS="$DEFINES -DPTHREADS"
    ;;
esac

dnl Set up C warning flags.  Copy of XORG_CWARNFLAGS.
if  test "x$GCC" = xyes ; then
    CWARNFLAGS="-Wall -Wpointer-arith -Wstrict-prototypes -Wmissing-prototypes \
-Wmissing-declarations -Wnested-externs -fno-strict-aliasing \
-Wbad-function-cast"
    case `$CC -dumpversion` in
    3.4.* | 4.*)
	CWARNFLAGS="$CWARNFLAGS -Wold-style-definition -Wdeclaration-after-statement"
	;;
    esac
else
    AC_CHECK_DECL([__SUNPRO_C], [SUNCC="yes"], [SUNCC="no"])
    if test "x$SUNCC" = "xyes"; then
	CWARNFLAGS="-v"
    fi
fi
DEMO_CFLAGS="$DEMO_CFLAGS $CWARNFLAGS"

PKG_CHECK_MODULES(X11, [x11 xext], [x11_enabled=yes], [enable_x11=no])

mesa_source_enabled=no
AC_ARG_WITH([mesa-source],
	[AS_HELP_STRING([--with-mesa-source=DIR],
			[Mesa source directory for finding gl_API.xml])],
	[mesa_source_enabled="yes"
	 mesa_source_dir="$withval"],
	[mesa_source_enabled="no"])
MESA_GLAPI="$mesa_source_dir/src/mesa/glapi"

dnl
dnl Program library dependencies
dnl    Only libm is added here if necessary as the libraries should
dnl    be pulled in by the linker
dnl
case "$host_os" in
solaris*)
    DEMO_LIBS="$DEMO_LIBS -lX11 -lsocket -lnsl -lm"
    ;;
cygwin*)
    DEMO_LIBS="$DEMO_LIBS -lX11"
    ;;
*)
    DEMO_LIBS="$DEMO_LIBS -lm"
    ;;
esac

AC_SUBST([DEMO_CFLAGS])
AC_SUBST([DEMO_LIBS])
AC_SUBST([EGL_CFLAGS])
AC_SUBST([EGL_LIBS])
AC_SUBST([GLESV1_CFLAGS])
AC_SUBST([GLESV1_LIBS])
AC_SUBST([GLESV2_CFLAGS])
AC_SUBST([GLESV2_LIBS])
AC_SUBST([GLUT_CFLAGS])
AC_SUBST([GLUT_LIBS])
AC_SUBST([X11_CFLAGS])
AC_SUBST([X11_LIBS])
AC_SUBST([OSMESA_CFLAGS])
AC_SUBST([OSMESA_LIBS])
AC_SUBST([OSMESA16_LIBS])
AC_SUBST([OSMESA32_LIBS])
AC_SUBST([MESA_GLAPI])

AM_CONDITIONAL(HAVE_EGL, test "x$egl_enabled" = "xyes")
AM_CONDITIONAL(HAVE_GLESV1, test "x$glesv1_enabled" = "xyes")
AM_CONDITIONAL(HAVE_GLESV2, test "x$glesv2_enabled" = "xyes")
AM_CONDITIONAL(HAVE_VG, test "x$vg_enabled" = "xyes")
AM_CONDITIONAL(HAVE_GLUT, test "x$glut_enabled" = "xyes")
AM_CONDITIONAL(HAVE_X11, test "x$x11_enabled" = "xyes")
AM_CONDITIONAL(HAVE_OSMESA, test "x$osmesa_enabled" = "xyes")
AM_CONDITIONAL(BUILD_GLTRACE, false)
AM_CONDITIONAL(HAVE_MESA_SOURCE, test "x$mesa_source_enabled" = "xyes")

AC_OUTPUT([
	Makefile
	src/Makefile
	src/demos/Makefile
	src/egl/Makefile
	src/egl/eglut/Makefile
	src/egl/opengl/Makefile
	src/egl/opengles1/Makefile
	src/egl/opengles2/Makefile
	src/egl/openvg/Makefile
	src/egl/openvg/trivial/Makefile
	src/fp/Makefile
	src/fpglsl/Makefile
	src/glsl/Makefile
	src/gs/Makefile
	src/images/Makefile
	src/objviewer/Makefile
	src/osdemos/Makefile
	src/perf/Makefile
	src/rbug/Makefile
	src/redbook/Makefile
	src/samples/Makefile
	src/slang/Makefile
	src/tests/Makefile
	src/tools/Makefile
	src/tools/trace/Makefile
	src/trivial/Makefile
	src/util/Makefile
	src/vp/Makefile
	src/vpglsl/Makefile
	src/wgl/Makefile
	src/xdemos/Makefile
])
