dnl Copyright (C) 1996 Noah S. Friedman <friedman@prep.ai.mit.edu>

dnl $Id$

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
dnl along with this program; if not, you can either send email to this
dnl program's maintainer or write to: The Free Software Foundation,
dnl Inc.; 59 Temple Place, Suite 330; Boston, MA 02111-1307, USA.

dnl ############################################################

define([AC_ARG_WITH_RESOLV],
[AC_ARG_WITH([resolv],
   [  --with-resolv           Use -lresolv for host name lookups.
  --without-resolv        Do not use -lresolv for host name lookups.],
   [sed_downcase='y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/'
    val=`echo "$withval" | sed -e "$sed_downcase"`
    case "$val" in
      "" )               ac_use_resolv=yes ;;
      yes | no | maybe ) ac_use_resolv=$val ;;
      * ) AC_MSG_ERROR([$withval: invalid argument to --with-resolv]) ;;
    esac
   ],[# Default is not to link with resolv.
      ac_use_resolv=no
   ])dnl
])

dnl ############################################################

define([AC_USE_RESOLV],[
# Check whether resolv support is desired.
# If --with-resolv is specified to configure, support for resolv must
#   be available or configure will exit with an error.
# If --without-resolv is specified, no attempt is made to look for
#   resolv headers or libraries.
# By default, configure will use resolv if all the necessary support is
#   available, otherwise it won't.  No fatal errors should occur in either
#   circumstance.

# Default if not otherwise found
ac_resolv_support=no

if test $ac_use_resolv != no ; then
  AC_CHECK_LIB(resolv, gethostbyname)
  if eval test '$'ac_cv_lib_resolv_gethostbyname = yes; then
    ac_resolv_support=yes
  elif test $ac_use_resolv = yes; then
    AC_MSG_ERROR([Cannot link with -lresolv.  Is it missing?])
  fi
fi])

dnl ############################################################

define([AC_ARG_ENABLE_XAUTH],
[AC_ARG_ENABLE([xauth],
  [  --enable-xauth          Support XAUTH authentication support.
  --disable-xauth         Do not support XAUTH authentication support.],
  [sed_downcase='y/ABCDEFGHIJKLMNOPQRSTUVWXYZ/abcdefghijklmnopqrstuvwxyz/'
   val=`echo "$enableval" | sed -e "$sed_downcase"`
   case "$val" in
     yes | no | try ) require_xauth=$val ;;
     * ) AC_MSG_ERROR([$enableval: invalid argument to --enable-xauth]) ;;
    esac],[require_xauth=try])])

dnl ############################################################

define([AC_HAVE_XAUTH],
[AC_PATH_X
AC_SUBST(LIBXAUTH)
if test "$require_xauth" != "no" ; then
  if test "$have_x" = yes; then
    o_CPPFLAGS="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -I$x_includes -L$x_libraries"
  fi

  AC_CHECK_HEADER(X11/Xauth.h,[use_xauth=yes],[use_xauth=no])
  AC_CHECK_LIB(Xau, XauGetAuthByAddr, [use_xauth=yes], [use_xauth=no])

  if test "$have_x" = yes ; then
    CPPFLAGS="$o_CPPFLAGS"
  fi

  if test "$use_xauth" = "yes" ; then
    AC_DEFINE(HAVE_XAUTH)
    LIBXAUTH="-lXau"
    if test "$have_x" = yes ; then
      LIBXAUTH="-L$x_libraries $LIBXAUTH"
      CPPFLAGS="$CPPFLAGS -I$x_includes"
    fi
  elif test "$require_xauth" = "yes" ; then
    AC_MSG_ERROR([Cannot find headers or libraries for XAUTH support.])
  fi
fi])

dnl aclocal.m4 ends here
