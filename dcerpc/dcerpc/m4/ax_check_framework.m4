dnl Search for the named OS X framework.
dnl
dnl Usage: AX_CHECK_FRAMEWORK(Kerberos,
dnl            [action-if-found], [action-if-not-found])
dnl
dnl If the framework XXX is found, HAVE_XXX_FRAMEWORK is
dnl defined.

AC_DEFUN([AX_CHECK_FRAMEWORK],
dnl   - $1 name of the framework
dnl   - $2 action if found
dnl   - $3 action if not found
dnl
[
    AC_CACHE_CHECK(
	[checking for the ]$1[ framework],
	[ax_cv_framework_]$1,
	[
	    _ax_check_framework_CFLAGS="$CFLAGS"
	    _ax_check_framework_LIBS="$LIBS"
	    LIBS="$LIBS -framework $1"
	    AC_LINK_IFELSE(
		[AC_LANG_PROGRAM([], [])],
		[ax_cv_framework_]$1[=yes],
		[ax_cv_framework_]$1[=no]
	    )
	    CFLAGS="$_ax_check_framework_CFLAGS"
	    LIBS="$_ax_check_framework_LIBS"
	])

    if test "$ax_cv_framework_$1" = "yes" ; then
	# Run actions-if-true
	AC_DEFINE(AS_TR_CPP([HAVE_]$1[_FRAMEWORK]),1,
		            [Define if you have the ] $1 [ framework])
	$2
    else
	# Run actions-if-false
	$3
	:
    fi
])
