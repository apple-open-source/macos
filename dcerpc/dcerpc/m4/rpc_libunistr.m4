dnl Search for a UNICODE converter library.
dnl
dnl We are looking for a library that has the ambstowc16s() and
dnl awc16stombs() APIs, or a library that we can use to trivially
dnl implement them.

AC_DEFUN([RPC_LIBUNISTR],
dnl   - $1 shell variable to add include path to
dnl   - $2 shell variable to add linker path to
dnl   - $3 shell variable to add linker path to
dnl   - $4 action if not found
dnl
[

dnl I'm not sure why we need both of these libunistr tests. It
dnl doesn't seem necessary, but I'm afraid I'll break someone's build
dnl by removing one of them. -- jpeach

AC_ARG_WITH(
[unistr-dir],
[AS_HELP_STRING([--with-unistr-dir=DIR], [look for libunistr in DIR])],
[
    _rpc_libunistr_CPPFLAGS=-I$withval/include
    _rpc_libunistr_LDFLAGS=-L$withval/`basename $libdir`
])

AC_ARG_WITH(
[unistr-libdir],
[AS_HELP_STRING([--with-unistr-libdir=DIR], [look for libunistr libraries in DIR])],
[
    _rpc_libunistr_LDFLAGS="-L$withval"
])

_rpc_libunistr_found=no

_rpc_libunistr_save_CPPFLAGS=$CPPFLAGS
_rpc_libunistr_save_LDFLAGS=$LDFLAGS
_rpc_libunistr_save_LIBS=$LIBS

CPPFLAGS=$_rpc_libunistr_CPPFLAGS
LDFLAGS=$_rpc_libunistr_LDFLAGS
LIBS=

AC_MSG_NOTICE([searching for a UNICODE converter])

AC_SEARCH_LIBS(ambstowc16s, unistr,
	[
	    _rpc_libunistr_found=yes
	    AC_SEARCH_LIBS(awc16stombs, unistr, [],
		[
		    dnl We expect libunistr to contain both APIs.
		    AC_MSG_ERROR([libunistr is incomplete])
		])
	],
	[
	    CPPFLAGS=
	    LDFLAGS=
	    LIBS=

	    AC_CHECK_HEADERS([CoreFoundation/CFStringEncodingConverter.h],
	    [
		LIBS="-framework CoreFoundation"
		AC_LINK_IFELSE(
		    [
			#include <CoreFoundation/CFStringEncodingConverter.h>
			int main(void) {
			    CFStringEncodingBytesToUnicode(
				0 /* encoding */, 0 /* flags */,
				NULL /* bytes */, 0 /* numBytes */,
				NULL /* usedByteLen */, NULL /* characters */,
				0 /* maxCharLen */, NULL /* usedCharLen */);
			    return 0;
			}
		    ],
		    [
			_rpc_libunistr_found=yes
		    ])
	    ])
])

if test "x$_rpc_libunistr_found" = xyes ; then
    # Found a UNICODE converter, set the variables
    $1="${$1} $CPPFLAGS"
    $2="${$2} $LDFLAGS"
    $3="${$3} $LIBS"
else
    # Failed to find a UNICODE converter, do the if-not-found action.
    $4
fi

CPPFLAGS=$_rpc_libunistr_save_CPPFLAGS
LDFLAGS=$_rpc_libunistr_save_LDFLAGS
LIBS=$_rpc_libunistr_save_LIBS

unset _rpc_libunistr_found
unset _rpc_libunistr_save_CPPFLAGS
unset _rpc_libunistr_save_LDFLAGS
unset _rpc_libunistr_save_LIBS
unset _rpc_libunistr_CPPFLAGS
unset _rpc_libunistr_LDFLAGS

])


