
#------------------------------------------------------------------------
# TDOM_ENABLE_DTD --
#
#   Allows the building with DTD support
#
# Arguments:
#   None
#   
# Results:
#
#   Adds the following arguments to configure:
#       --enable-dtd=yes|no
#
#   Defines the following vars:
#
#   Sets the following vars:
#
#------------------------------------------------------------------------

AC_DEFUN(TDOM_ENABLE_DTD, [
    AC_MSG_CHECKING([wether to enable dtd support])
    AC_ARG_ENABLE(dtd,
    [  --enable-dtd            build with the dtd support [--enable-dtd]],
    [tcl_ok=$enableval], [tcl_ok=yes])

    if test "${enable_dt+set}" = set; then
        enableval="$enable_dtd"
        tcl_ok=$enableval
    else
        tcl_ok=yes
    fi

    if test "$tcl_ok" = "yes" ; then
        AC_MSG_RESULT([yes])
        AC_DEFINE(XML_DTD)
    else
        AC_MSG_RESULT([no])
    fi
])

#------------------------------------------------------------------------
# TDOM_ENABLE_NS --
#
#   Allows the building with namespace support
#
# Arguments:
#   None
#   
# Results:
#
#   Adds the following arguments to configure:
#       --enable-ns=yes|no
#
#   Defines the following vars:
#
#   Sets the following vars:
#
#------------------------------------------------------------------------

AC_DEFUN(TDOM_ENABLE_NS, [
    AC_MSG_CHECKING([wether to enable namespace support])
    AC_ARG_ENABLE(ns,
    [  --enable-ns             build with the namespace support [--enable-ns]],
    [tcl_ok=$enableval], [tcl_ok=yes])

    if test "${enable_ns+set}" = set; then
        enableval="$enable_ns"
        tcl_ok=$enableval
    else
        tcl_ok=yes
    fi

    if test "$tcl_ok" = "yes" ; then
        AC_MSG_RESULT([yes])
        AC_DEFINE(XML_NS)
    else
        AC_MSG_RESULT([no])
    fi
])

#------------------------------------------------------------------------
# TDOM_ENABLE_UNKNOWN --
#
#   Allows the building with (or without) the custom unknown command
#
# Arguments:
#   none
#   
# Results:
#
#   Adds the following arguments to configure:
#       --enable-unknown=yes|no
#
#   Defines the following vars:
#
#   Sets the following vars:
#
#------------------------------------------------------------------------

AC_DEFUN(TDOM_ENABLE_UNKNOWN, [
    AC_MSG_CHECKING([wether to enable built-in unknown command])
    AC_ARG_ENABLE(ucmd,
    [  --enable-unknown        enable built-in unknown command [--disable-unknown]],
    [tcl_ok=$enableval], [tcl_ok=no])

    if test "${enable_unknown+set}" = set; then
        enableval="$enable_unknown"
        tcl_ok=$enableval
    else
        tcl_ok=no
    fi

    if test "$tcl_ok" = "no" ; then
        AC_MSG_RESULT([no])
        AC_DEFINE(TDOM_NO_UNKNOWN_CMD)
    else
        AC_MSG_RESULT([yes])
    fi
])
#------------------------------------------------------------------------
# TDOM_ENABLE_TDOMALLOC --
#
#   Allows the building with tDOMs block allocator for nodes
#
# Arguments:
#   none
#
# Results:
#
#   Adds the following arguments to configure:
#       --enable-tdomalloc=yes|no
#
#   Defines the following vars:
#
#   Sets the following vars:
#
#------------------------------------------------------------------------

AC_DEFUN(TDOM_ENABLE_TDOMALLOC, [
    AC_MSG_CHECKING([wether to enable tDOMs block allocator])
    AC_ARG_ENABLE(tdomalloc,
    [  --enable-tdomalloc      build with the tDOM allocator [--enable-tdomalloc]],
    [tcl_ok=$enableval], [tcl_ok=yes])

    if test "${enable_tdomalloc+set}" = set; then
        enableval="$enable_tdomalloc"
        tcl_ok=$enableval
    else
        tcl_ok=yes
    fi

    if test "$tcl_ok" = "yes" ; then
        AC_MSG_RESULT([yes])
    else
        AC_MSG_RESULT([no])
        AC_DEFINE(USE_NORMAL_ALLOCATOR)
    fi
])

#------------------------------------------------------------------------
# TDOM_PATH_AOLSERVER
#
#   Allows the building with support for AOLserver 
#
# Arguments:
#   none
#   
# Results:
#
#   Adds the following arguments to configure:
#       --with-aolserver=...
#
#   Defines the following vars:
#       AOL_DIR Full path to the directory containing AOLserver distro
#
#   Sets the following vars:
#       NS_AOLSERVER 
#------------------------------------------------------------------------

