dnl  Copyright 2005 Red Hat, Inc.
dnl
dnl  Permission to use, copy, modify, distribute, and sell this software
dnl  and its documentation for any purpose is hereby granted without
dnl  fee, provided that the above copyright notice appear in all copies
dnl  and that both that copyright notice and this permission notice
dnl  appear in supporting documentation, and that the name of Red Hat
dnl  not be used in advertising or publicity pertaining to distribution
dnl  of the software without specific, written prior permission.  Red
dnl  Hat makes no representations about the suitability of this software
dnl  for any purpose.  It is provided "as is" without express or implied
dnl  warranty.
dnl
dnl  RED HAT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
dnl  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN
dnl  NO EVENT SHALL RED HAT BE LIABLE FOR ANY SPECIAL, INDIRECT OR
dnl  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
dnl  OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
dnl  NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
dnl  CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
dnl
dnl Process this file with autoconf to create configure.

AC_PREREQ([2.57])
AC_INIT([util-macros],
        [1.1.6],
        [https://bugs.freedesktop.org/enter_bug.cgi?product=xorg])
AM_INIT_AUTOMAKE([foreign dist-bzip2])

# Since this is the package that installs the xorgversion.m4 file, we
# cannot use it here.  Therefore, we copy the code from the m4 file.
# Please try to keep them in sync.
AC_ARG_WITH(release-version,
		AC_HELP_STRING([--with-release-version=STRING]
			[Use release version string in package name]),
		[RELEASE_VERSION="$withval"],
		[RELEASE_VERSION=""])
if test "x$RELEASE_VERSION" != "x"; then
	PACKAGE="$PACKAGE-$RELEASE_VERSION"
	PACKAGE_TARNAME="$PACKAGE_TARNAME-$RELEASE_VERSION"
	AC_MSG_NOTICE([Building with package name set to $PACKAGE])
fi

AC_OUTPUT([Makefile xorg-macros.m4])
