#------------------------------------------------------------------------
# TEA_PATH_CONFIG --
#
#	Locate the ${1}Config.sh file and perform a sanity check on
#	the ${1} compile flags.  These are used by packages like
#	[incr Tk] that load *Config.sh files from more than Tcl and Tk.
#
#	Normally aborts if the package could not be found. This can be
#	supressed by specifying a second argument with a value of "optional".
#
# Arguments:
#	$1	Name of the package to look for.
#	$2	Optional: Flag. If present and $2 == "optional"
#               then the script will _not_ abort when failing to
#		find the package.
#
# Results:
#
#	Adds the following arguments to configure:
#		--with-$1=...
#
#	Defines the following vars:
#		$1_BIN_DIR	Full path to the directory containing
#				the $1Config.sh file
#                               Contains a shell comment if nothing was found
#               HAVE_$1_PACKAGE Boolean 1 = Package found.
#------------------------------------------------------------------------

AC_DEFUN(TEA_PATH_CONFIG_X, [
    #
    # Ok, lets find the $1 configuration
    # First, look for one uninstalled.
    # the alternative search directory is invoked by --with-$1
    #

    if test x"${no_$1}" = x ; then
	# we reset no_$1 in case something fails here
	no_$1=true
	AC_ARG_WITH($1, [  --with-$1              directory containing $1 configuration ($1Config.sh)], with_$1config=${withval})
	AC_MSG_CHECKING([for $1 configuration])
	AC_CACHE_VAL(ac_cv_c_$1config,[

	    # First check to see if --with-$1 was specified.
	    if test x"${with_$1config}" != x ; then
		if test -f "${with_$1config}/$1Config.sh" ; then
		    ac_cv_c_$1config=`(cd ${with_$1config}; pwd)`
		else
		    AC_MSG_ERROR([${with_$1config} directory doesn't contain $1Config.sh])
		fi
	    fi

	    # then check for a private $1 installation
	    if test x"${ac_cv_c_$1config}" = x ; then
		for i in \
			../$1 \
			`ls -dr ../$1[[8-9]].[[0-9]]* 2>/dev/null` \
			../../$1 \
			`ls -dr ../../$1[[8-9]].[[0-9]]* 2>/dev/null` \
			../../../$1 \
			`ls -dr ../../../$1[[8-9]].[[0-9]]* 2>/dev/null` \
			${srcdir}/../$1 \
			`ls -dr ${srcdir}/../$1[[8-9]].[[0-9]]* 2>/dev/null` \
			; do
		    if test -f "$i/$1Config.sh" ; then
			ac_cv_c_$1config=`(cd $i; pwd)`
			break
		    fi
		    if test -f "$i/unix/$1Config.sh" ; then
			ac_cv_c_$1config=`(cd $i/unix; pwd)`
			break
		    fi
		done
	    fi

	    # check in a few common install locations
	    if test x"${ac_cv_c_$1config}" = x ; then
		for i in `ls -d ${exec_prefix}/lib 2>/dev/null` \
			`ls -d ${prefix}/lib 2>/dev/null` \
			`ls -d /usr/local/lib 2>/dev/null` \
			`ls -d /usr/contrib/lib 2>/dev/null` \
			`ls -d /usr/lib 2>/dev/null` \
			; do
		    if test -f "$i/$1Config.sh" ; then
			ac_cv_c_$1config=`(cd $i; pwd)`
			break
		    fi
		done
	    fi
	])

	if test x"${ac_cv_c_$1config}" = x ; then
	    $1_BIN_DIR="# no $1 configs found"
	    AC_MSG_WARN("Cannot find $1 configuration definitions")
	    if test "X$2" != "Xoptional" ; then
		exit 0
	    fi
	    HAVE_$1_PACKAGE=0
	else
	    no_$1=
	    $1_BIN_DIR=${ac_cv_c_$1config}
	    AC_MSG_RESULT([found $$1_BIN_DIR/$1Config.sh])
	    HAVE_$1_PACKAGE=1
	    AC_DEFINE([HAVE_$1_PACKAGE])
	fi
    fi
])


AC_DEFUN(TRF_FIND_ZLIB_SSL, [

AC_ARG_WITH(zlib,
	[  --with-zlib=DIR		zlib.h resides in DIR/include, libz resides in DIR/lib],
	[ZLIB_LIB_DIR=$withval/lib; ZLIB_INCLUDE_DIR=$withval/include],
	[])
dnl
AC_ARG_WITH(zlib-include-dir,
	[  --with-zlib-include-dir=DIR	zlib.h resides in DIR],
	[ZLIB_INCLUDE_DIR=$withval],
	[])
dnl
AC_ARG_WITH(zlib-lib-dir,
	[  --with-zlib-lib-dir=DIR	libz resides in DIR],
	[ZLIB_LIB_DIR=$withval],
	[])

AC_ARG_WITH(ssl,
	[  --with-ssl=DIR		md2.h/sha1.h reside in DIR/include, libcrypto resides in DIR/lib],
	[SSL_LIB_DIR=$withval/lib; SSL_INCLUDE_DIR=$withval/include],
	[])
dnl
AC_ARG_WITH(ssl-include-dir,
	[  --with-ssl-include-dir=DIR	md2.h/sha1.h reside in DIR],
	[SSL_INCLUDE_DIR=$withval],
	[])
dnl
AC_ARG_WITH(ssl-lib-dir,
	[  --with-ssl-lib-dir=DIR	libcrypto resides in DIR],
	[SSL_LIB_DIR=$withval],
	[])

AC_ARG_WITH(bz2,
	[  --with-bz2=DIR		bzlib.h resides in DIR/include, libbz2 resides in DIR/lib],
	[BZ2_LIB_DIR=$withval/lib; BZ2_INCLUDE_DIR=$withval/include],
	[])
dnl
AC_ARG_WITH(bz2-include-dir,
	[  --with-bz2-include-dir=DIR	bzlib.h resides in DIR],
	[BZ2_INCLUDE_DIR=$withval],
	[])
dnl
AC_ARG_WITH(bz2-lib-dir,
	[  --with-bz2-lib-dir=DIR	libbz2 resides in DIR],
	[BZ2_LIB_DIR=$withval],
	[])

AC_ARG_ENABLE(static-zlib,
	[  --enable-static-zlib         link 'zlib' statically],
	[STATIC_ZLIB=$enableval], [STATIC_ZLIB=no])

AC_ARG_ENABLE(static-bzlib,
	[  --enable-static-bzlib         link 'bzlib' statically],
	[STATIC_BZLIB=$enableval], [STATIC_BZLIB=no])

AC_ARG_ENABLE(static-md5,
	[  --enable-static-md5           link 'md5' statically],
	[STATIC_MD5=$enableval], [STATIC_MD5=no])

AC_ARG_ENABLE(trf_debug,
	[  --enable-trf-debug             enable debugging output],
	[trf_debug=$enableval], [trf_debug=no])

AC_ARG_ENABLE(stream_debug,
	[  --enable-stream-debug          enable debugging of IO streams],
	[stream_debug=$enableval], [stream_debug=no])


dnl ----------------------------------------------------------------
dnl
dnl Crossover between --with-zlib-include-dir and --with-zlib-lib-dir
dnl Setting one, but not the other will cause automatic definition
dnl of the missing part.

if test "X" = "X$ZLIB_LIB_DIR" -a "X" != "X$ZLIB_INCLUDE_DIR" 
then
    ZLIB_LIB_DIR="$ZLIB_INCLUDE_DIR/../lib"
fi

if test "X" = "X$ZLIB_INCLUDE_DIR" -a "X" != "X$ZLIB_LIB_DIR" 
then
    ZLIB_INCLUDE_DIR="$ZLIB_LIB_DIR/../include"
fi


dnl ----------------------------------------------------------------
dnl
dnl Crossover between --with-bz2-include-dir and --with-bz2-lib-dir
dnl Setting one, but not the other will cause automatic definition
dnl of the missing part.

if test "X" = "X$BZ2_LIB_DIR" -a "X" != "X$BZ2_INCLUDE_DIR" 
then
    BZ2_LIB_DIR="$BZ2_INCLUDE_DIR/../lib"
fi

if test "X" = "X$BZ2_INCLUDE_DIR" -a "X" != "X$BZ2_LIB_DIR" 
then
    BZ2_INCLUDE_DIR="$BZ2_LIB_DIR/../include"
fi


dnl ----------------------------------------------------------------
dnl
dnl Crossover between --with-ssl-include-dir and --with-ssl-lib-dir
dnl Setting one, but not the other will cause automatic definition
dnl of the missing part.

if test "X" = "X$SSL_LIB_DIR" -a "X" != "X$SSL_INCLUDE_DIR"
then
    SSL_LIB_DIR="$SSL_INCLUDE_DIR/../lib"
fi

if test "X" = "X$SSL_INCLUDE_DIR" -a "X" != "X$SSL_LIB_DIR"
then
    SSL_INCLUDE_DIR="$SSL_LIB_DIR/../include"
fi


dnl ----------------------------------------------------------------
dnl
dnl Locate zlib.h
dnl
dnl Searches:
dnl	ZLIB_INCLUDE_DIR	(--with-zlib, --with-zlib-include-dir)
dnl     TCL_INCLUDE_DIR		(--with-tcl, --with-tcl-include-dir)
dnl	$prefix/include		(--prefix)
dnl	/usr/local/include
dnl	/usr/include
dnl
AC_CACHE_CHECK(for directory with zlib.h,
	trf_cv_path_ZLIB_INCLUDE_DIR,
	[trf_cv_path_ZLIB_INCLUDE_DIR=""
	 places="$ZLIB_INCLUDE_DIR \
		$TCL_INCLUDE_DIR \
		$prefix/include \
		/usr/local/include \
		/usr/include"
     for dir in $places; do
         if test -r $dir/zlib.h ; then
            trf_cv_path_ZLIB_INCLUDE_DIR=$dir
            break
         fi
     done])
dnl
dnl verify success of search
dnl

if test -z "$trf_cv_path_ZLIB_INCLUDE_DIR" ; then
    AC_MSG_ERROR(not found; falling back to compat/ headers; use --with-zlib=DIR or --with-zlib-include-dir=DIR)
    ZLIB_INCLUDE_DIR="\".\""
else
    ZLIB_INCLUDE_DIR="\"`${CYGPATH} $trf_cv_path_ZLIB_INCLUDE_DIR`\""

    eval AC_DEFINE_UNQUOTED(HAVE_ZLIB_H, 1)
    TRF_TESTS="$TRF_TESTS hasZlib"
fi
dnl

AC_SUBST(ZLIB_INCLUDE_DIR)


dnl ----------------------------------------------------------------
dnl
dnl Locate zlib library
dnl Searches:
dnl	ZLIB_LIB_DIR		(--with-zlib, --with-zlib-lib-dir)
dnl	TCL_LIB_DIR		(--with-tcl, --with-tcl-lib-dir)
dnl	$exec_prefix/lib	(--exec-prefix)
dnl	$prefix/lib		(--prefix)
dnl	/usr/local/lib
dnl	/usr/lib
dnl
AC_CACHE_CHECK(for libz library,
	trf_cv_lib_ZLIB_LIB_DIR,
	[trf_cv_lib_ZLIB_LIB_DIR=""
	 places="$ZLIB_LIB_DIR \
	$TCL_LIB_DIR \
	 $exec_prefix/lib \
	 $prefix/lib \
	 /usr/local/lib \
	 /usr/lib"
    for dir in $places; do
        if test -n "$trf_cv_lib_ZLIB_LIB_DIR"; then
            break
        fi

        for libsuff in .so ".so.*" .sl .a .dylib; do
	    if test -n "$trf_cv_lib_ZLIB_LIB_DIR"; then
                    break
            fi
            if test -f "$dir/libz$libsuff"; then
                trf_cv_lib_ZLIB_LIB_DIR="$dir"
		    ZLIB_LIB_DIR="$dir"
            fi
        done
    done])

if test -z "$trf_cv_lib_ZLIB_LIB_DIR" ; then
    AC_MSG_WARN(not found; use --with-zlib-lib-dir=path)
    ZLIB_LIB_DIR="\".\""
else
    ZLIB_LIB_DIR="`${CYGPATH} $trf_cv_lib_ZLIB_LIB_DIR`"
fi

AC_SUBST(ZLIB_LIB_DIR)
dnl AC_CACHE_VAL(trf_cv_ZLIB_LIB_DIR, [trf_cv_ZLIB_LIB_DIR="$ZLIB_LIB_DIR"])


dnl ----------------------------------------------------------------
dnl
dnl Locate md2.h / sha1.h
dnl
dnl Searches:
dnl	SSL_INCLUDE_DIR		(--with-ssl, --with-ssl-include-dir)
dnl     TCL_INCLUDE_DIR		(--with-tcl, --with-tcl-include-dir)
dnl	$prefix/include		(--prefix)
dnl	/usr/local/ssl/include
dnl	/usr/local/include
dnl	/usr/include
dnl
AC_CACHE_CHECK(for directory with ssl.h,
	trf_cv_path_SSL_INCLUDE_DIR,
	[trf_cv_path_SSL_INCLUDE_DIR=""
	 places="$SSL_INCLUDE_DIR \
		$TCL_INCLUDE_DIR \
		$prefix/include \
		/usr/local/ssl/include \
		/usr/local/include \
		/usr/include"
     for dir in $places; do
         if test -r $dir/openssl/md2.h ; then
            trf_cv_path_SSL_INCLUDE_DIR=$dir
	    TRF_DEFS="$TRF_DEFS -DOPENSSL_SUB"
            break
         fi
         if test -r $dir/openssl/sha1.h ; then
            trf_cv_path_SSL_INCLUDE_DIR=$dir
	    TRF_DEFS="$TRF_DEFS -DOPENSSL_SUB"
            break
         fi
         if test -r $dir/md2.h ; then
            trf_cv_path_SSL_INCLUDE_DIR=$dir
            break
         fi
         if test -r $dir/sha1.h ; then
            trf_cv_path_SSL_INCLUDE_DIR=$dir
            break
         fi
     done])
dnl
dnl verify success of search
dnl

if echo "$TRF_DEFS" | grep -q OPENSSL_SUB; then
    AC_DEFINE_UNQUOTED(OPENSSL_SUB, 1)
fi

if test -z "$trf_cv_path_SSL_INCLUDE_DIR" ; then
    AC_MSG_WARN(not found; falling back compat/ headers; use --with-ssl=DIR or --with-ssl-include-dir=DIR)
    SSL_INCLUDE_DIR="\".\""
else
    SSL_INCLUDE_DIR="\"`${CYGPATH} $trf_cv_path_SSL_INCLUDE_DIR`\""

    if test "${TEA_PLATFORM}" = "windows" ; then
	AC_MSG_WARN([Squashing SSL / Windows])
	SSL_INCLUDE_DIR="\".\""
    else
        eval AC_DEFINE_UNQUOTED(HAVE_SSL_H, 1)
        eval AC_DEFINE_UNQUOTED(HAVE_MD2_H, 1)
        eval AC_DEFINE_UNQUOTED(HAVE_MD5_H, 1)
        eval AC_DEFINE_UNQUOTED(HAVE_SHA_H, 1)
        #DEFS="$DEFS -DHAVE_SSL_H -DHAVE_MD2_H -DHAVE_SHA_H"
        TRF_TESTS="$TRF_TESTS hasSSL"
    fi
fi
dnl

AC_SUBST(SSL_INCLUDE_DIR)


dnl ----------------------------------------------------------------
dnl
dnl Locate ssl library
dnl Searches:
dnl	SSL_LIB_DIR		(--with-ssl, --with-ssl-lib-dir)
dnl	TCL_LIB_DIR		(--with-tcl, --with-tcl-lib-dir)
dnl	$exec_prefix/lib	(--exec-prefix)
dnl	$prefix/lib		(--prefix)
dnl	/usr/local/ssl/lib
dnl	/usr/local/lib
dnl	/usr/lib
dnl
AC_CACHE_CHECK(for ssl libcrypto library (for message digests),
	trf_cv_lib_SSL_LIB_DIR,
	[trf_cv_lib_SSL_LIB_DIR=""
	 places="$SSL_LIB_DIR \
	$TCL_LIB_DIR \
	$exec_prefix/lib \
	$prefix/lib \
	/usr/local/ssl/lib \
	/usr/local/lib \
	/usr/lib"
    for dir in $places; do
        if test -n "$trf_cv_lib_SSL_LIB_DIR"; then
            break
        fi

        for libsuff in .so ".so.*" .sl .a .dylib; do
	    if test -n "$trf_cv_lib_SSL_LIB_DIR"; then
                    break
            fi
            if test -f $dir/libcrypto$libsuff; then
                trf_cv_lib_SSL_LIB_DIR="$dir"
		    SSL_LIB_DIR="$dir"
            fi
        done
    done])

if test -z "$trf_cv_lib_SSL_LIB_DIR" ; then
    AC_MSG_WARN(not found; use --with-ssl-lib-dir=path)
    SSL_LIB_DIR="\".\""
else
    SSL_LIB_DIR="`${CYGPATH} $trf_cv_lib_SSL_LIB_DIR`"
fi

AC_SUBST(SSL_LIB_DIR)
dnl AC_CACHE_VAL(trf_cv_SSL_LIB_DIR, [trf_cv_SSL_LIB_DIR="$SSL_LIB_DIR"])


dnl ----------------------------------------------------------------
dnl
dnl Locate bzlib.h
dnl
dnl Searches:
dnl	BZ2_INCLUDE_DIR		(--with-bz2, --with-bz2-include-dir)
dnl     TCL_INCLUDE_DIR		(--with-tcl, --with-tcl-include-dir)
dnl	$prefix/include		(--prefix)
dnl	/usr/local/include
dnl	/usr/include
dnl
AC_CACHE_CHECK(for directory with bzlib.h,
	trf_cv_path_BZ2_INCLUDE_DIR,
	[trf_cv_path_BZ2_INCLUDE_DIR=""
	 places="$BZ2_INCLUDE_DIR \
		$TCL_INCLUDE_DIR \
		$prefix/include \
		/usr/local/include \
		/usr/include"
     for dir in $places; do
         if test -r $dir/bzlib.h ; then
            trf_cv_path_BZ2_INCLUDE_DIR=$dir
            break
         fi
     done])
dnl
dnl verify success of search
dnl

if test -z "$trf_cv_path_BZ2_INCLUDE_DIR" ; then
    AC_MSG_WARN(not found; falling back to compat/ headers; use --with-bz2=DIR or --with-bz2-include-dir=DIR)
    BZ2_INCLUDE_DIR="\".\""
else
    BZ2_INCLUDE_DIR="\"`${CYGPATH} $trf_cv_path_BZ2_INCLUDE_DIR`\""
    eval AC_DEFINE_UNQUOTED(HAVE_BZ2_H, 1)
fi
dnl

AC_SUBST(BZ2_INCLUDE_DIR)

dnl ----------------------------------------------------------------
dnl
dnl Locate bz2 library
dnl Searches:
dnl	BZ2_LIB_DIR		(--with-bz2, --with-bz2-lib-dir)
dnl	TCL_LIB_DIR		(--with-tcl, --with-tcl-lib-dir)
dnl	$exec_prefix/lib	(--exec-prefix)
dnl	$prefix/lib		(--prefix)
dnl	/usr/local/bz2/lib
dnl	/usr/local/lib
dnl	/usr/lib
dnl
AC_CACHE_CHECK(for bz2 compressor library,
	trf_cv_lib_BZ2_LIB_DIR,
	[trf_cv_lib_BZ2_LIB_DIR=""
	 places="$BZ2_LIB_DIR \
	$TCL_LIB_DIR \
	$exec_prefix/lib \
	$prefix/lib \
	/usr/local/bz2/lib \
	/usr/local/lib \
	/usr/lib \
	/lib"
    for dir in $places; do
        if test -n "$trf_cv_lib_BZ2_LIB_DIR"; then
            break
        fi

        for libsuff in .so ".so.*" .sl .a .dylib; do
	    if test -n "$trf_cv_lib_BZ2_LIB_DIR"; then
                    break
            fi
            if test -f $dir/libbz2$libsuff; then
                trf_cv_lib_BZ2_LIB_DIR="$dir"
		    BZ2_LIB_DIR="$dir"
            fi
        done
    done])

if test -z "$trf_cv_lib_BZ2_LIB_DIR" ; then
    AC_MSG_WARN(not found; use --with-bz2-lib-dir=path)
    BZ2_LIB_DIR=.
else
    TRF_TESTS="$TRF_TESTS hasBz"
    BZ2_LIB_DIR="`${CYGPATH} $trf_cv_lib_BZ2_LIB_DIR`"
fi

AC_SUBST(BZ2_LIB_DIR)
dnl AC_CACHE_VAL(trf_cv_BZ2_LIB_DIR, [trf_cv_BZ2_LIB_DIR="$BZ2_LIB_DIR"])


if test "x$ZLIB_STATIC" = "xyes"
then
	eval AC_DEFINE_UNQUOTED(ZLIB_STATIC_BUILD, 1)
fi

if test "x$BZLIB_STATIC" = "xyes"
then
	eval AC_DEFINE_UNQUOTED(BZLIB_STATIC_BUILD, 1)
fi

if test "x$MD5_STATIC" = "xyes"
then
	eval AC_DEFINE_UNQUOTED(MD5_STATIC_BUILD, 1)
fi

if test "x$trf_debug" = "xyes"
then
	eval AC_DEFINE_UNQUOTED(TRF_DEBUG, 1)
fi

if test "x$stream_debug" = "xyes"
then
	eval AC_DEFINE_UNQUOTED(TRF_STREAM_DEBUG, 1)
fi


AC_SUBST(TRF_TESTS)

AC_HAVE_HEADERS(dlfcn.h stdlib.h features.h)
])
