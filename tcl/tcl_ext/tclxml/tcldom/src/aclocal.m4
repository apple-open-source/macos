dnl
dnl $Id: aclocal.m4,v 1.1 2000/12/15 00:15:57 jenglish Exp $
dnl

dnl
dnl Specify where to find auxilliary configuration utilities:
dnl
CONFIGDIR=../config
AC_SUBST(CONFIGDIR)
AC_CONFIG_AUX_DIR(../config)
builtin(include,../config/tcl.m4)

dnl
dnl Stuff that should be in tcl.m4 but isn't:
dnl
#------------------------------------------------------------------------
# SC_CYGPATH --
#
#	Set the CYGPATH shell variable used by other SC_ macros. 
#
#------------------------------------------------------------------------
AC_DEFUN(SC_CYGPATH, [
    case "`uname -s`" in
	*win32* | *WIN32* | *CYGWIN_NT*|*CYGWIN_98*|*CYGWIN_95*)
	    CYGPATH="cygpath -w"
	;;
	*)
	    CYGPATH=echo
	;;
    esac
])

