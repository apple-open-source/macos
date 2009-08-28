builtin(include,tclconfig/tcl.m4)

#------------------------------------------------------------------------
# TLS_SSL_DIR --
#
#	Locate the installed ssl files
#
# Arguments:
#	None.
#
# Requires:
#	CYGPATH must be set
#
# Results:
#
#	Adds a --with-ssl-dir switch to configure.
#	Result is cached.
#
#	Substs the following vars:
#		SSL_DIR
#------------------------------------------------------------------------

AC_DEFUN(TLS_CHECK_SSL, [

    #--------------------------------------------------------------------
    # If the variable OPENSSL is set, we will build with the OpenSSL
    # libraries.  If it is not set, then we will use RSA BSAFE SSL-C
    # libraries instead of the default OpenSSL libaries.
    #--------------------------------------------------------------------

    AC_ARG_ENABLE(bsafe, [  --enable-bsafe          Use RSA BSAFE SSL-C libs.  Default is to use OpenSSL libs], OPENSSL="", OPENSSL="1")

    #--------------------------------------------------------------------
    # Establish the location of the root directory for OpenSSL.
    # If we're not using OpenSSL, set the root for BSAFE SSL-C.
    # If we're using BSAFE, define the BSAFE compiler flag.
    # The "FLAT_INC" flag is used in the BSAFE ssl.h header file and
    # doesn't seem to be referenced anywhere else.
    #--------------------------------------------------------------------
    if test -n "${OPENSSL}"; then
	SSL_DIR='/usr /usr/local'
	AC_DEFINE(NO_IDEA)
	AC_DEFINE(NO_RC5)
    else
	SSL_DIR='/usr/sslc /usr/local/sslc'
	AC_DEFINE(BSAFE)
	AC_DEFINE(FLAT_INC)
    fi
    
    AC_MSG_CHECKING([for SSL directory])

    AC_ARG_WITH(ssl-dir, [  --with-ssl-dir=DIR      SSL root directory], with_ssldir=${withval})

    AC_CACHE_VAL(ac_cv_c_ssldir, [
	# Use the value from --with-ssl-dir, if it was given
	if test x"${with_ssldir}" != x ; then
	    if test -d "${with_ssldir}" ; then
		ac_cv_c_ssldir=${with_ssldir}
	    else
		AC_MSG_ERROR([${with_ssldir} is not a valid directory])
	    fi
	else
	    list="`ls -d ${SSL_DIR} 2>/dev/null`"
	    for i in $list ; do
		if test -d "$i" ; then
		    ac_cv_c_ssldir=$i
		    break
		fi
	    done
	fi
    ])

    # Print a message based on how we determined the include path

    if test x"${ac_cv_c_ssldir}" = x ; then
	AC_MSG_ERROR([Could not find SSL directory.
Please specify its location with --with-ssl-dir])
    else
	SSL_DIR=${ac_cv_c_ssldir}
	AC_MSG_RESULT([${SSL_DIR}])
    fi


    #--------------------------------------------------------------------
    # The OpenSSL and BSAFE SSL-C directory structures differ.
    #--------------------------------------------------------------------

    if test -n "${OPENSSL}"; then
	SSL_LIB_DIR=${SSL_DIR}/lib
	SSL_INCLUDE_DIR=${SSL_DIR}/include
	if test ! -f "${SSL_INCLUDE_DIR}/openssl/opensslv.h"; then
	    AC_ERROR([bad ssl-dir: cannot find openssl/opensslv.h under ${SSL_INCLUDE_DIR}])
	fi
    else
	#--------------------------------------------------------------------
	# If we're using RSA BSAFE SSL-C, we need to establish what platform
	# we're running on before we can figure out some paths.
	# This step isn't necessary if we're using OpenSSL.
	#--------------------------------------------------------------------

	if test -z "${OPENSSL}"; then
	    AC_MSG_CHECKING([host type])
	    case "`uname -s`" in
		*win32* | *WIN32* | *CYGWIN_NT*|*CYGWIN_98*|*CYGWIN_95*)
		    PLATFORM=WIN32
		    ;;
		*SunOS*)
		    PLATFORM=SOLARIS
		    ;;
		HP-UX)
		    PLATFORM=HPUX
		    ;;
		*)
		    PLATFORM=LINUX
		    ;;
	    esac
	    AC_MSG_RESULT(${PLATFORM})
	fi
	SSL_LIB_DIR=${SSL_DIR}/${PLATFORM}/library/lib
	SSL_INCLUDE_DIR=${SSL_DIR}/${PLATFORM}/library/include
	if test ! -f "${SSL_INCLUDE_DIR}/crypto.h"; then
	    AC_ERROR([bad ssl-dir: cannot find crypto.h under ${SSL_INCLUDE_DIR}])
	fi
    fi

    AC_SUBST(SSL_DIR)
    AC_SUBST(SSL_LIB_DIR)
    AC_SUBST(SSL_INCLUDE_DIR)

    SSL_INCLUDE_DIR_NATIVE=\"`${CYGPATH} ${SSL_INCLUDE_DIR}`\"
    SSL_LIB_DIR_NATIVE=\"`${CYGPATH} ${SSL_LIB_DIR}`\"
    AC_SUBST(SSL_INCLUDE_DIR_NATIVE)
    AC_SUBST(SSL_LIB_DIR_NATIVE)

    #--------------------------------------------------------------------
    # If OpenSSL was built with gcc then there may be some symbols that need
    # resolving before we can load it into tclsh (__udivd3i on solaris.
    # Let the user specify if we need to add libgcc to the link line to
    # resolve these symbols.
    #
    # This doesn't seem to be necessary if the RSA BSAFE SSL-C libraries
    # are used instead of OpenSSL.
    #--------------------------------------------------------------------
    
    if test -n "${OPENSSL}"; then
	AC_MSG_CHECKING(if libgcc is needed to resolve openssl symbols)

	AC_ARG_WITH(gcclib, [  --with-gcclib           link with libgcc to resolve symbols in a gcc-built openssl library], GCCLIB="-lgcc", GCCLIB="")

	if test "x${GCCLIB}" = "x" ; then
	    AC_MSG_RESULT(no)
	else
	    AC_MSG_RESULT(yes)
	    AC_MSG_CHECKING(for gcc library location)
	    GCCPATH=`${CC} -print-libgcc-file-name | sed -e 's#[^/]*$##'`
	    GCCPATH="-L${GCCPATH}"
	    AC_MSG_RESULT(${GCCPATH})
	fi
    fi

])
