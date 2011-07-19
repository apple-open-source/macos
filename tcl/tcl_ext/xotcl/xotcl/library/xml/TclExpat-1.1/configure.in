#!/bin/bash -norc
dnl	This file is an input file used by the GNU "autoconf" program to
dnl	generate the file "configure", which is run during Tcl installation
dnl	to configure the system for the local environment.
#
# RCS: @(#) $Id: configure.in,v 1.20 2007/10/12 19:53:32 neumann Exp $

#-----------------------------------------------------------------------
# Sample configure.in for Tcl Extensions.  The only places you should
# need to modify this file are marked by the string __CHANGE__
#-----------------------------------------------------------------------

configdir=$(srcdir)/../../../config

#-----------------------------------------------------------------------
# __CHANGE__
# Set your package name and version numbers here.
#
# This initializes the environment with PACKAGE_NAME and PACKAGE_VERSION
# set as provided.  These will also be added as -D defs in your Makefile
# so you can encode the package version directly into the source files.
#-----------------------------------------------------------------------

AC_INIT([xotclexpat], [0.9])

#--------------------------------------------------------------------
# Call TEA_INIT as the first TEA_ macro to set up initial vars.
# This will define a ${TEA_PLATFORM} variable == "unix" or "windows"
# as well as PKG_LIB_FILE and PKG_STUB_LIB_FILE.
#--------------------------------------------------------------------

TEA_INIT([3.9])

AC_CONFIG_AUX_DIR(../../../config)

#--------------------------------------------------------------------
# specify some extra flags
#--------------------------------------------------------------------

AC_ARG_WITH(xotcl,
        [  --with-xotcl=DIR_CONTAINING_XOTCLCONFIG_SH
            absolute path to xotclConfig.sh, 
           --without-xotcl disables, but this is pointless],
        [with_xotcl=$withval], [AC_MSG_ERROR([--with-xotcl is required])])
AC_ARG_WITH(expat,
        [  --with-expat=sys assumes a system-wide expat installation, 
           --with-expat=<INC_DIR,LIB_DIR> point to a custom expat installation,
           --without-expat falls back to the bundled expat installation],
        [with_expat=$withval],[with_expat=bundle])

case $with_expat in
    bundle) ;;
    sys)
        AC_MSG_RESULT([search for expat in ${prefix} /usr/local /usr])
        for f in $prefix /usr/local /usr; do
            if test -f "$f/include/expat.h" ; then
		expat_dir=$f
                EXPAT_INC_SPEC="-I`(cd $inc_dir; pwd)`"
                break
            fi
        done
        ;;
    *)
	inc_dir="`echo $with_expat |cut -f1 -d,`"
        lib_dir="`echo $with_expat |cut -f2 -d, -s`"
        if test -f "$inc_dir/expat.h"; then
            EXPAT_INC_SPEC="-I`(cd $inc_dir; pwd)`"
        else
            AC_MSG_ERROR([${inc_dir} directory does not contain expat.h])
        fi
	expat_dir="`(cd $inc_dir/..; pwd)`"
	if test -z "${lib_dir}"; then
            EXPAT_LIB_SPEC=""
	else
            EXPAT_LIB_SPEC="-L`(cd $lib_dir; pwd)`"
	fi
esac

#--------------------------------------------------------------------
# Load the tclConfig.sh file
#--------------------------------------------------------------------

TEA_PATH_TCLCONFIG
TEA_LOAD_TCLCONFIG

#--------------------------------------------------------------------
# Load the tkConfig.sh file if necessary (Tk extension)
#--------------------------------------------------------------------

#TEA_PATH_TKCONFIG
#TEA_LOAD_TKCONFIG

#-----------------------------------------------------------------------
# Handle the --prefix=... option by defaulting to what Tcl gave.
# Must be called after TEA_LOAD_TCLCONFIG and before TEA_SETUP_COMPILER.
#-----------------------------------------------------------------------

TEA_PREFIX

#-----------------------------------------------------------------------
# Standard compiler checks.
# This sets up CC by using the CC env var, or looks for gcc otherwise.
# This also calls AC_PROG_CC, AC_PROG_INSTALL and a few others to create
# the basic setup necessary to compile executables.
#-----------------------------------------------------------------------

TEA_SETUP_COMPILER

#--------------------------------------------------------------------
# Load the xotclConfig.sh file
#--------------------------------------------------------------------

AC_MSG_NOTICE([Reading file ${with_xotcl}/xotclConfig.sh])
source ${with_xotcl}/xotclConfig.sh

#-----------------------------------------------------------------------
# __CHANGE__
# Specify the C source files to compile in TEA_ADD_SOURCES,
# public headers that need to be installed in TEA_ADD_HEADERS,
# stub library C source files to compile in TEA_ADD_STUB_SOURCES,
# and runtime Tcl library files in TEA_ADD_TCL_SOURCES.
# This defines PKG(_STUB)_SOURCES, PKG(_STUB)_OBJECTS, PKG_HEADERS
# and PKG_TCL_SOURCES.
#-----------------------------------------------------------------------
if test "${TEA_PLATFORM}" = "windows" ; then
    filemap="win32filemap.c"
else
    filemap="unixfilemap.c"
fi

