AC_PREREQ([2.57])
AC_INIT([encodings], [1.0.0], [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg])
AM_INIT_AUTOMAKE([foreign dist-bzip2])

# mkfontscale

AC_PATH_PROG(MKFONTSCALE, mkfontscale)
if [[ -z $MKFONTSCALE ]]; then
   AC_MSG_ERROR([mkfontscale is needed to build fontenc. FIXME: when we
		 know where it is going to live, put a more informative
		 comment here])
fi

# gzip

AC_PATH_PROG(GZIP, gzip)
if [[ -z $GZIP ]]; then
   AC_MSG_ERROR([gzip is needed to compress font encodings])
fi

m4_ifdef([AS_HELP_STRING], , [m4_define([AS_HELP_STRING], m4_defn([AC_HELP_STRING]))])

DEFAULT_ENCODINGSDIR=${libdir}/X11/fonts/encodings
AC_ARG_WITH(encodingsdir,
	AS_HELP_STRING([--with-encodingsdir=ENCODINGSDIR], [Path to install font encodings]),
	[ENCODINGSDIR="$withval"],
	[ENCODINGSDIR="$DEFAULT_ENCODINGSDIR"])
AC_SUBST(ENCODINGSDIR)

XORG_RELEASE_VERSION

AC_OUTPUT([Makefile large/Makefile])
