dnl
dnl $XFree86: xc/lib/Xft/configure.in,v 1.2 2002/06/07 17:55:40 keithp Exp $
dnl
dnl Copyright © 2002 Keith Packard, member of The XFree86 Project, Inc.
dnl
dnl Permission to use, copy, modify, distribute, and sell this software and its
dnl documentation for any purpose is hereby granted without fee, provided that
dnl the above copyright notice appear in all copies and that both that
dnl copyright notice and this permission notice appear in supporting
dnl documentation, and that the name of Keith Packard not be used in
dnl advertising or publicity pertaining to distribution of the software without
dnl specific, written prior permission.  Keith Packard makes no
dnl representations about the suitability of this software for any purpose.  It
dnl is provided "as is" without express or implied warranty.
dnl
dnl KEITH PACKARD DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
dnl INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
dnl EVENT SHALL KEITH PACKARD BE LIABLE FOR ANY SPECIAL, INDIRECT OR
dnl CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
dnl DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
dnl TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
dnl PERFORMANCE OF THIS SOFTWARE.
dnl

# Process this file with autoconf to produce a configure script.
AC_INIT(Xft.h, 2.0.0, fonts@xfree86.org)
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_HEADER(config.h)

AC_ARG_WITH(freetype_includes, [  --with-freetype-includes=DIR  Use FreeType includes in DIR], freetype_includes=$withval, freetype_includes=yes)
AC_ARG_WITH(freetype_lib,      [  --with-freetype-lib=DIR       Use FreeType library in DIR], freetype_lib=$withval, freetype_lib=yes)
AC_ARG_WITH(freetype_config,   [  --with-freeetype-config=PROG  Use FreeType configuration program PROG], freetype_config=$withval, freetype_config=yes)
AC_ARG_WITH(fontconfig_includes, [  --with-fontconfig-includes=DIR  Use Fontconfig includes in DIR], fontconfig_includes=$withval, fontconfig_includes=yes)
AC_ARG_WITH(fontconfig_lib,      [  --with-fontconfig-lib=DIR       Use Fontconfig library in DIR], fontconfig_lib=$withval, fontconfig_lib=yes)
AC_ARG_WITH(fontconfig_config,   [  --with-fontconfig-config=PROG  Use Fontconfig configuration program PROG], fontconfig_config=$withval, fontconfig_config=yes)

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PATH_X

PACKAGE_VERSION=2.0.0

# Set major version
PACKAGE_MAJOR=`echo $PACKAGE_VERSION | awk -F . '{ print $1 }'`
PACKAGE_MINOR=`echo $PACKAGE_VERSION | awk -F . '{ print $2 }'`
PACKAGE_REVISION=`echo $PACKAGE_VERSION | awk -F . '{ print $3 }'`
AC_SUBST(PACKAGE_VERSION)
AC_SUBST(PACKAGE_MAJOR)
AC_SUBST(PACKAGE_MINOR)
AC_SUBST(PACKAGE_REVISION)

OBJEXT="o"
AC_SUBST(OBJEXT)

AC_DEFINE_UNQUOTED(PACKAGE_MAJOR,$PACKAGE_MAJOR)
AC_DEFINE_UNQUOTED(PACKAGE_MINOR,$PACKAGE_MINOR)
AC_DEFINE_UNQUOTED(PACKAGE_REVISION,$PACKAGE_REVISION)

if test "$GCC" = "yes"; then
    MKSHLIB='$(CC) $(CFLAGS) $(DSO_PIC_CFLAGS) $(DSO_LDOPTS) -o $@'
    DSO_LDOPTS='-shared -Wl,-h,`echo $@ | sed '"'s/\.so\.\([[0-9][0-9]]*\).*/.so.\1/'"'`'
    DSO_CFLAGS=''
    DSO_PIC_CFLAGS='-fPIC -DPIC'
else

    AC_CANONICAL_HOST
    
    MKSHLIB='$(LD) $(DSO_LDOPTS) -o $@'
    DSO_CFLAGS=''
    DSO_PIC_CFLAGS='-KPIC -DPIC'
    
    case "$host" in
    *solaris*)
    	DSO_LDOPTS='-G -z text -z defs -h $@'
	LIBS="$LIBS -lc"
	;;
    *)
    	DSO_LDOPTS='-shared -h $@'
    	;;
    esac
fi

AC_SUBST(MKSHLIB)
AC_SUBST(DSO_LDOPTS)
AC_SUBST(DSO_CFLAGS)
AC_SUBST(DSO_PIC_CFLAGS)

