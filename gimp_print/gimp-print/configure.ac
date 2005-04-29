dnl $Id: configure.ac,v 1.3 2004/12/23 01:41:46 jlovell Exp $
dnl Copyright (C) 2001-2004 Roger Leigh
dnl 
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2, or (at your option)
dnl any later version.
dnl 
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl 
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

dnl Process this file with autoconf to produce a configure script.

dnl MAKING RELEASES (a step-by-step guide!)
dnl ===============
dnl
dnl Since the last release:
dnl 1. if only source code (not the interface) has changed, set
dnl      GIMPPRINT_MICRO_VERSION += 1;
dnl      GIMPPRINT_INTERFACE_AGE += 1;
dnl 2. if any functions have been added, removed, or changed, set
dnl      GIMPPRINT_INTERFACE_AGE = 0;
dnl      GIMPPRINT_CURRENT_INTERFACE += 1;
dnl 3. if interfaces have been added, set
dnl      GIMPPRINT_BINARY_AGE += 1;
dnl 4. if interfaces have been removed, set
dnl      GIMPPRINT_BINARY_AGE = 0;
dnl
dnl For more detailed information, see the libtool info documentation.
dnl
pushdef([GIMPPRINT_NAME],              [gimp-print])
pushdef([GIMPPRINT_MAJOR_VERSION],     [5])
pushdef([GIMPPRINT_MINOR_VERSION],     [0])
pushdef([GIMPPRINT_MICRO_VERSION],     [0])
pushdef([GIMPPRINT_EXTRA_VERSION],     [-beta2])
pushdef([GIMPPRINT_CURRENT_INTERFACE], [7])
pushdef([GIMPPRINT_INTERFACE_AGE],     [0])
pushdef([GIMPPRINT_BINARY_AGE],        [0])
pushdef([GIMPPRINT_VERSION], GIMPPRINT_MAJOR_VERSION.GIMPPRINT_MINOR_VERSION.GIMPPRINT_MICRO_VERSION[]GIMPPRINT_EXTRA_VERSION)

AC_INIT(GIMPPRINT_NAME, GIMPPRINT_VERSION, [gimp-print-devel@lists.sourceforge.net])
AC_PREREQ(2.53)
AC_REVISION($Revision: 1.3 $)
AC_CONFIG_SRCDIR(src/main/print-vars.c)
AC_CONFIG_AUX_DIR(scripts)
STP_INIT
AC_COPYRIGHT(
[
Parts of this configure script come from the 'configure.ac' file, and
are not based on freely-redistributable m4 macros; you can redistribute
them and/or modify them under the terms of the GNU General Public
as published by the Free Software Foundation; either version 2, or (at
your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
])

[GIMPPRINT_MAJOR_VERSION]=GIMPPRINT_MAJOR_VERSION
[GIMPPRINT_MINOR_VERSION]=GIMPPRINT_MINOR_VERSION
[GIMPPRINT_MICRO_VERSION]=GIMPPRINT_MICRO_VERSION
[GIMPPRINT_EXTRA_VERSION]=GIMPPRINT_EXTRA_VERSION
[GIMPPRINT_CURRENT_INTERFACE]=GIMPPRINT_CURRENT_INTERFACE
[GIMPPRINT_INTERFACE_AGE]=GIMPPRINT_INTERFACE_AGE
[GIMPPRINT_BINARY_AGE]=GIMPPRINT_BINARY_AGE
[GIMPPRINT_VERSION]=GIMPPRINT_VERSION
popdef([GIMPPRINT_MAJOR_VERSION])
popdef([GIMPPRINT_MINOR_VERSION])
popdef([GIMPPRINT_MICRO_VERSION])
popdef([GIMPPRINT_EXTRA_VERSION])
popdef([GIMPPRINT_CURRENT_INTERFACE])
popdef([GIMPPRINT_INTERFACE_AGE])
popdef([GIMPPRINT_BINARY_AGE])

AC_SUBST(GIMPPRINT_MAJOR_VERSION)
AC_SUBST(GIMPPRINT_MINOR_VERSION)
AC_SUBST(GIMPPRINT_MICRO_VERSION)
AC_SUBST(GIMPPRINT_CURRENT_INTERFACE)
AC_SUBST(GIMPPRINT_INTERFACE_AGE)
AC_SUBST(GIMPPRINT_BINARY_AGE)
AC_SUBST(GIMPPRINT_VERSION)

dnl libtool versioning
LT_RELEASE=$GIMPPRINT_MAJOR_VERSION.$GIMPPRINT_MINOR_VERSION
LT_CURRENT=$GIMPPRINT_CURRENT_INTERFACE
LT_REVISION=$GIMPPRINT_INTERFACE_AGE
LT_AGE=$GIMPPRINT_BINARY_AGE
AC_SUBST(LT_RELEASE)
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

dnl Initialize automake stuff.
AM_INIT_AUTOMAKE(GIMPPRINT_NAME, GIMPPRINT_VERSION)
popdef([GIMPPRINT_NAME])
popdef([GIMPPRINT_VERSION])


dnl In the following script, there are a the following variants
dnl of cflags and variables
dnl
dnl *_CFLAGS:   cflags for compiling libraries and example progs
dnl *_LIBS:     libraries for linking programs. ONLY to be used
dnl             to generate ${gimpprint_libs}
dnl *_LIBDEPS:  libraries for linking libraries against
dnl *_libdeps:  library dependencies for pkg-config
dnl *_cflags:   cflags for pkg-config
dnl *_libs:     libs for pkg-config

GIMPPRINT_LIBS="-lgimpprint"
GIMPPRINT_UI_LIBS="-lgimpprintui"
GIMPPRINTUI2_LIBS="-lgimpprintui2"
gimpprint_version=${GIMPPRINT_VERSION}
gimpprint_ui_version=${GIMPPRINT_VERSION}
gimpprintui2_version=${GIMPPRINT_VERSION}

GIMPPRINT_UI_LIBDEPS="${GIMPPRINT_UI_LIBDEPS} \$(GIMPPRINT_LIBS)"
GIMPPRINTUI2_LIBDEPS="${GIMPPRINTUI2_LIBDEPS} \$(GIMPPRINT_LIBS)"

dnl Specify a configuration file.
AM_CONFIG_HEADER([config.h])

dnl Available languages (translations)
ALL_LINGUAS="cs da de el en_GB es fr ja nl no pl pt sk sv"
AC_SUBST(ALL_LINGUAS)

dnl Set up libtool scripts.
dnl Disable static library building to speed up the build
AC_LIBTOOL_DLOPEN
# We need static libraries for CUPS genppd program for now, for bug 865265
# --rlk 20031223
AC_ENABLE_STATIC(yes)
#AC_DISABLE_SHARED
AM_PROG_LIBTOOL
AC_SUBST(LIBTOOL_DEPS)
dnl define HAVE_GNU_LD if GNU ld is present
if test "$ac_cv_prog_gnu_ld" = "yes" ; then
  AC_DEFINE(HAVE_GNU_LD,, [Define if GNU ld is present.])
fi

dnl Set up maintainer mode
AM_MAINTAINER_MODE
if test x${USE_MAINTAINER_MODE} = xyes ; then
  MAINTAINER_MODE="true"
  BUILD_TEST="yes"
  AC_DEFINE(HAVE_MAINTAINER_MODE,, [Define if maintainer-mode is to be used.])
fi

dnl Check canonical system/host (a kludge: replace me with something better).
if test x${host_os} = xlinux-gnu ; then
  OSTYPE="linux"
elif test "`echo ${host_os} | sed 's/^\(linux\).*$/\1/'`" = "linux" ; then
  OSTYPE="linux"
elif test "`echo ${host_os} | sed 's/^\(irix\).*$/\1/'`" = "irix" ; then
  OSTYPE="irix"
else
  OSTYPE="${host_os}"
fi
AC_DEFINE_UNQUOTED(OSTYPE, ["${OSTYPE}"], [The operating system to build for])


dnl Command line options.
AC_PATH_PROG(CUPS_CONFIG, cups-config)
if test -z "${CUPS_CONFIG}" ; then
  BUILD_CUPS="no"
else
  BUILD_CUPS="yes"
fi
STP_ARG_WITH_DETAILED(
  [cups],
  [=PREFIX],
  [whether to build CUPS driver],
  [(default PREFIX=/usr)],
  [BUILD_CUPS],
  [(automatic)],
  [
    yes) [BUILD_CUPS]="yes" ; AC_MSG_RESULT([yes]) ; cups_prefix="/usr" ;;
    no) [BUILD_CUPS]="no" ; AC_MSG_RESULT([no]) ; cups_prefix="/usr" ;;
    *) if test -d ${withval} ; then
         [BUILD_CUPS]="yes"
	 AC_MSG_RESULT([yes (using specified prefix)])
         cups_prefix="${withval}"
       else
         AC_MSG_RESULT([unknown])
         AC_MSG_ERROR([${withval}: no such directory for --with-cups])
       fi ;;
  ],[
     cups_prefix="${prefix}"
  ])

