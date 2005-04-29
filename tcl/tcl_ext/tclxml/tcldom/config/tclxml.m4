# tclxml.m4 --
#
#	This file provides a set of autoconf macros to help 
#	the TEA-enabled TclXML package.
#
# Copyright (c) 2000 Zveno Pty Ltd
# http://www.zveno.com/
#
# Zveno makes this software available free of charge for any purpose.
# Copies may be made of this software but all of this notice must be included
# on any copy.
#
# The software was developed for research purposes only and Zveno does not
# warrant that it is error free or fit for any purpose.  Zveno disclaims any
# liability for all claims, expenses, losses, damages and costs any user may
# incur as a result of using, copying or modifying this software.
#
# $Id: tclxml.m4,v 1.3 2002/02/18 06:58:46 balls Exp $

#------------------------------------------------------------------------
# SC_LIB_SPEC_PATH --
#
#	Locate the library file, which may be static or shared.
#	If the library is not found, then disable building the module.
#
# Arguments:
#	None.
#
# Results:
#
#	Defines the following vars:
#		${basename}_LIB_DIR	Full path to the directory containing
#					the library file.
#		${basename}_LIB_NAME	Library file found.
#		${basename}_LIB_PATH_NAME
#					Full path of the library file.
#		${basename}_TCL_LIB_FILE
#					Name of the Tcl wrapper library to build
#		${basename}_TCL_LD_LIBS	Flags for ld
#		BUILD_${basename}	Flag for enabling/disabling building 
#					the module.
#------------------------------------------------------------------------

AC_DEFUN(SC_LIB_SPEC_PATH, [
    AC_ARG_ENABLE($1,[  --enable-$1		$2 [--disable-$1]],
	[ok=$enableval], [ok=no])
    if test "$ok" = "yes" ; then
	BUILD_$1=yes
    else
	BUILD_$1=no
    fi

    if test "$ok" = "yes" ; then
	# reset sc_lib_name_dir, just in case
	sc_lib_name_dir=

	#
	# First, look for one installed.
	# The alternative search directory is invoked by --with--${basename}
	#

	AC_ARG_WITH($1, [  --with-$1		directory containing the library (lib$1.a)], with_$1=${withval})
	AC_MSG_CHECKING([for $1 library file])

	# First check to see if --with-${basename} was specified
	if test x"${with_$1}" != x ; then
	    for i in \
		${with_$1}/lib$1*.a \
		${with_$1}/lib$1*.so ; do
		if test -f "$i" ; then
		    sc_lib_name_dir=`dirname $i`
		    $1_LIB_NAME=`basename $i`
		    $1_LIB_PATH_NAME=$i
		    sc_shlib_name=`basename $i`
		    break
		fi
	    done
	    if test x"$sc_lib_name_dir" = x ; then
		AC_MSG_ERROR([${with_lib} directory doesn't contain library])
	    fi
	fi

	# then check for a private installation
	if test x"${sc_lib_name_dir}" = x ; then
	    for i in \
		../$1 \
		../../$1 \
		../../../$1 ; do
		for j in \
			$i/lib$1*.a \
			$i/lib$1*.so ; do
		    if test -f "$i" ; then
			sc_lib_name_dir=`dirname $i`
			$1_LIB_NAME=`basename $i`
			$1_LIB_PATH_NAME=$i
			sc_shlib_name=`basename $i`
			break
		    fi
		done
		if test x"${sc_lib_name_dir}" != x ; then
		    break
		fi
	    done
	fi

	# check in the Tcl library directory
	if test x"${sc_lib_name_dir}" = x ; then
	    if test x"${exec_prefix}" != x"NONE" ; then
		sc_c_lib_name_dir="${exec_prefix}/lib"
	    elif test x"${prefix}" != x"NONE" ; then
		sc_c_lib_name_dir="${prefix}/lib"
	    else
		eval "sc_c_lib_name_dir=${libdir}"
	    fi
	    for i in \
		${sc_c_lib_name_dir}/lib$1*.a \
		${sc_c_lib_name_dir}/lib$1*.so ; do
		if test -f "$i" ; then
		    sc_lib_name_dir=`dirname $i`
		    $1_LIB_NAME=`basename $i`
		    sc_shlib_name=`basename $i`
		    $1_LIB_PATH_NAME=$i
		    break
		fi
	    done
	fi

	# check in the usual library directories
	if test x"${sc_lib_name_dir}" = x ; then
	    for i in \
		/usr/lib/lib$1*.a \
		/usr/lib/lib$1*.so \
		/usr/local/lib/lib$1*.a \
		/usr/local/lib/lib$1.so ; do
		if test -f "$i" ; then
		    sc_lib_name_dir=`dirname $i`
		    $1_LIB_NAME=`basename $i`
		    sc_shlib_name=`basename $i`
		    $1_LIB_PATH_NAME=$i
		    break
		fi
	    done
	fi

	if test x"${sc_lib_name_dir}" = x ; then
	    BUILD_$1=no
	    $1_TCL_LIB_FILE=
	    SHLIB_TCL_$1_LD_LIBS=
	    $1_DIR=
	    AC_MSG_WARN(Can't find $1 library)
	else
	    BUILD_$1=yes
	    $1_TCL_LIB_FILE=libtcl$1${TCL_SHLIB_SUFFIX}
	    $1_LIB_DIR=$sc_lib_name_dir
	    $1_DIR=`dirname $sc_lib_name_dir`
	    $1_TCL_LIB_NAME=tcl$1
	    case "`uname -s`" in
		*win32* | *WIN32* | *CYGWIN_NT*)
		    SHLIB_TCL_$1_LD_LIBS=\"-L$sc_lib_name_dir -l$sc_shlib_name\"
		    ;;
		*)
		    # strip off leading "lib" and trailing ".a" or ".so"
		    sc_shlib_name=`echo ${sc_shlib_name}|sed -e 's/^lib//' -e 's/\.[[^.]]*$//'`
		    SHLIB_TCL_$1_LD_LIBS="-L${sc_lib_name_dir} -l${sc_shlib_name}"
		    ;;
	    esac
	    AC_MSG_RESULT(found ${$1_LIB_PATH_NAME})
	fi
    fi
])

