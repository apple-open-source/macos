dnl Process this file with autoconf to create configure.

AC_PREREQ([2.57])

AC_INIT([libXaw],
	1.0.4,
	[https://bugs.freedesktop.org/enter_bug.cgi?product=xorg],
	libXaw)

AM_INIT_AUTOMAKE([dist-bzip2])

AM_CONFIG_HEADER(config.h)

# Check for progs
AC_PROG_CC
AC_PROG_LIBTOOL

# Need to call this explicitly since the first call to PKG_CHECK_MODULES
# is in an if statement, and later calls would break if it's skipped.
PKG_PROG_PKG_CONFIG

#
# fix libtool to set SONAME to libXaw.so.$major
#
AC_MSG_CHECKING([hacks in libtool for libXaw SONAME])
if grep "xorglibxawname" libtool > /dev/null ; then
	AC_MSG_RESULT([already done])
else
	ed libtool << \EOF
/^soname_spec/i
# X.Org hack to match monolithic Xaw SONAME
xorglibxawname="libXaw"
.
/^soname_spec/s/libname/xorglibxawname/
w
q
EOF
	AC_MSG_RESULT([fixed])
fi

# OSX/Win32 rules are different.
platform_win32=no
platform_darwin=no
LIBEXT=so
case $host_os in
    cygwin*|mingw*)
	platform_win32=yes
	;;
    darwin*)
	LIBEXT=dylib
	platform_darwin=yes
	;;
esac
AC_SUBST(LIBEXT)
AM_CONDITIONAL(PLATFORM_WIN32, test "x$platform_win32" = "xyes")
AM_CONDITIONAL(PLATFORM_DARWIN, test "x$platform_darwin" = "xyes")

# Whether to build Xaw6

AC_ARG_ENABLE(xaw6, AC_HELP_STRING([--disable-xaw6],
				[Disable building of libXaw.so.6]),
			[build_v6=$enableval], [build_v6=yes])

if test "x$build_v6" = xyes; then
   PKG_CHECK_MODULES(XAW6, xproto x11 xext xextproto xt xmu)

   AC_SUBST(XAW6_CFLAGS)
   AC_SUBST(XAW6_LIBS)
fi


# Whether to build Xaw7

AC_ARG_ENABLE(xaw7, AC_HELP_STRING([--disable-xaw7],
				[Disable building of libXaw.so.7]),
			[build_v7=$enableval], [build_v7=yes])

if test "x$build_v7" = xyes; then
   PKG_CHECK_MODULES(XAW7, xproto x11 xext xextproto xt xmu xpm)

   AC_SUBST(XAW7_CFLAGS)
   AC_SUBST(XAW7_LIBS)
fi


# Whether to build Xaw8

AC_ARG_ENABLE(xaw8, AC_HELP_STRING([--disable-xaw8],
				[Disable building of libXaw.so.8]),
			[build_v8=$enableval], [build_v8=auto])

if test "x$build_v8" = xyes || test "x$build_v8" = xauto; then
   PKG_CHECK_MODULES(XPRINT, xp, xprint_found=yes, xprint_found=no)

   if test "x$xprint_found" = xno; then
      if test "x$build_v8" = xyes; then
          AC_ERROR([
	    Xprint is required to build libXaw.so.8. You can disable
	    building of libXaw.so.8 by passing --disable-xaw8 to
	    configure])
      fi
      build_v8=no
   else
       PKG_CHECK_MODULES(XAW8, xproto x11 xext xextproto xt xmu xpm xp printproto xau)
       AC_SUBST(XAW8_CFLAGS)
       AC_SUBST(XAW8_LIBS)
       build_v8=yes
   fi
fi

AM_CONDITIONAL(BUILD_XAW6, [test x$build_v6 = xyes])
AM_CONDITIONAL(BUILD_XAW7, [test x$build_v7 = xyes])
AM_CONDITIONAL(BUILD_XAW8, [test x$build_v8 = xyes])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADER(wctype.h,
       	AC_DEFINE([HAS_WCTYPE_H],1,
			[Define to 1 if you have the <wctype.h> header file.]))
AC_CHECK_HEADER(wchar.h,
       	AC_DEFINE([HAS_WCHAR_H],1,
			[Define to 1 if you have the <wchar.h> header file.]))
AC_CHECK_HEADER(widec.h, [],
       	AC_DEFINE([NO_WIDEC_H],1,
			[Define to 1 if you DO NOT have the <widec.h> header file.]))

# Checks for functions
AC_CHECK_FUNCS([iswalnum])

XORG_MANPAGE_SECTIONS
XORG_RELEASE_VERSION

AC_OUTPUT([Makefile
	  include/Makefile
	  man/Makefile
	  src/Makefile])

if test "x$build_v6" = xyes; then
   AC_OUTPUT(xaw6.pc)
fi

if test "x$build_v7" = xyes; then
   AC_OUTPUT(xaw7.pc)
fi

if test "x$build_v8" = xyes; then
   AC_OUTPUT(xaw8.pc)
fi
