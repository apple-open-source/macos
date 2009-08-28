# xotcl.m4 --
#
#	This file provides a set of autoconf macros to help TEA-enable
#	a Tcl extension.
#
# Copyright (c) 1999 Scriptics Corporation.
# Copyright (c) 1999-2008 Gustaf Neumann, Uwe Zdun
#
# See the file "tcl-license.terms" for information on usage and redistribution
# of this file, and for a DISCLAIMER OF ALL WARRANTIES.

#------------------------------------------------------------------------
# SC_PATH_XOTCLCONFIG --
#
#	Locate the xotclConfig.sh file and perform a sanity check on
#	the Tcl compile flags
#
# Arguments:
#	none
#
# Results:
#
#	Adds the following arguments to configure:
#		--with-xotcl=...
#
#	Defines the following vars:
#		XOTCL_BIN_DIR	Full path to the directory containing
#				the xotclConfig.sh file
#------------------------------------------------------------------------

AC_DEFUN(SC_PATH_XOTCLCONFIG, [
    #
    # Ok, lets find the tcl configuration
    # First, look for one uninstalled.
    # the alternative search directory is invoked by --with-tcl
    #
    if test x"${no_xotcl}" = x ; then
	# we reset no_xotcl in case something fails here
	no_xotcl=true
	AC_ARG_WITH(xotcl, [  --with-xotcl              directory containing xotcl configuration (xotclConfig.sh)], with_xotclconfig=${withval})
	AC_MSG_CHECKING([for XOTcl configuration])
	AC_CACHE_VAL(ac_cv_c_xotclconfig,[

	    # First check to see if --with-xotcl was specified.
	    if test x"${with_xotclconfig}" != x ; then
		if test -f "${with_xotclconfig}/xotclConfig.sh" ; then
		    ac_cv_c_xotclconfig=`(cd ${with_xotclconfig}; pwd)`
		else
		    AC_MSG_ERROR([${with_xotclconfig} directory doesn't contain xotclConfig.sh])
		fi
	    fi

	    # then check for a private Tcl installation
	    if test x"${ac_cv_c_xotclconfig}" = x ; then
		for i in \
			${srcdir}/../xotcl \
			`ls -dr ${srcdir}/../xotcl-* 2>/dev/null` \
			${srcdir}/../../xotcl \
			`ls -dr ${srcdir}/../../xotcl-* 2>/dev/null` \
			${srcdir}/../../../xotcl \
			`ls -dr ${srcdir}/../../../xotcl-* 2>/dev/null` \
			${srcdir}/../../../../xotcl \
			`ls -dr ${srcdir}/../../../../xotcl-* 2>/dev/null` \
			${srcdir}/../../../../../xotcl \
			`ls -dr ${srcdir}/../../../../../xotcl-* 2>/dev/null` ; do
		    if test -f "$i/xotclConfig.sh" ; then
			ac_cv_c_xotclconfig=`(cd $i; pwd)`
			break
		    fi
		done
	    fi

	    # check in a few common install locations
	    if test x"${ac_cv_c_xotclconfig}" = x ; then
		for i in `ls -d ${prefix}/lib 2>/dev/null` \
			`ls -d /usr/local/lib 2>/dev/null` ; do
		    if test -f "$i/xotclConfig.sh" ; then
			ac_cv_c_xotclconfig=`(cd $i; pwd)`
			break
		    fi
		done
	    fi

	])

	if test x"${ac_cv_c_xotclconfig}" = x ; then
	    XOTCL_BIN_DIR="# no XOTcl configs found"
	    AC_MSG_WARN(Can't find XOTcl configuration definitions)
	    exit 0
	else
	    no_xotcl=
	    XOTCL_BIN_DIR=${ac_cv_c_xotclconfig}
	    AC_MSG_RESULT(found $XOTCL_BIN_DIR/xotclConfig.sh)
	fi
    fi
])

#------------------------------------------------------------------------
# SC_LOAD_XOTCLCONFIG --
#
#	Load the tclConfig.sh file
#
# Arguments:
#	
#	Requires the following vars to be set:
#		XOTCL_BIN_DIR
#
# Results:
#
#	Subst the vars:
#
#------------------------------------------------------------------------

AC_DEFUN(SC_LOAD_XOTCLCONFIG, [
    AC_MSG_CHECKING([for existence of $XOTCL_BIN_DIR/xotclConfig.sh])

    if test -f "$XOTCL_BIN_DIR/xotclConfig.sh" ; then
        AC_MSG_RESULT([loading])
	. $XOTCL_BIN_DIR/xotclConfig.sh
    else
        AC_MSG_RESULT([file not found])
    fi

    #
    # The eval is required to do the TCL_DBGX substitution in the
    # TCL_LIB_FILE variable
    #
    AC_SUBST(XOTCL_VERSION)
    AC_SUBST(XOTCL_MAJOR_VERSION)
    AC_SUBST(XOTCL_MINOR_VERSION)
    AC_SUBST(XOTCL_RELEASE_LEVEL)
    AC_SUBST(XOTCL_LIB_FILE)
    AC_SUBST(XOTCL_BUILD_LIB_SPEC)
    AC_SUBST(XOTCL_LIB_SPEC)
    AC_SUBST(XOTCL_STUB_LIB_FILE)
    AC_SUBST(XOTCL_BUILD_STUB_LIB_SPEC)
    AC_SUBST(XOTCL_STUB_LIB_SPEC)
    AC_SUBST(XOTCL_SRC_DIR)
])

