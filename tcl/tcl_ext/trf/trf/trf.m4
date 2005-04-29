
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
AC_CACHE_VAL(trf_cv_ZLIB_LIB_DIR, [trf_cv_ZLIB_LIB_DIR="$ZLIB_LIB_DIR"])


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
	    eval AC_DEFINE_UNQUOTED(OPENSSL_SUB, 1)
	    TRF_DEFS="$TRF_DEFS -DOPENSSL_SUB"
            break
         fi
         if test -r $dir/openssl/sha1.h ; then
            trf_cv_path_SSL_INCLUDE_DIR=$dir
	    eval AC_DEFINE_UNQUOTED(OPENSSL_SUB, 1)
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
AC_CACHE_VAL(trf_cv_SSL_LIB_DIR, [trf_cv_SSL_LIB_DIR="$SSL_LIB_DIR"])


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
AC_CACHE_VAL(trf_cv_BZ2_LIB_DIR, [trf_cv_BZ2_LIB_DIR="$BZ2_LIB_DIR"])


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