BUILD_CUPS_PPDS="${BUILD_CUPS}"

STP_ARG_ENABLE([cups-ppds],
               [build CUPS PPD files],
               [BUILD_CUPS_PPDS],
               [(automatic)])

STP_ARG_ENABLE([translated-cups-ppds],
               [build translated CUPS PPD files],
               [BUILD_TRANSLATED_CUPS_PPDS],
               [no])

USE_LEVEL3_PS=no
if test -n "${CUPS_CONFIG}" -a "${BUILD_CUPS}" = "yes" ; then
  cups_version=`${CUPS_CONFIG} --version`
dnl Quote to protect sed regexes
  [cups_major=`echo $cups_version | sed -e 's/\([0-9][0-9]*\).\([0-9][0-9]*\).\([0-9][0-9]*\).*/\1/'`]
  [cups_minor=`echo $cups_version | sed -e 's/\([0-9][0-9]*\).\([0-9][0-9]*\).\([0-9][0-9]*\).*/\2/'`]
  [cups_point=`echo $cups_version | sed -e 's/\([0-9][0-9]*\).\([0-9][0-9]*\).\([0-9][0-9]*\).*/\3/'`]
  if test "$cups_major" -gt 1 -o "$cups_minor" -gt 1 -o "$cups_point" -gt 14 ; then
    USE_LEVEL3_PS=yes
  fi
fi

STP_ARG_ENABLE([cups-ppds-at-top-level],
	       [place CUPS PPD files at top level],
	       [CUPS_PPDS_AT_TOP_LEVEL],
	       [no])

STP_ARG_ENABLE([cups-level3-ppds],
               [generate PostScript level 3 PPD files],
               [USE_LEVEL3_PS],
               [(automatic)])

STATIC_GENPPD="$enable_static"
STP_ARG_ENABLE([static-genppd],
               [build a statically-linked cups-genppd and rastertogimpprint],
	       [STATIC_GENPPD],
	       [(automatic)])

AC_PATH_PROG(FOOMATIC_CONFIGURE, foomatic-configure)
if test -z "${FOOMATIC_CONFIGURE}" ; then
  BUILD_FOOMATIC="no"
else
  BUILD_FOOMATIC="yes"
fi
STP_ARG_WITH([foomatic],
             [build foomatic data files],
             [BUILD_FOOMATIC],
             [(automatic)])