#
# general fallback to the bundled expat
#
if test "${expat_dir}" = ""; then
    with_expat="bundle"
fi

if test "${with_expat}" = bundle ; then
    # 1) the bundled case
    AC_MSG_RESULT([using bundled expat distribution])
    TEA_ADD_SOURCES([xmltok.c ${filemap} xmlrole.c xmlwf.c codepage.c xmlparse.c hashtable.c tclexpat.c])
else
    # 2) the shared case
    AC_MSG_RESULT([using shared expat found in ${expat_dir}])
    TEA_ADD_SOURCES([tclexpat.c])
    TEA_ADD_INCLUDES([${EXPAT_INC_SPEC}])
    TEA_ADD_LIBS([${EXPAT_LIB_SPEC} -lexpat])
fi


TEA_ADD_HEADERS([])
TEA_ADD_INCLUDES([-I${with_xotcl}/generic ${XOTCL_BUILD_INCLUDE_SPEC}])
TEA_ADD_LIBS([$XOTCL_BUILD_STUB_LIB_SPEC])
TEA_ADD_CFLAGS([])
TEA_ADD_STUB_SOURCES([])
TEA_ADD_TCL_SOURCES([])

#--------------------------------------------------------------------
# __CHANGE__
# A few miscellaneous platform-specific items:
#
# Define a special symbol for Windows (BUILD_sample in this case) so
# that we create the export library with the dll.
#
# Windows creates a few extra files that need to be cleaned up.
# You can add more files to clean if your extension creates any extra
# files.
#
# TEA_ADD_* any platform specific compiler/build info here.
#--------------------------------------------------------------------

if test "${TEA_PLATFORM}" = "windows" ; then
    AC_DEFINE(BUILD_sample)
    CLEANFILES="pkgIndex.tcl *.lib *.dll *.exp *.ilk *.pdb vc*.pch"
    #TEA_ADD_SOURCES([win/winFile.c])
    #TEA_ADD_INCLUDES([-I\"$(${CYGPATH} ${srcdir}/win)\"])
else
    CLEANFILES="pkgIndex.tcl"
    #TEA_ADD_SOURCES([unix/unixFile.c])
    #TEA_ADD_LIBS([-lsuperfly])
fi
AC_SUBST(CLEANFILES)

#--------------------------------------------------------------------
# __CHANGE__
# Choose which headers you need.  Extension authors should try very
# hard to only rely on the Tcl public header files.  Internal headers
# contain private data structures and are subject to change without
# notice.
# This MUST be called after TEA_LOAD_TCLCONFIG / TEA_LOAD_TKCONFIG
#--------------------------------------------------------------------

TEA_PUBLIC_TCL_HEADERS
#TEA_PRIVATE_TCL_HEADERS

#TEA_PUBLIC_TK_HEADERS
#TEA_PRIVATE_TK_HEADERS
#TEA_PATH_X

#--------------------------------------------------------------------
# Check whether --enable-threads or --disable-threads was given.
#--------------------------------------------------------------------

TEA_ENABLE_THREADS

#--------------------------------------------------------------------
# The statement below defines a collection of symbols related to
# building as a shared library instead of a static library.
#--------------------------------------------------------------------

TEA_ENABLE_SHARED

#--------------------------------------------------------------------
# This macro figures out what flags to use with the compiler/linker
# when building shared/static debug/optimized objects.  This information
# can be taken from the tclConfig.sh file, but this figures it all out.
#--------------------------------------------------------------------

TEA_CONFIG_CFLAGS

#--------------------------------------------------------------------
# Set the default compiler switches based on the --enable-symbols option.
#--------------------------------------------------------------------

TEA_ENABLE_SYMBOLS

#--------------------------------------------------------------------
# Everyone should be linking against the Tcl stub library.  If you
# can't for some reason, remove this definition.  If you aren't using
# stubs, you also need to modify the SHLIB_LD_LIBS setting below to
# link against the non-stubbed Tcl library.  Add Tk too if necessary.
#--------------------------------------------------------------------

AC_DEFINE(USE_TCL_STUBS)
#AC_DEFINE(USE_TK_STUBS)

#--------------------------------------------------------------------
# This macro generates a line to use when building a library.  It
# depends on values set by the TEA_ENABLE_SHARED, TEA_ENABLE_SYMBOLS,
# and TEA_LOAD_TCLCONFIG macros above.
#--------------------------------------------------------------------

TEA_MAKE_LIB

#--------------------------------------------------------------------
# Find tclsh so that we can run pkg_mkIndex to generate the pkgIndex.tcl
# file during the install process.  Don't run the TCLSH_PROG through
# ${CYGPATH} because it's being used directly by make.
# Require that we use a tclsh shell version 8.2 or later since earlier
# versions have bugs in the pkg_mkIndex routine.
# Add WISH as well if this is a Tk extension.
#--------------------------------------------------------------------

TEA_PROG_TCLSH
#TEA_PROG_WISH

#--------------------------------------------------------------------
# Finally, substitute all of the various values into the Makefile.
# You may alternatively have a special pkgIndex.tcl.in or other files
# which require substituting th AC variables in.  Include these here.
#--------------------------------------------------------------------

AC_OUTPUT([Makefile])
























































