dnl Copyright 2005 Red Hat, Inc
dnl
dnl Permission to use, copy, modify, distribute, and sell this software and its
dnl documentation for any purpose is hereby granted without fee, provided that
dnl the above copyright notice appear in all copies and that both that
dnl copyright notice and this permission notice appear in supporting
dnl documentation.
dnl
dnl The above copyright notice and this permission notice shall be included
dnl in all copies or substantial portions of the Software.
dnl
dnl THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
dnl OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
dnl MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
dnl IN NO EVENT SHALL THE OPEN GROUP BE LIABLE FOR ANY CLAIM, DAMAGES OR
dnl OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
dnl ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
dnl OTHER DEALINGS IN THE SOFTWARE.
dnl
dnl Except as contained in this notice, the name of the copyright holders shall
dnl not be used in advertising or otherwise to promote the sale, use or
dnl other dealings in this Software without prior written authorization
dnl from the copyright holders.
dnl

# XORG_RELEASE_VERSION
# --------------------
# Adds --with/without-release-string and changes the PACKAGE and
# PACKAGE_TARNAME to use "$PACKAGE{_TARNAME}-$RELEASE_VERSION".  If
# no option is given, PACKAGE and PACKAGE_TARNAME are unchanged.  Also
# defines PACKAGE_VERSION_{MAJOR,MINOR,PATCHLEVEL} for modules to use.
 
AC_DEFUN([XORG_RELEASE_VERSION],[
	AC_ARG_WITH(release-version,
			AC_HELP_STRING([--with-release-version=STRING],
				[Use release version string in package name]),
			[RELEASE_VERSION="$withval"],
			[RELEASE_VERSION=""])
	if test "x$RELEASE_VERSION" != "x"; then
		PACKAGE="$PACKAGE-$RELEASE_VERSION"
		PACKAGE_TARNAME="$PACKAGE_TARNAME-$RELEASE_VERSION"
		AC_MSG_NOTICE([Building with package name set to $PACKAGE])
	fi
	AC_DEFINE_UNQUOTED([PACKAGE_VERSION_MAJOR],
		[`echo $PACKAGE_VERSION | cut -d . -f 1`],
		[Major version of this package])
	PVM=`echo $PACKAGE_VERSION | cut -d . -f 2`
	if test "x$PVM" = "x"; then
		PVM="0"
	fi
	AC_DEFINE_UNQUOTED([PACKAGE_VERSION_MINOR],
		[$PVM],
		[Minor version of this package])
	PVP=`echo $PACKAGE_VERSION | cut -d . -f 3`
	if test "x$PVP" = "x"; then
		PVP="0"
	fi
	AC_DEFINE_UNQUOTED([PACKAGE_VERSION_PATCHLEVEL],
		[$PVP],
		[Patch version of this package])
])