AC_PATH_PROG(FOOMATIC_PPDFILE, foomatic-ppdfile)
if test -z "${FOOMATIC_PPDFILE}" ; then
  BUILD_FOOMATIC_3="no"
else
  BUILD_FOOMATIC_3="yes"
fi
STP_ARG_WITH([foomatic3],
             [make use of foomatic 3 extra features],
             [BUILD_FOOMATIC_3],
             [(automatic)])

AC_PATH_PROG(IJS_CONFIG, ijs-config)

if test -z "${IJS_CONFIG}"  ; then
  BUILD_GHOSTSCRIPT="no"
else
  BUILD_GHOSTSCRIPT="yes"
fi
STP_ARG_WITH([ghostscript],
             [build Ghostscript driver],
             [BUILD_GHOSTSCRIPT],
             [(automatic)])

STP_ARG_WITH_DETAILED([modules],
                      [],
                      [enable loadable modules],
	              [(options: ltdl, dlopen, static)],
	              [WITH_MODULES],
	              [(automatic)],[
    yes) [WITH_MODULES]="yes";  AC_MSG_RESULT([yes]) ;;
    no) [WITH_MODULES]="no";  AC_MSG_RESULT([no]) ;;
    ltdl) [WITH_MODULES]="ltdl";  AC_MSG_RESULT([ltdl]) ;;
    dlopen) [WITH_MODULES]="dlopen";  AC_MSG_RESULT([dlopen]) ;;
    static) [WITH_MODULES]="static";  AC_MSG_RESULT([static]) ;;
    *) AC_MSG_ERROR([${withval}: not a valid module loader]) ;;
                      ],[
    # if static libraries are enabled, modules must be static
    if test x$enable_static = xyes; then
      [WITH_MODULES]="static"
    else
      [WITH_MODULES]="yes"]
    fi)

# Abort if a broken configuration was required
if test x$enable_static = xyes; then
  if test $WITH_MODULES = no; then
    :
  elif test $WITH_MODULES = static; then
    :
  else
    AC_MSG_ERROR([--disable-static must be used with --with-modules])
  fi
fi 

STP_ARG_ENABLE([debug],
               [turn on debugging in build],
               [ENABLE_DEBUG],
               [no])

STP_ARG_ENABLE([profile],
               [turn on profiling in build],
               [ENABLE_PROFILE],
               [no])

STP_ARG_WITH_DETAILED([readline], ,
                      [use readline],
                      [(default tries -lncurses, -lcurses, -ltermcap)],
                      [USE_READLINE],
                      [yes],[
    yes) [USE_READLINE]="yes" ; AC_MSG_RESULT([yes]) ;;
    no) [USE_READLINE]="no" ; AC_MSG_RESULT([no]) ;;
    only) [USE_READLINE]="yes"
          [EXTRA_LIBREADLINE_DEPS]=" "
          AC_MSG_RESULT([yes (using only readline)]) ;;
    *) [USE_READLINE]="yes"
       [EXTRA_LIBREADLINE_DEPS]="${withval}"
       AC_MSG_RESULT([yes (using extra libraries ${withval})]) ;;
             ])
dnl if test -z "${[EXTRA_LIBREADLINE_DEPS]}" ; then
dnl   [EXTRA_LIBREADLINE_DEPS]=""
dnl fi

AC_PATH_PROGS([GIMPTOOL_CHECK], [gimptool-1.2 gimptool])
if test -z "${GIMPTOOL_CHECK}" ; then
  BUILD_GIMP="no"
else
  # Check we have version 1.2
  gimptool_check_version=`${GIMPTOOL_CHECK} --version`
dnl Quote to protect sed regexes
  [gimptool_check_major=`echo "$gimptool_check_version" | sed -e 's/\([0-9][0-9]*\).\([0-9][0-9]*\).\([0-9][0-9]*\).*/\1/'`]
  [gimptool_check_minor=`echo "$gimptool_check_version" | sed -e 's/\([0-9][0-9]*\).\([0-9][0-9]*\).\([0-9][0-9]*\).*/\2/'`]
  if test "$gimptool_check_major" -eq 1 -a "$gimptool_check_minor" -eq 2; then
    BUILD_GIMP="yes"
  else
    BUILD_GIMP="no"
  fi
fi

AM_PATH_GTK(1.2.0,
  BUILD_LIBGIMPPRINTUI="yes"
  GIMPPRINT_UI_LIBDEPS="${GIMPPRINT_UI_LIBDEPS} ${GTK_LIBS}",
  BUILD_LIBGIMPPRINTUI="no")

STP_ARG_WITH([gimp],
             [build GIMP 1.2 Print plugin],
             [BUILD_GIMP],
             [(automatic)])

STP_ARG_ENABLE([libgimpprintui],
               [build Gimp-Print GTK+-1.2 user interface library],
               [BUILD_LIBGIMPPRINTUI],
               [(automatic)])

AC_PATH_PROGS([GIMPTOOL2_CHECK], [gimptool-2.0 gimptool])
if test -z "${GIMPTOOL2_CHECK}" ; then
  BUILD_GIMP2="no"
else
  # Check we have version 2.0
  gimptool_check_version=`${GIMPTOOL2_CHECK} --version`
dnl Quote to protect sed regexes
  [gimptool_check_major=`echo "$gimptool_check_version" | sed -e 's/\([0-9][0-9]*\).\([0-9][0-9]*\).\([0-9][0-9]*\).*/\1/'`]
  [gimptool_check_minor=`echo "$gimptool_check_version" | sed -e 's/\([0-9][0-9]*\).\([0-9][0-9]*\).\([0-9][0-9]*\).*/\2/'`]
  if test "$gimptool_check_major" -eq 2 -a "$gimptool_check_minor" -eq 0; then
    BUILD_GIMP2="yes"
  else
    BUILD_GIMP2="no"
  fi
