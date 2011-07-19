dnl Attempt to link the SMBClient framework
AC_DEFUN([LINK_SMBCLIENT_IFELSE],
dnl   - $1 action if successful
dnl   - $2 action if unsuccessful
[
    AC_LINK_IFELSE(
	[
	    #include <SMBClient/smbclient.h>
	    int main(void) {
		SMBHANDLE ctx;
		NTSTATUS s = SMBOpenServer("\\foo", &ctx);
	    }
	],
	[ $1 ],
	[ $2 ])
])

dnl Search for the SMBClient framework.
dnl
dnl We are looking for a the Darwin SMBClient framework, whose location
dnl varies depending on whether we are building on Snow Leopard or later.
AC_DEFUN([RPC_SMBCLIENT_FRAMEWORK],
dnl   - $1 shell variable to add include path to
dnl   - $2 shell variable to add linker path to
dnl   - $3 shell variable to add libs
dnl   - $4 action if not found
dnl
[

_rpc_smbclient_save_CPPFLAGS=$CPPFLAGS
_rpc_smbclient_save_LDFLAGS=$LDFLAGS
_rpc_smbclient_save_LIBS=$LIBS

for location in "$SDKROOT/System/Library/PrivateFrameworks" \
	    "$SDKROOT/AppleInternal/Library/Frameworks" ; do

    AC_MSG_CHECKING(for SMBClient.framework in $location)
    CPPFLAGS="$_rpc_smbclient_save_CPPFLAGS -F$location"
    LDFLAGS="$_rpc_smbclient_save_LDFLAGS -F$location"
    LIBS="$_rpc_smbclient_save_LIBS -framework SMBClient"

    LINK_SMBCLIENT_IFELSE(
	[ _rpc_smbclient_found=yes ],
	[ _rpc_smbclient_found=no ])
    AC_MSG_RESULT($_rpc_smbclient_found)

    if test "x$_rpc_smbclient_found" = "xyes" ; then
	$1="${$1} $CPPFLAGS"
	$2="${$2} $LDFLAGS"
	$3="${$3} $LIBS"
	break
    fi
done

if test "x$_rpc_smbclient_found" = "xno" ; then
    # Failed to find SMBClient framework, do the if-not-found action.
    $4
fi

CPPFLAGS=$_rpc_smbclient_save_CPPFLAGS
LDFLAGS=$_rpc_smbclient_save_LDFLAGS
LIBS=$_rpc_smbclient_save_LIBS

unset _rpc_smbclient_found
unset _rpc_smbclient_save_CPPFLAGS
unset _rpc_smbclient_save_LDFLAGS
unset _rpc_smbclient_save_LIBS
])
