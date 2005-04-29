# tclxslt.m4 --
#
#	This file provides a set of autoconf macros to help 
#	the TEA-enabled TclXSLT package.
#
# Copyright (c) 2001 Zveno Pty Ltd
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
# $Id: tclxslt.m4,v 1.2 2001/08/30 10:31:40 doss Exp $


#------------------------------------------------------------------------
# SC_CHECK_HEADER --
#
#	verify if system header file is available 
#       ( using current CPPFLAGS setting )
#
#       Quieter alternative to AC_CHECK_HEADER
#
# Arguments:
#	HEADER-FILE, [ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND]]
#
# Results:
#
#	executes supplied if-found, if-not-found actions 
#       
#
#------------------------------------------------------------------------

AC_DEFUN(SC_CHECK_HEADER, [ 
  AC_TRY_CPP(
    [#include <$1>],
    ifelse([$2], , :, [$2]),
    ifelse([$3], , , [$3]))
])

#------------------------------------------------------------------------
# SC_CHECK_C_LIB --
#
#	verify if system library is available 
#       ( using current LDFLAGS var )
#
#       Quieter alternative to AC_CHECK_LIB for C libs only
#
# Arguments:
#	LIBRARY, FUNCTION [, ACTION-IF-FOUND [, ACTION-IF-NOT-FOUND [, OTHER-LIBRARIES]]]
#
# Results:
#
#	executes supplied if-found, if-not-found actions 
#       
#
#------------------------------------------------------------------------

AC_DEFUN(SC_CHECK_C_LIB, [

    ac_save_LIBS="$LIBS"  
    LIBS="-l$1 $5 $LIBS"

    AC_TRY_LINK(
      [/* We use char because int might match the return type of a gcc2
          builtin and then its argument prototype would still apply.  */
          char $2();
      ],
      [$2()],
      ifelse([$3], , :, [$3]),
      ifelse([$4], , , [$4]))

    LIBS="$ac_save_LIBS"
])

#------------------------------------------------------------------------
# SC_REQUIRE_LIB --
#
#
# Arguments:
#	LIBRARY, LIBRARY DESCRIPTION, FUNCTION, HEADER-FILE
#
# Results:
#
#	Appends to CPPFLAGS, LDFLAGS if library exists
#       Otherwise aborts config
#
#------------------------------------------------------------------------
AC_DEFUN(SC_REQUIRE_LIB, [

  #_cppflags="${CPPFLAGS}"
  #_ldflags="${LDFLAGS}"

  AC_MSG_CHECKING(for $2)
  AC_ARG_WITH($1,
    [  --with-$1=DIR         Path prefix of the $2 lib & include directories  ],
    [  
      case "$withval" in
        ""|yes|no)
          AC_MSG_WARN([called with --with-$1, but base directory not supplied])
          ;;
        * )
          CPPFLAGS="-I${withval}/include $CPPFLAGS"
          LDFLAGS="-L${withval}/lib $LDFLAGS"
          if test ! -d ${withval}; then
	    AC_MSG_ERROR([called with --with-$1, but base directory specified ( ${withval} ) does not exist or is not a directory])
          fi
        ;;
      esac ])

  SC_CHECK_HEADER($4, 
    SC_CHECK_C_LIB($1, $3, 
      AC_MSG_RESULT(yes),  
      AC_MSG_ERROR([linker cant link $1 library])),
    AC_MSG_ERROR([compiler cant see $1 library header file(s)]))
  
  #CPPFLAGS=${_cppflags}
  #LDFLAGS=${_ldflags}
])

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
    AC_ARG_ENABLE($1,[  --enable-$1	  $2 [--disable-$1]],
	[ok=$enableval], [ok=no])
    if test "$ok" = "yes" ; then
	BUILD_$1=yes
    else
	BUILD_$1=no
    fi

    if test "$ok" = "yes" ; then

	#
	# First, look for one installed.
	# The alternative search directory is invoked by --with--${basename}
	#

	AC_ARG_WITH($1, [  --with-$1=DIR	  directory containing the lib$1 libraries], with_$1=${withval})
	AC_MSG_CHECKING([for $1 library file])

	SC_LOCATE_LIB($1, ${with_$1}, yes)

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
	    SC_LIB_INC_PATH($1)
	fi
    fi
])