fi

PKG_CHECK_MODULES([GTK2], [gtk+-2.0],
                  [HAVE_GTK2=yes
                   BUILD_LIBGIMPPRINTUI2=yes
                   GIMPPRINTUI2_LIBDEPS="${GIMPPRINTUI2_LIBDEPS} ${GTK2_LIBS}"],
                  [HAVE_GTK2=no
	           BUILD_LIBGIMPPRINTUI2=no])
PKG_CHECK_MODULES([GIMP2], [gimpui-2.0], [HAVE_GTK2=yes], [HAVE_GTK2=no])

STP_ARG_WITH([gimp2],
             [build GIMP 2.0 Print plugin],
             [BUILD_GIMP2],
             [(automatic)])

STP_ARG_ENABLE([libgimpprintui2],
               [build Gimp-Print GTK+-2.0 user interface library],
               [BUILD_LIBGIMPPRINTUI2],
               [(automatic)])

STP_ARG_ENABLE([samples],
               [install sample images],
               [INSTALL_SAMPLES],
               [yes])

STP_ARG_ENABLE([user-guide],
               [install user guide],
               [INSTALL_USER_GUIDE],
               [yes])

STP_ARG_ENABLE([escputil],
               [build escputil],
               [BUILD_ESCPUTIL],
               [yes])

STP_ARG_ENABLE([test],
               [build test programs],
               [BUILD_TEST],
               [yes])

if test "$BUILD_TEST" = "yes" ; then
  BUILD_TESTPATTERN="yes"
else
  BUILD_TESTPATTERN="no"
fi

STP_ARG_ENABLE([testpattern],
               [build testpattern generator],
               [BUILD_TESTPATTERN],
               [(automatic)])

STP_ARG_ENABLE([user-install],
               [install GIMP plugin in home directory],
               [USER_INSTALL],
               [no])
if test x$USER_INSTALL = xyes ; then
  PLUG_IN_PATH="bin"
else
  PLUG_IN_PATH="admin-bin"
fi

dnl aclocal flags.
dnl AM_ACLOCAL_INCLUDE(m4 m4local)

