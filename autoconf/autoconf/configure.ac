#                                                       -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# Copyright (C) 1992, 1993, 1994, 1995, 1999, 2000, 2001, 2002
# Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.

# We need AC_CONFIG_TESTDIR.
AC_PREREQ([2.55])

AC_INIT([GNU Autoconf], [2.57], [bug-autoconf@gnu.org])
AC_SUBST([PACKAGE_NAME])dnl
AC_CONFIG_SRCDIR([ChangeLog])

AC_CONFIG_AUX_DIR([config])
AC_CONFIG_FILES([config/Makefile])

AM_INIT_AUTOMAKE([check-news 1.7.1 dist-bzip2 readme-alpha])

# Initialize the test suite and build position independent wrappers.
AC_CONFIG_TESTDIR([tests])
AC_CONFIG_FILES([tests/Makefile tests/atlocal])
AC_CONFIG_FILES([tests/autoconf:tests/wrapper.in],
                [chmod +x tests/autoconf])
AC_CONFIG_FILES([tests/autoheader:tests/wrapper.in],
                [chmod +x tests/autoheader])
AC_CONFIG_FILES([tests/autom4te:tests/wrapper.in],
                [chmod +x tests/autom4te])
AC_CONFIG_FILES([tests/autoreconf:tests/wrapper.in],
                [chmod +x tests/autoreconf])
AC_CONFIG_FILES([tests/autoscan:tests/wrapper.in],
                [chmod +x tests/autoscan])
AC_CONFIG_FILES([tests/autoupdate:tests/wrapper.in],
                [chmod +x tests/autoupdate])
AC_CONFIG_FILES([tests/ifnames:tests/wrapper.in],
                [chmod +x tests/ifnames])
AC_PATH_PROG(EXPR, expr)


# We use a path for GNU m4 so even if users have another m4 first in
# their path, the installer can configure with a path that has GNU m4
# first and get that path embedded in the installed autoconf and
# autoheader scripts.
AC_PROG_GNU_M4
if test x"$ac_cv_prog_gnu_m4" != xyes; then
  AC_MSG_ERROR([GNU M4 1.4 is required])
fi

# Man pages.
AC_CONFIG_FILES(man/Makefile)
AM_MISSING_PROG(HELP2MAN, help2man)

# We use a path for perl so the #! line in autoscan will work.
AC_PATH_PROG([PERL], perl, no)
AC_SUBST([PERL])dnl
if test "$PERL" = no; then
  AC_MSG_ERROR([perl is not found])
fi
$PERL -e 'require 5.005_03;' || {
   AC_MSG_ERROR([Perl 5.005_03 or better is required])
}

# Emacs modes.
AC_CONFIG_FILES([lib/emacs/Makefile])
test x"$EMACS" = xt && EMACS=
AC_CHECK_PROGS(EMACS, emacs xemacs, no)
AM_PATH_LISPDIR

AC_CONFIG_FILES([Makefile doc/Makefile
                 lib/Makefile lib/Autom4te/Makefile lib/autoscan/Makefile
                 lib/m4sugar/Makefile
                 lib/autoconf/Makefile lib/autotest/Makefile
                 bin/Makefile])

AC_OUTPUT
# Report the state of this version of Autoconf if this is a beta.
m4_bmatch(m4_defn([AC_PACKAGE_VERSION]),  [[a-z]],
[    cat <<EOF

You are about to use an experimental version of Autoconf.  Be sure to
read the relevant mailing lists, most importantly <autoconf@gnu.org>.

Below you will find information on the status of this version of Autoconf.


EOF
    sed -n '/^\* Status/,$p' $srcdir/BUGS
])
