#
# m4 configure macros specific to Img.
#

AC_DEFUN(IMG_SRCPATH, [
#--------------------------------------------------------------------
# Compute an absolute path to the src directory of module '$1' so
# that we are able to find its headers even if they are not installed.
#--------------------------------------------------------------------

case [$]$1_SRC_DIR in
/*)	$1_SRC_PATH=[$]$1_SRC_DIR
	;;
*)	# SRC_DIR relative, splice with BUILD_PATH
	$1_SRC_PATH="`dirname [$]$1_BUILD_STUB_LIB_PATH`/[$]$1_SRC_DIR"
esac

$1_BUILD_PATH="`dirname [$]$1_BUILD_STUB_LIB_PATH`"

if test "[$]{TEA_PLATFORM}" = "windows" ; then
    $1_SRC_PATH="\"`[$]CYGPATH [$]$1_SRC_PATH`\""
    $1_BUILD_PATH="\"`[$]CYGPATH [$]$1_BUILD_PATH`\""
fi

AC_SUBST($1_SRC_PATH)
AC_SUBST($1_BUILD_PATH)
])