dnl Checks for programs.
AC_ISC_POSIX
AC_PROG_AWK
AC_PROG_CC
AM_PROG_CC_STDC
AC_PROG_INSTALL
AC_PROG_LN_S
AM_PROG_LEX
AC_PROG_YACC
AC_PATH_PROG([FIND], [find])
AC_PATH_PROG([PERL], [perl])
AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])
AC_PATH_PROG([CONVERT], [convert])
if test x${USE_MAINTAINER_MODE} = xyes ; then
  if test x${CONVERT} = x ; then
    AC_MSG_WARN([PostScript user's guide cannot be regenerated!])
  fi
fi
AC_PATH_PROG(TEXI2HTML, texi2html)
if test x${USE_MAINTAINER_MODE} = xyes ; then
  if test x${TEXI2HTML} = x ; then
    AC_MSG_WARN([HTML documentation cannot be regenerated!])
  fi
fi
AC_PATH_PROG(DVIPS, dvips)
if test x${USE_MAINTAINER_MODE} = xyes ; then
  if test x${DVIPS} = x ; then
    AC_MSG_WARN([PostScript documentation cannot be regenerated!])
  fi
fi
AC_PATH_PROG(DB2PDF, db2pdf)
if test x${USE_MAINTAINER_MODE} = xyes ; then
  if test x${DB2PDF} = x ; then
    AC_MSG_WARN([PDF user's guide cannot be regenerated!])
  fi
fi
AC_PATH_PROGS(DVIPDF, dvipdf dvipdfm)
if test x${USE_MAINTAINER_MODE} = xyes ; then
  if test x${DVIPDF} = x ; then
    AC_MSG_WARN([PDF documentation cannot be regenerated!])
  fi
fi
AC_PATH_PROG(DB2PS, db2ps)
if test x${USE_MAINTAINER_MODE} = xyes ; then
  if test x${DB2PS} = x ; then
    AC_MSG_WARN([PostScript user's guide cannot be regenerated!])
  fi
fi
AC_PATH_PROG(DB2HTML, db2html)
if test x${USE_MAINTAINER_MODE} = xyes ; then
  if test x${DB2HTML} = x ; then
    AC_MSG_WARN([HTML user's guide cannot be regenerated!])
  fi
fi
AC_PATH_PROG(DOXYGEN, doxygen)
if test x${USE_MAINTAINER_MODE} = xyes ; then
  if test x${DOXYGEN} = x ; then
    AC_MSG_WARN([HTML user's guide cannot be regenerated!])
  fi
fi

dnl CUPS checks
STP_CUPS_PROGS

dnl foomatic checks
if test x${BUILD_FOOMATIC} = xyes ; then
  PATH_SBIN=$PATH:/usr/sbin:/usr/local/sbin
  AC_PATH_PROG(FOOMATIC_KITLOAD, foomatic-kitload, , [$PATH_SBIN])
  if test -z "$FOOMATIC_KITLOAD" ; then
    AC_MSG_WARN(Cannot find foomatic-kitload; attempt to install foomatic will fail)
  fi
fi

dnl ijs checks
if test x${BUILD_GHOSTSCRIPT} = xyes ; then
  if test -z "$IJS_CONFIG" ; then
    AC_MSG_ERROR(Cannot find ijs-config; please ensure Ghostscript 6.53 or above is installed);
  fi
  IJS_CFLAGS=`$IJS_CONFIG --cflags`
  IJS_LIBS=`$IJS_CONFIG --libs`
fi

dnl GNU gettext checks.
AM_GNU_GETTEXT_VERSION([0.11.5])
AM_GNU_GETTEXT([external])

GIMPPRINT_RELEASE_VERSION=${GIMPPRINT_MAJOR_VERSION}.${GIMPPRINT_MINOR_VERSION}
AH_TEMPLATE(GIMPPRINT_RELEASE_VERSION,, [Gimp-Print release version])
AC_DEFINE_UNQUOTED(GIMPPRINT_RELEASE_VERSION, "${GIMPPRINT_RELEASE_VERSION}")

dnl Set PACKAGE_LOCALE_DIR in config.h
AH_TEMPLATE(PACKAGE_LOCALE_DIR,, [Package locale directory])
if test "x${prefix}" = "xNONE"; then
  AC_DEFINE_UNQUOTED(PACKAGE_LOCALE_DIR, ["${ac_default_prefix}/share/locale"])
else
  AC_DEFINE_UNQUOTED(PACKAGE_LOCALE_DIR, ["${prefix}/share/locale"])
fi

dnl Set PACKAGE_DATA_DIR in config.h.
AH_TEMPLATE(PACKAGE_DATA_DIR,, [Package data directory])
if test "x${datadir}" = 'x${prefix}/share'; then
  if test "x${prefix}" = "xNONE"; then
    PACKAGE_DATA_DIR="${ac_default_prefix}/share/${PACKAGE}"
  else
    PACKAGE_DATA_DIR="${prefix}/share/${PACKAGE}"
  fi
else
  PACKAGE_DATA_DIR="${datadir}/${PACKAGE}"
fi

AC_DEFINE_UNQUOTED(PACKAGE_DATA_DIR, "${PACKAGE_DATA_DIR}")

dnl Set PACKAGE_LIB_DIR in config.h.
AH_TEMPLATE(PACKAGE_LIB_DIR,, [Package lib directory])
if test "x${libdir}" = 'x${exec_prefix}/lib'; then
  if test "x${exec_prefix}" = "xNONE"; then
    if test "x${prefix}" = "xNONE"; then
      PACKAGE_LIB_DIR="${ac_default_prefix}/lib/${PACKAGE}"
    else
      PACKAGE_LIB_DIR="${prefix}/lib/${PACKAGE}"
    fi
  else
    PACKAGE_LIB_DIR="${exec_prefix}/lib/${PACKAGE}"
  fi
else
  PACKAGE_LIB_DIR="${libdir}/${PACKAGE}"
fi

AC_DEFINE_UNQUOTED(PACKAGE_LIB_DIR, "${PACKAGE_LIB_DIR}")

AH_TEMPLATE(PKGXMLDATADIR,, [Package XML data directory])
PKGXMLDATADIR="${PACKAGE_DATA_DIR}/${PACKAGE_VERSION}/xml"
AC_DEFINE_UNQUOTED(PKGXMLDATADIR, ["$PKGXMLDATADIR"], )

AH_TEMPLATE(PKGMODULEDIR,, [Package module directory])
PKGMODULEDIR="${PACKAGE_LIB_DIR}/${PACKAGE_VERSION}/modules"
AC_DEFINE_UNQUOTED(PKGMODULEDIR, ["$PKGMODULEDIR"])

dnl Compiler flags
if test "$CC" = "gcc" ; then
  STP_ADD_COMPILER_ARGS([-Wall -Wcast-align -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs -Wwrite-strings -Werror-implicit-function-declaration -Winline -finline-limit=1048576], GNUCFLAGS)
  if test x${USE_MAINTAINER_MODE} = xyes ; then
    STP_ADD_COMPILER_ARGS([-pedantic -Waggregate-return -Wcast-qual -Wshadow -Wredundant-decls], GNUCFLAGS)
  fi
else
  if test x$ENABLE_DEBUG = xyes ; then
    STP_ADD_COMPILER_ARG([-g])
  else
    STP_ADD_FIRST_COMPILER_ARG([-O6 -O3 -O2 -O1 -O])
  fi
fi
if test x$ENABLE_PROFILE = xyes ; then
  STP_ADD_COMPILER_ARG([-pg])
fi
AC_SUBST(GNUCFLAGS)

dnl Checks for libraries.
dnl When checking readline, check using extra libraries first.
dnl We want to protect against the link somehow succeeding, but only
dnl failing at runtime, as seems to happen on some BSD systems.
AH_TEMPLATE(HAVE_LIBREADLINE, [Is libreadline present?])
if test "$USE_READLINE" = yes ; then
  if test "$EXTRA_LIBREADLINE_DEPS" = "" ; then
    unset ac_cv_lib_readline_readline
    AC_CHECK_LIB(readline, readline, HAVE_LIBREADLINE=true,
		 HAVE_LIBREADLINE=false, -lncurses)
    if test x${HAVE_LIBREADLINE} = xtrue ; then
      echo "  Using -lreadline -lncurses"
      EXTRA_LIBREADLINE_DEPS=-lncurses
      AC_DEFINE(HAVE_LIBREADLINE, [1])
    else
      unset ac_cv_lib_readline_readline
      AC_CHECK_LIB(readline, readline, HAVE_LIBREADLINE=true,
		   HAVE_LIBREADLINE=false, -lcurses)
      if test x${HAVE_LIBREADLINE} = xtrue ; then
	echo "  Using -lreadline -lcurses"
	EXTRA_LIBREADLINE_DEPS=-lcurses
	AC_DEFINE(HAVE_LIBREADLINE, [1])
      else
	unset ac_cv_lib_readline_readline
	AC_CHECK_LIB(readline, readline, HAVE_LIBREADLINE=true,
		     HAVE_LIBREADLINE=false, -ltermcap)
	if test x${HAVE_LIBREADLINE} = xtrue ; then
	  echo "  Using -lreadline -ltermcap"
	  EXTRA_LIBREADLINE_DEPS=-ltermcap
	  AC_DEFINE(HAVE_LIBREADLINE, [1])
	else
	  unset ac_cv_lib_readline_readline
	  AC_CHECK_LIB(readline, readline, HAVE_LIBREADLINE=true,
		       HAVE_LIBREADLINE=false)
	  if test x${HAVE_LIBREADLINE} = xtrue ; then
	    AC_DEFINE(HAVE_LIBREADLINE, [1])
	  else
	    AC_DEFINE(HAVE_LIBREADLINE, [0])
	  fi
	fi
      fi
    fi
  else
    unset ac_cv_lib_readline_readline
    AC_CHECK_LIB(readline, readline, HAVE_LIBREADLINE=true,
		 HAVE_LIBREADLINE=false, ${EXTRA_LIBREADLINE_DEPS})
    if test x${HAVE_LIBREADLINE} = xtrue ; then
      echo "  Using -lreadline ${EXTRA_LIBREADLINE_DEPS}"
      AC_DEFINE(HAVE_LIBREADLINE, [1])
    else
      AC_DEFINE(HAVE_LIBREADLINE, [0])
    fi
  fi
else
  HAVE_LIBREADLINE=false
  AC_DEFINE(HAVE_LIBREADLINE, [0])
fi

dnl libltdl, used for dynamic module loading
AC_CHECK_LIB(ltdl, lt_dlopen, [LTDL_LIBS="-lltdl"])

AC_CHECK_LIB(dl, dlopen, [DLOPEN_LIBS="-ldl"])

AC_CHECK_LIB(m,pow,
             GIMPPRINT_LIBDEPS="${GIMPPRINT_LIBDEPS} -lm"
             gimpprint_libdeps="${gimpprint_libdeps} -lm")

STP_CUPS_LIBS

if test x$BUILD_LEXMARKUTIL = xyes -o x$BUILD_GIMPPRINTUI = xyes ; then
AM_PATH_GTK(1.2.0, ,
            AC_MSG_ERROR(Cannot find GTK: Is gtk-config in path?))
fi

STP_GIMP_LIBS
STP_GIMP_PLUG_IN_DIR
STP_GIMP2_LIBS
dnl STP_GIMP2_PLUG_IN_DIR is obsolete

dnl Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS(getopt.h)
AC_CHECK_HEADER(readline/readline.h,
  HAVE_READLINE_READLINE_H=true,
  HAVE_READLINE_READLINE_H=false)
if test x${HAVE_READLINE_READLINE_H} = xtrue ; then
  AC_DEFINE(HAVE_READLINE_READLINE_H,, [Define if libreadline header is present.])
  if test x${HAVE_LIBREADLINE} = xtrue ; then
    LIBREADLINE_DEPS="-lreadline $EXTRA_LIBREADLINE_DEPS"
  fi
fi
AC_CHECK_HEADERS(dlfcn.h, [HAVE_DLFCN_H=true])
AC_CHECK_HEADERS(fcntl.h)
AC_CHECK_HEADERS(limits.h)
AC_CHECK_HEADERS(locale.h)
AC_CHECK_HEADERS(ltdl.h, [HAVE_LTDL_H=true])
AC_CHECK_HEADERS(stdarg.h stdlib.h string.h)
AC_CHECK_HEADERS(sys/time.h sys/types.h)
AC_CHECK_HEADERS(time.h)
AC_CHECK_HEADERS(unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_TYPE_SIGNAL

dnl Checks for library functions.
AC_CHECK_FUNCS([nanosleep poll usleep])
AC_CHECK_FUNC([getopt_long])


dnl Define what has to be built
AM_CONDITIONAL(BUILD_FOOMATIC, test x${BUILD_FOOMATIC} = xyes)

AM_CONDITIONAL(BUILD_FOOMATIC_3, test x${BUILD_FOOMATIC_3} = xyes)

AM_CONDITIONAL(BUILD_GHOSTSCRIPT, test x${BUILD_GHOSTSCRIPT} = xyes)

AM_CONDITIONAL(BUILD_CUPS, test x${BUILD_CUPS} = xyes)

AM_CONDITIONAL(BUILD_CUPS_PPDS, test x${BUILD_CUPS_PPDS} = xyes)

AM_CONDITIONAL(CUPS_PPDS_AT_TOP_LEVEL, test x${CUPS_PPDS_AT_TOP_LEVEL} = xyes)

AM_CONDITIONAL(BUILD_TRANSLATED_CUPS_PPDS, test x${BUILD_TRANSLATED_CUPS_PPDS} = xyes)

if test x${USE_LEVEL3_PS} = xno ; then
  CUPS_PPD_PS_LEVEL=2
else
  CUPS_PPD_PS_LEVEL=3
fi

AM_CONDITIONAL(USE_LEVEL3_PS, test x${USE_LEVEL3_PS} = xyes)

AM_CONDITIONAL(BUILD_ESCPUTIL, test x${BUILD_ESCPUTIL} = xyes)

AM_CONDITIONAL(BUILD_TEST, test x${BUILD_TEST} = xyes)

AM_CONDITIONAL(BUILD_TESTPATTERN, test x${BUILD_TESTPATTERN} = xyes)

AM_CONDITIONAL(BUILD_LIBGIMPPRINTUI, test x${BUILD_LIBGIMPPRINTUI} = xyes)

AM_CONDITIONAL(BUILD_LIBGIMPPRINTUI2, test x${BUILD_LIBGIMPPRINTUI2} = xyes)

AM_CONDITIONAL(BUILD_GIMP, test x${BUILD_GIMP} = xyes)

AM_CONDITIONAL(BUILD_GIMP2, test x${BUILD_GIMP2} = xyes)

AM_CONDITIONAL(INSTALL_SAMPLES, test x${INSTALL_SAMPLES} = xyes)

AM_CONDITIONAL(INSTALL_USER_GUIDE, test x${INSTALL_USER_GUIDE} = xyes)

STP_CUPS_PATH

AM_CONDITIONAL(USE_NLS, test x${USE_NLS} = xyes)

# loadable modules
# portable sh has no AND test...
if test -n "$HAVE_LTDL_H"; then
  if test -n "$LTDL_LIBS"; then
    LTDL_POSSIBLE="true"
  fi
fi
if test -n "$HAVE_DLFCN_H"; then
  if test -n "$DLOPEN_LIBS"; then
    DLOPEN_POSSIBLE="true"
  fi
fi
# define what the user chose to build, if possible.
# Prefer dlopen over ltdl.
if test x$WITH_MODULES = xyes; then
  if test x$DLOPEN_POSSIBLE = xtrue; then
    USE_DLOPEN="true"
  elif test x$LDTL_POSSIBLE = xtrue; then
    USE_LTDL="true"
  fi
elif test x$WITH_MODULES = xltdl; then
  if test x$LTDL_POSSIBLE = xtrue; then
    USE_LTDL="true"
  fi
elif test x$WITH_MODULES = xdlopen; then
  if test x$DLOPEN_POSSIBLE = xtrue; then
    USE_DLOPEN="true"
  fi
fi
# define what module system is to be used
AC_MSG_CHECKING([which module system will be used])
if test x$USE_LTDL = xtrue; then
  MODULE="true"
  AC_DEFINE(MODULE, "1", [Build a modular libgimpprint])
  AC_DEFINE(USE_LTDL, "1", [Use GNU libltdl as module loader])
  GIMPPRINT_LIBDEPS="${GIMPPRINT_LIBDEPS} ${LTDL_LIBS}"
  gimpprint_libdeps="${gimpprint_libdeps} ${LTDL_LIBS}"
  AC_MSG_RESULT([ltdl])
elif test x$USE_DLOPEN = xtrue; then
  MODULE="true"
  AC_DEFINE(MODULE, "1", [Build a modular libgimpprint])
  AC_DEFINE(USE_DLOPEN, "1", [Use libdl/dlopen as module loader])
  GIMPPRINT_LIBDEPS="${GIMPPRINT_LIBDEPS} ${DLOPEN_LIBS}"
  gimpprint_libdeps="${gimpprint_libdeps} ${DLOPEN_LIBS}"
  AC_MSG_RESULT([dlopen])
else
  MODULE="false"
  AC_MSG_RESULT([static])
fi
AM_CONDITIONAL(BUILD_MODULES, test x$MODULE = xtrue)

AM_CONDITIONAL(BUILD_GENPPD_STATIC, test x"$STATIC_GENPPD" = xyes)

if test "$enable_static" = no -a "$BUILD_CUPS" = yes ; then
  AC_MSG_WARN([--enable-static is disabled, but CUPS is being built.])
  AC_MSG_WARN([This may result in a *SILENTLY* incorrect build.])
  AC_MSG_WARN([Please read the release notes carefully!])
fi

dnl Define LTLIBOBJS
AC_CONFIG_COMMANDS_PRE(
[LTLIBOBJS=`echo "$LIB@&t@OBJS" | sed 's/\.o/.lo/g'`
AC_SUBST(LTLIBOBJS)
LTALLOCA=`echo "$ALLOCA" | sed 's/\.o/.lo/g'`
AC_SUBST(LTALLOCA)])

dnl Definitions
AC_DEFINE_UNQUOTED(CUPS_DATADIR, "${cups_conf_datadir}", [CUPS data directory.])

AH_TEMPLATE(CUPS_MODELDIR,, [CUPS PPD file directory])
if test "${CUPS_PPDS_AT_TOP_LEVEL}" = yes ; then
  AC_DEFINE_UNQUOTED(CUPS_MODELDIR, "${cups_conf_datadir}/model/")
else
  AC_DEFINE_UNQUOTED(CUPS_MODELDIR, "${cups_conf_datadir}/model/${PACKAGE}/${GIMPPRINT_RELEASE_VERSION}/")
fi

AH_TEMPLATE(CUPS_PPD_NICKNAME_STRING,, [CUPS PPD Nickname string])
AC_DEFINE_UNQUOTED(CUPS_PPD_NICKNAME_STRING, [" - Gimp-Print v"])


AC_DEFINE_UNQUOTED(CUPS_PPD_PS_LEVEL, ${CUPS_PPD_PS_LEVEL}, [CUPS PPD PostScript level])
STP_RELEASE_DATE

dnl Some versions of make get very grouchy about $(RM)
RM='rm'

dnl Substitutions
AC_SUBST(cups_prefix)
AC_SUBST(cups_exec_prefix)
AC_SUBST(cups_bindir)
AC_SUBST(cups_sbindir)
AC_SUBST(cups_conf_datadir)
AC_SUBST(cups_conf_serverbin)
AC_SUBST(cups_conf_serverroot)
AC_SUBST(CUPS_CFLAGS)
AC_SUBST(CUPS_LIBS)
AC_SUBST(CUPS_PPD_PS_LEVEL)
AC_SUBST(FOOMATIC_KITLOAD)
AC_SUBST(GENPPD_LIBS)
AC_SUBST(IJS_CFLAGS)
AC_SUBST(IJS_LIBS)
AC_SUBST(RM)
AC_SUBST(GIMP_CFLAGS)
AC_SUBST(GIMP_LIBS)
AC_SUBST(GIMP2_CFLAGS)
AC_SUBST(GIMP2_LIBS)
AC_SUBST(gimp_plug_indir)
AC_SUBST(gimp2_plug_indir)
AC_SUBST(GIMPPRINT_CFLAGS)
AC_SUBST(GIMPPRINT_LIBS)
AC_SUBST(GIMPPRINT_LIBDEPS)
AC_SUBST(GIMPPRINT_VERSION)
AC_SUBST(GIMPPRINT_UI_CFLAGS)
AC_SUBST(GIMPPRINT_UI_LIBS)
AC_SUBST(GIMPPRINT_UI_LIBDEPS)
AC_SUBST(GIMPPRINT_UI_VERSION)
AC_SUBST(GIMPPRINTUI2_CFLAGS)
AC_SUBST(GIMPPRINTUI2_LIBS)
AC_SUBST(GIMPPRINTUI2_LIBDEPS)
AC_SUBST(GIMPPRINTUI2_VERSION)
AC_SUBST(gimpprint_cflags)
gimpprint_libs="${GIMPPRINT_LIBS} ${gimpprint_libdeps}"
AC_SUBST(gimpprint_libs)
AC_SUBST(gimpprint_ui_cflags)
gimpprint_ui_libs="${GIMPPRINT_UI_LIBS} ${gimpprint_ui_libdeps}"
AC_SUBST(gimpprint_ui_libs)
AC_SUBST(gimpprintui2_cflags)
gimpprintui2_libs="${GIMPPRINTUI2_LIBS} ${gimpprintui2_libdeps}"
AC_SUBST(gimpprintui2_libs)
AC_SUBST(LIBREADLINE_DEPS)
AC_SUBST(MAINTAINER_CFLAGS)
AC_SUBST(PLUG_IN_PATH)

dnl Output files

AC_CONFIG_FILES([Makefile])
AC_CONFIG_FILES([debian/Makefile])
AC_CONFIG_FILES([doc/Makefile])
AC_CONFIG_FILES([doc/gimpprint.dox])
AC_CONFIG_FILES([doc/developer/Makefile])
AC_CONFIG_FILES([doc/users_guide/Makefile])
AC_CONFIG_FILES([include/Makefile])
AC_CONFIG_FILES([include/gimp-print/Makefile])
AC_CONFIG_FILES([include/gimp-print-ui/Makefile])
AC_CONFIG_FILES([include/gimp-print-ui2/Makefile])
AC_CONFIG_FILES([include/gimp-print/gimp-print-version.h])
AC_CONFIG_FILES([man/Makefile])
AC_CONFIG_FILES([man/cups-calibrate.8])
AC_CONFIG_FILES([man/cups-genppd.8])
AC_CONFIG_FILES([man/cups-genppdconfig.8])
AC_CONFIG_FILES([man/cups-genppdupdate.8])
AC_CONFIG_FILES([man/escputil.1])
AC_CONFIG_FILES([man/gimpprint.3])
AC_CONFIG_FILES([man/gimpprint-color.7])
AC_CONFIG_FILES([man/gimpprint-dithers.7])
AC_CONFIG_FILES([man/gimpprint-imagetypes.7])
AC_CONFIG_FILES([man/gimpprint-inktypes.7])
AC_CONFIG_FILES([man/gimpprint-mediasizes.7])
AC_CONFIG_FILES([man/gimpprint-mediasources.7])
AC_CONFIG_FILES([man/gimpprint-mediatypes.7])
AC_CONFIG_FILES([man/gimpprint-models.7])
AC_CONFIG_FILES([man/gimpprint-resolutions.7])
AC_CONFIG_FILES([man/ijsgimpprint.1])
AC_CONFIG_FILES([po/Makefile.in])
AC_CONFIG_FILES([samples/Makefile])
AC_CONFIG_FILES([src/Makefile])
AC_CONFIG_FILES([src/cups/Makefile])
AC_CONFIG_FILES([src/cups/cups-genppdconfig],
                [chmod +x src/cups/cups-genppdconfig;
		 mv src/cups/cups-genppdconfig src/cups/cups-genppdconfig.${GIMPPRINT_RELEASE_VERSION}])
AC_CONFIG_FILES([src/cups/cups-genppdupdate],
                [chmod +x src/cups/cups-genppdupdate;
		 mv src/cups/cups-genppdupdate src/cups/cups-genppdupdate.${GIMPPRINT_RELEASE_VERSION}])
AC_CONFIG_FILES([src/escputil/Makefile])
AC_CONFIG_FILES([src/foomatic/Makefile])
AC_CONFIG_FILES([src/foomatic/foomatic-generator],
                [chmod +x src/foomatic/foomatic-generator])
AC_CONFIG_FILES([src/foomatic/mk_printerlist],
                [chmod +x src/foomatic/mk_printerlist])
AC_CONFIG_FILES([src/ghost/Makefile])
AC_CONFIG_FILES([src/testpattern/Makefile])
AC_CONFIG_FILES([src/gimp/Makefile])
AC_CONFIG_FILES([src/gimp2/Makefile])
AC_CONFIG_FILES([src/main/Makefile])
AC_CONFIG_FILES([src/main/gimpprint.pc])
AC_CONFIG_FILES([src/libgimpprintui/Makefile])
AC_CONFIG_FILES([src/libgimpprintui/gimpprintui.pc])
AC_CONFIG_FILES([src/libgimpprintui2/Makefile])
AC_CONFIG_FILES([src/libgimpprintui2/gimpprintui2.pc])
AC_CONFIG_FILES([cups-gimpprint.list])
AC_CONFIG_FILES([test/Makefile])
AC_CONFIG_FILES([scripts/Makefile])
AC_CONFIG_COMMANDS([default], [
echo "Finished configuring."
echo "Type 'make' to build the package"
echo "then 'make install' to install it."
],
[GIMPPRINT_RELEASE_VERSION=${GIMPPRINT_RELEASE_VERSION}]
)

AC_OUTPUT
