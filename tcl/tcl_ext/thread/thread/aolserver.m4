
#------------------------------------------------------------------------
# NS_PATH_AOLSERVER
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
#       AOL_INCLUDES
#       AOL_LIBS
#
#   Sets the following vars:
#       NS_AOLSERVER 
#
#   Updates following vars:
#------------------------------------------------------------------------

AC_DEFUN(NS_PATH_AOLSERVER, [
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
        AOL_INCLUDES="-I\"${AOL_DIR}/include\""
        if test "`uname -s`" = Darwin ; then
            aollibs=`ls ${AOL_DIR}/lib/libns* 2>/dev/null`
            if test x"$aollibs" != x ; then
                AOL_LIBS="-L\"${AOL_DIR}/lib\" -lnsd -lnsthread"
            fi
        fi
        AC_DEFINE(NS_AOLSERVER)
    fi
])

# EOF