AC_DEFUN(SC_LOCATE_LIB, [

	# reset sc_lib_name_dir, just in case
	sc_lib_name_dir=

	# First check to see if a default path was specified
	if test x"$1" != x ; then
	    for i in \
		${2}/lib$1*.a \
		${2}/lib$1*.so ; do
		if test -f "$i" ; then
		    sc_lib_name_dir=`dirname $i`
		    $1_LIB_NAME=`basename $i`
		    $1_LIB_PATH_NAME=$i
		    sc_shlib_name=`basename $i`
		    break
		fi
	    done
	fi

	# then check local neigbourhood for a bundled (possibly customised) package or a private installation

	if test $3 = yes -a x"${sc_lib_name_dir}" = x ; then
	    for i in \
		$1  \
		../$1 \
		../../$1 \
		../../../$1 ; do
		for j in \
			$i/lib$1*.a \
			$i/lib$1*.so ; do
		    if test -f "$j" ; then
			sc_lib_name_dir=$i
			$1_LIB_NAME=`basename $j`
			$1_LIB_PATH_NAME=$j
			sc_shlib_name=${$1_LIB_NAME}
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

	# Finally Check in standard library directories - /lib, /usr/lib, /usr/local/lib
	if test x"{$sc_lib_name_dir}" = x ; then
	    for i in \
		/lib/lib$1.a \
		/lib/lib$1.so \
		/usr/lib/lib$1.a \
		/usr/lib/lib$1.so \
		/usr/local/lib/lib$1.a \
		/usr/local/lib/lib$1.so ; do
		echo "looking for libxslt in $i"
		if test -f "$i" ; then
		    sc_lib_name_dir=`dirname $i`
		    $1_LIB_NAME=`basename $i`
		    sc_shlib_name=`basename $i`
		    $1_LIB_PATH_NAME=$i
		    break
		fi
	    done
	fi
])

#------------------------------------------------------------------------
# SC_LIB_INC_PATH --
#
#	Locate the library include files
#	If not found, then disable building the module.
#
# Arguments:
#	None.
#
# Results:
#
#	Defines the following vars:
#		${basename}_INCLUDE_DIR Full path to the directory containing
#------------------------------------------------------------------------

AC_DEFUN(SC_LIB_INC_PATH, [

	# reset sc_inc_name_dir, just in case
	sc_inc_dir=

	#
	# First, look for one installed.
	# The alternative search directory is invoked by --with-${basename}
	#

	AC_ARG_WITH($1, [  --with-$1-inc=DIR	  directory containing the include file(s) for lib$1.a], with_$1=${withval})
	AC_MSG_CHECKING([for $1 include file(s)])

	# First check to see if --with-${basename-inc} was specified
	if test x"${with_$1_inc}" != x ; then
	    if test -d ${with_$1_inc}  ; then
		sc_inc_dir=${with_$1_inc}
 	    else
		AC_MSG_ERROR([${with_$1_inc} isnt a directory])
	    fi
	fi

	# then check for a bundled (possibly customised) package or a private installation
	if test x"${sc_inc_dir}" = x ; then
	    for i in \
		$1/include  $1 \
		../$1/include  ../$1 \
                ../../$1/include ../../$1 \
		../../../$1/include  ../../../$1 ; do
	        if test -d "$i" ; then
		    sc_inc_dir=$i
		    break
		fi
	    done
	fi

	# check in the Tcl library directory
	if test x"${sc_inc_dir}" = x ; then
	    if test x"${exec_prefix}" != x"NONE" ; then
		sc_c_inc_dir="${exec_prefix}/include"
	    elif test x"${prefix}" != x"NONE" ; then
		sc_c_inc_dir="${prefix}/include"
	    else
		eval "sc_c_inc_dir=${includedir}"
	    fi
	    if test -d ${sc_c_inc_dir} ; then
	        sc_inc_dir=${sc_c_inc_dir}
	    fi
	fi

	# check in standard locations - /usr/include, /usr/local/include

	if test x"${sc_inc_dir}" = x ; then
	    for i in \
		/usr/include/$i \
		/usr/local/include/$i ; do
		if test -d "$i" ; then
		    sc_inc_dir=$i
		    break
		fi
	    done
	fi

	if test x"${sc_inc_dir}" = x ; then
	    $1_INCLUDE_DIR=
	    AC_MSG_WARN(Can't find $1 include files)
	else
	    $1_INCLUDE_DIR=$sc_inc_dir
	    AC_MSG_RESULT(found ${$1_INCLUDE_DIR})
	fi
])