#
# Check X configuration
#
case "$have_x" in
yes)
	XRENDER_CFLAGS="-I$x_includes"
	XRENDER_LIBS="-L$x_libraries -lXrender"

	saved_LIBS="$LIBS"
	LIBS="$LIBS $XRENDER_LIBS"
	saved_CPPFLAGS="$CPPFLAGS"
	CPPFLAGS="$CPPFLAGS $XRENDER_CFLAGS"
	AC_CHECK_HEADERS(X11/extensions/Xrender.h)
	
	case "$ac_cv_header_X11_extensions_Xrender_h" in
	no)
		CPPFLAGS="$saved_CPPFLAGS"
		LIBS="$saved_LIBS"
		;;
	yes)
		AC_CHECK_FUNCS(XRenderQueryVersion)
		case "$ac_cv_func_XRenderQueryVersion" in
		no)
			CPPFLAGS="$saved_CPPFLAGS"
			LIBS="$saved_LIBS"
			;;
		yes)
			AC_DEFINE(HAVE_XRENDER)
			AC_SUBST(XRENDER_CFLAGS)
			AC_SUBST(XRENDER_LIBS)
			;;
		esac
		;;
	esac

	;;
esac

#
# Check freetype configuration
#
case "$freetype_config" in
no)
	;;
yes)
	AC_CHECK_PROG(ft_config,freetype-config,freetype-config,no)
	;;
*)
	ft_config="$freetype_config"
	;;
esac

case "$freetype_includes" in
no)
	FREETYPE_CFLAGS=""
	;;
yes)
	case "$ft_config" in
	no)
		FREETYPE_CFLAGS=""
		;;
	*)
		FREETYPE_CFLAGS="`$ft_config --cflags`"
		;;
	esac
	;;
*)
	FREETYPE_CFLAGS="-I$freetype_includes"
	;;
esac

case "$freetype_lib" in
no)
	freetype_lib=""
	;;
yes)
	case "$ft_config" in
	no)
		freetype_lib=""
		;;
	*)
		FREETYPE_LIBS="`$ft_config --libs`"
		;;
	esac
	;;
*)
	FREETYPE_LIBS="-L$freetype_lib -lfreetype"
	;;
esac

saved_LIBS="$LIBS"
LIBS="$LIBS $FREETYPE_LIBS"
saved_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $FREETYPE_CFLAGS"
AC_CHECK_HEADERS(freetype/freetype.h)

case "$ac_cv_header_freetype_freetype_h" in
no)
	CPPFLAGS="$saved_CPPFLAGS"
	LIBS="$saved_LIBS"
	;;
yes)
	AC_CHECK_FUNCS(FT_Init_FreeType)
	case "$ac_cv_func_FT_Init_FreeType" in
	no)
		CPPFLAGS="$saved_CPPFLAGS"
		LIBS="$saved_LIBS"
		;;
	yes)
		AC_DEFINE(HAVE_FREETYPE)
		AC_SUBST(FREETYPE_CFLAGS)
		AC_SUBST(FREETYPE_LIBS)
		;;
	esac
	;;
esac

#
# Check fontconfig configuration
#
case "$fontconfig_config" in
no)
	;;
yes)
	AC_CHECK_PROG(fc_config,fontconfig-config,fontconfig-config,no)
	;;
*)
	fc_config="$fontconfig_config"
	;;
esac

case "$fontconfig_includes" in
no)
	FONTCONFIG_CFLAGS=""
	;;
yes)
	case "$fc_config" in
	no)
		FONTCONFIG_CFLAGS=""
		;;
	*)
		FONTCONFIG_CFLAGS="`$fc_config --cflags`"
		;;
	esac
	;;
*)
	FONTCONFIG_CFLAGS="-I$fontconfig_includes"
	;;
esac

case "$fontconfig_lib" in
no)
	fontconfig_lib=""
	;;
yes)
	case "$fc_config" in
	no)
		fontconfig_lib=""
		;;
	*)
		FONTCONFIG_LIBS="`$fc_config --libs`"
		;;
	esac
	;;
*)
	FONTCONFIG_LIBS="-L$fontconfig_lib -lfontconfig"
	;;
esac

saved_LIBS="$LIBS"
LIBS="$LIBS $FONTCONFIG_LIBS"
saved_CPPFLAGS="$CPPFLAGS"
CPPFLAGS="$CPPFLAGS $FONTCONFIG_CFLAGS"
AC_CHECK_HEADERS(fontconfig/fontconfig.h)

case "$ac_cv_header_fontconfig_fontconfig_h" in
no)
	CPPFLAGS="$saved_CPPFLAGS"
	LIBS="$saved_LIBS"
	;;
yes)
	AC_CHECK_FUNCS(FcInit)
	case "$ac_cv_func_FcInit" in
	no)
		CPPFLAGS="$saved_CPPFLAGS"
		LIBS="$saved_LIBS"
		;;
	yes)
		AC_DEFINE(HAVE_FONTCONFIG)
		AC_SUBST(FONTCONFIG_CFLAGS)
		AC_SUBST(FONTCONFIG_LIBS)
		;;
	esac
	;;
esac


AC_OUTPUT([Makefile
	   xft.pc
	   xft-config])