AC_DEFUN(TDOM_PATH_AOLSERVER, [
    AC_MSG_CHECKING([for AOLserver configuration])
    AC_ARG_WITH(aol, 
    [  --with-aolserver        directory with AOLserver distribution],\
    with_aolserver=${withval})

    AC_CACHE_VAL(ac_cv_c_aolserver,[
    if test x"${with_aolserver}" != x ; then
        if test -f "${with_aolserver}/include/ns.h" ; then
            ac_cv_c_aolserver=`(cd ${with_aolserver}; pwd)`
        else
            AC_MSG_ERROR([${with_aolserver} directory doesn't contain ns.h])
        fi
    fi
    ])
    if test x"${ac_cv_c_aolserver}" = x ; then
        AC_MSG_RESULT([none found])
    else
        AOL_DIR=${ac_cv_c_aolserver}
        AC_MSG_RESULT([found AOLserver in $AOL_DIR])
        AC_DEFINE(NS_AOLSERVER)
        AC_DEFINE(USE_NORMAL_ALLOCATOR)
    fi
])

#------------------------------------------------------------------------
# TDOM_PATH_CONFIG --
#
#	Locate the tdomConfig.sh file
#
# Arguments:
#	None
#
# Results:
#
#	Adds the following arguments to configure:
#       --with-tdom=...
#
#	Defines the following vars:
#       TDOM_BIN_DIR   Full path to the directory with tdomConfig.sh
#------------------------------------------------------------------------

AC_DEFUN(TDOM_PATH_CONFIG, [
    if test x"${no_tdom}" = x ; then
	    AC_MSG_CHECKING([for tDOM configuration])
	    AC_ARG_WITH(tdom, 
        [  --with-tdom             directory with tDOM tdomConfig.sh],\
        with_tdomconfig=${withval})

	    no_tdom=true
        if test "${TEA_PLATFORM}" = "windows" ; then
            bindir=win
        else
            bindir=unix
        fi

	    AC_CACHE_VAL(ac_cv_c_tdomconfig,[

	    # First check to see if --with-tdom was specified.
	    if test x"${with_tdomconfig}" != x ; then
		    if test -f "${with_tdomconfig}/tdomConfig.sh" ; then
		        ac_cv_c_tdomconfig=`(cd ${with_tdomconfig}; pwd)`
		    else
		        AC_MSG_ERROR([${with_tdomconfig} directory doesn't contain tdomConfig.sh])
		    fi
	    fi
	    # Then check for a sibling installation
	    if test x"${ac_cv_c_tdomconfig}" = x ; then
		    for i in \
			    ../tdom `ls -dr ../tdom-* 2>/dev/null` \
			    ../../tdom `ls -dr ../../tdom-* 2>/dev/null` \
			    ../../../tdom `ls -dr ../../../tdom-* 2>/dev/null` ; do
		        if test -f "$i/$bindir/tdomConfig.sh" ; then
			        ac_cv_c_tdomconfig=`(cd $i/$bindir; pwd)`
			        break
		        fi
		    done
	    fi
	    # Check in a few common install locations
	    if test x"${ac_cv_c_tdomconfig}" = x ; then
		    for i in \
                `ls -d ${prefix}/lib 2>/dev/null` \
			    `ls -d /usr/local/lib 2>/dev/null` ; do
		        if test -f "$i/tdomConfig.sh" ; then
			        ac_cv_c_tdomconfig=`(cd $i; pwd)`
			        break
		        fi
		    done
	    fi
	    # Check in a few other private locations
	    if test x"${ac_cv_c_tdomconfig}" = x ; then
		for i in \
            ${srcdir}/../tdom \
            `ls -dr ${srcdir}/../tdom[[0-9]].[[0-9]]* 2>/dev/null` ; do
		        if test -f "$i/$bindir/tdomConfig.sh" ; then
		            ac_cv_c_tdomconfig=`(cd $i/$bindir; pwd)`
		            break
		        fi
		    done
	    fi
	    ])
	    if test x"${ac_cv_c_tdomconfig}" = x ; then
	        TDOM_BIN_DIR="# no tDOM configuration file found"
	        AC_MSG_WARN(Can't find tDOM configuration definitions)
	        exit 0
	    else
	        no_tdom=
	        TDOM_BIN_DIR=${ac_cv_c_tdomconfig}
	        AC_MSG_RESULT(found $TDOM_BIN_DIR/tdomConfig.sh)
	    fi
    fi
])

#------------------------------------------------------------------------
# TDOM_LOAD_CONFIG --
#
#	Load the tdomConfig.sh file
#
# Arguments:
#	
#	Requires the following vars to be set:
#		TDOM_BIN_DIR
#
#   Defines the following vars:
#
#   Sets the following vars:
#
#------------------------------------------------------------------------

AC_DEFUN(TDOM_LOAD_CONFIG, [
    AC_MSG_CHECKING([for existence of $TDOM_BIN_DIR/tdomConfig.sh])
    if test -f "$TDOM_BIN_DIR/tdomConfig.sh" ; then
        AC_MSG_RESULT([loading])
	    . $TDOM_BIN_DIR/tdomConfig.sh
    else
        AC_MSG_RESULT([file not found])
    fi
    AC_SUBST(TDOM_MAJOR_VERSION)
    AC_SUBST(TDOM_MINOR_VERSION)
    AC_SUBST(TDOM_PATCHLEVEL)
    AC_SUBST(TDOM_VERSION)
    AC_SUBST(TDOM_BUILD_STUB_LIB_SPEC)
    AC_SUBST(TDOM_STUB_LIB_SPEC)
    AC_SUBST(TDOM_SRC_DIR)
])

# EOF
