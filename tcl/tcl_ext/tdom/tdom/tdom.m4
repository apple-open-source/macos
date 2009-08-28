
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
    AC_MSG_CHECKING([whether to enable dtd support])
    AC_ARG_ENABLE(dtd,
        AC_HELP_STRING([--enable-dtd],
            [build with dtd support (default: on)]),
        [tcl_ok=$enableval], [tcl_ok=yes])

    if test "${enable_dtd+set}" = set; then
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
    AC_MSG_CHECKING([whether to enable namespace support])
    AC_ARG_ENABLE(ns,
        AC_HELP_STRING([--enable-ns],
            [build with XML namespace support (default: on)]),
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
    AC_MSG_CHECKING([whether to enable built-in unknown command])
    AC_ARG_ENABLE(ucmd,
        AC_HELP_STRING([--enable-unknown],
            [enable built-in unknown command (default: off)]),
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
    AC_MSG_CHECKING([whether to enable tDOMs block allocator])
    AC_ARG_ENABLE(tdomalloc,
        AC_HELP_STRING([--enable-tdomalloc],
            [build with the tDOM allocator (default: off)]),
        [tcl_ok=$enableval], [tcl_ok=no])

    if test "${enable_tdomalloc+set}" = set; then
        enableval="$enable_tdomalloc"
        tcl_ok=$enableval
    else
        tcl_ok=no
    fi

    if test "$tcl_ok" = "yes" ; then
        AC_MSG_RESULT([yes])
        TEA_ADD_SOURCES([generic/domalloc.c])
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
        AC_HELP_STRING([--with-aolserver],
            [directory with AOLserver distribution]),
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
        AOL_INCLUDES="-I\"${AOL_DIR}/include\""
        if test "`uname -s`" = Darwin ; then
            aollibs=`ls ${AOL_DIR}/lib/libns* 2>/dev/null`
            if test x"$aollibs" != x ; then
                AOL_LIBS="-L\"${AOL_DIR}/lib\" -lnsd -lnsthread"
            fi
        fi
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
                AC_HELP_STRING([--with-tdom],
                    [directory containig tDOM configuration (tdomConfig.sh)]),
                with_tdomconfig=${withval})

	    no_tdom=true
        if test "${TEA_PLATFORM}" = "windows" ; then
            tdom_bindir=win
        else
            tdom_bindir=unix
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
		        if test -f "$i/$tdom_bindir/tdomConfig.sh" ; then
			        ac_cv_c_tdomconfig=`(cd $i/$tdom_bindir; pwd)`
		        fi
		    done
	    fi
            # Then check if tnc/tdom are compilied in the source tree
	    if test x"${ac_cv_c_tdomconfig}" = x ; then
                    if test -f "../../$tdom_bindir/tdomConfig.sh" ; then 
		        ac_cv_c_tdomconfig=`(cd ../../$tdom_bindir; pwd)`
 	            fi
            fi
	    # Check in a few common install locations
	    if test x"${ac_cv_c_tdomconfig}" = x ; then
		    for i in \
                `ls -d ${prefix}/lib 2>/dev/null` \
			    `ls -d /usr/local/lib 2>/dev/null` ; do
		        if test -f "$i/tdomConfig.sh" ; then
			        ac_cv_c_tdomconfig=`(cd $i; pwd)`
		        fi
		    done
	    fi
	    # Check in a few other private locations
	    if test x"${ac_cv_c_tdomconfig}" = x ; then
		for i in \
            ${srcdir}/../tdom \
            `ls -dr ${srcdir}/../tdom[[0-9]].[[0-9]]* 2>/dev/null` ; do
		        if test -f "$i/$tdom_bindir/tdomConfig.sh" ; then
		            ac_cv_c_tdomconfig=`(cd $i/$tdom_bindir; pwd)`
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
    if test -f "${TDOM_BIN_DIR}/Makefile" ; then
        TDOM_STUB_LIB_SPEC=${TDOM_BUILD_STUB_LIB_SPEC}
    fi
    AC_SUBST(TDOM_VERSION)
    AC_SUBST(TDOM_STUB_LIB_SPEC)
    AC_SUBST(TDOM_SRC_DIR)
])

#------------------------------------------------------------------------
# TDOM_EXPORT_CONFIG --
#
#	Define the data to insert into the ${PACKAGE_NAME}Config.sh file
#
# Arguments:
#	None
#
# Results:
#	Subst the following vars:
#
#------------------------------------------------------------------------

AC_DEFUN(TDOM_EXPORT_CONFIG, [
    #--------------------------------------------------------------------
    # These are for ${PACKAGE_NAME}Config.sh
    #--------------------------------------------------------------------

    # pkglibdir must be a fully qualified path and (not ${exec_prefix}/lib)
    eval pkglibdir="[$]{libdir}/${PACKAGE_NAME}${PACKAGE_VERSION}"
    if test "${TCL_LIB_VERSIONS_OK}" = "ok"; then
	eval PKG_STUB_LIB_FLAG="-l${PACKAGE_NAME}stub${PACKAGE_VERSION}"
    else
	eval PKG_STUB_LIB_FLAG="-l${PACKAGE_NAME}stub`echo ${PACKAGE_VERSION} | tr -d .`"
    fi
    PKG_BUILD_STUB_LIB_SPEC="-L`pwd` ${PKG_STUB_LIB_FLAG}"
    PKG_STUB_LIB_SPEC="-L${pkglibdir} ${PKG_STUB_LIB_FLAG}"
    PKG_BUILD_STUB_LIB_PATH="`pwd`/[$]{PKG_STUB_LIB_FILE}"
    PKG_STUB_LIB_PATH="${pkglibdir}/[$]{PKG_STUB_LIB_FILE}"

    AC_SUBST(PKG_BUILD_STUB_LIB_SPEC)
    AC_SUBST(PKG_STUB_LIB_SPEC)
    AC_SUBST(PKG_BUILD_STUB_LIB_PATH)
    AC_SUBST(PKG_STUB_LIB_PATH)
])

# Local Variables:
# mode: autoconf
# End:
# EOF
