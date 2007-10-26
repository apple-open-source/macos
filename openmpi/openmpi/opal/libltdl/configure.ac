## Process this file with autoconf to create configure. -*- autoconf -*-
# Copyright (C) 2004, 2005 Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301  USA

# This configure.ac is not used at all by the libtool bootstrap, but
# is copied to the ltdl subdirectory if you libtoolize --ltdl your own
# project.  Adding LT_WITH_LTDL to your project configure.ac will then
# configure this directory if your user doesn't want to use the installed
# libltdl.

AC_PREREQ(2.59)dnl We use AS_HELP_STRING


## ------------------------ ##
## Autoconf initialisation. ##
## ------------------------ ##
AC_INIT([libltdl], [2.1a], [bug-libtool@gnu.org])
AC_CONFIG_HEADERS([config.h:config-h.in])
AC_CONFIG_SRCDIR([ltdl.c])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
LT_CONFIG_LTDL_DIR([.]) # I am me!


## ---------------------------------------- ##
## Display a configure time version banner. ##
## ---------------------------------------- ##

# This is a sanity check so we can see which version is used in bug reports.
# It is assumed that we only want to see the date extension for cvs libtool
# versions (i.e. "odd" letters) and not actual alpha releases.
TIMESTAMP=
case $lt_alpha in
  [[acegikmoqsuwy]])
    TIMESTAMP=`${CONFIG_SHELL} ${ac_aux_dir}/mkstamp < ${srcdir}/ChangeLog`
    AS_BOX([Configuring AC_PACKAGE_TARNAME (Build:$TIMESTAMP) AC_PACKAGE_VERSION])
    echo
    ;;
esac
AC_SUBST([TIMESTAMP])


## ------------------------ ##
## Automake Initialisation. ##
## ------------------------ ##

AM_INIT_AUTOMAKE([gnits])

## ------------------------------- ##
## Libtool specific configuration. ##
## ------------------------------- ##
pkgdatadir='${datadir}'"/${PACKAGE}"


## ----------------------- ##
## Libtool initialisation. ##
## ----------------------- ##
LT_INIT([dlopen win32-dll])
LTDL_CONVENIENCE
LTDL_INIT


## -------- ##
## Outputs. ##
## -------- ##
AC_CONFIG_FILES([Makefile])
AC_OUTPUT
