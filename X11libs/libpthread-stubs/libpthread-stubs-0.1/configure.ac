AC_INIT([libpthread-stubs],
        0.1,
        [xcb@lists.freedesktop.org])
AC_CONFIG_SRCDIR([pthread-stubs.pc.in])
AM_INIT_AUTOMAKE([foreign dist-bzip2])

AC_CONFIG_HEADERS([config.h])

AC_PROG_LIBTOOL
AC_PROG_CC


dnl Detection code for compilers supporting the __attribute__((weak, alias))
dnl feature. Original code present in unieject's repository
dnl Diego Pettenò <flameeyes@gentoo.org>
ac_save_CFLAGS="$CFLAGS"
CFLAGS="$CFLAGS -Werror"
AC_CACHE_CHECK([if compiler supports __attribute__((weak, alias))],
	[cc_cv_attribute_alias],
	[AC_COMPILE_IFELSE([
		void other_function(void *foo) { }
		void some_function(void *foo) __attribute__((weak, alias("other_function")));
		],
		[cc_cv_attribute_alias=yes],
		[cc_cv_attribute_alias=no])
	])
CFLAGS="$ac_save_CFLAGS"
if test "x$cc_cv_attribute_alias" = "xyes"; then
	AC_DEFINE([SUPPORT_ATTRIBUTE_ALIAS], 1, [Define this if the compiler supports the alias attribute])
fi


PKG_CONFIG_LIBS=
AC_CHECK_FUNCS([pthread_self pthread_mutex_init pthread_mutex_destroy pthread_mutex_lock pthread_mutex_unlock pthread_cond_init pthread_cond_destroy pthread_cond_wait pthread_cond_signal pthread_cond_broadcast pthread_equal],
	       [], [PKG_CONFIG_LIBS='-L${libdir} -lpthread-stubs'])
AC_SUBST([PKG_CONFIG_LIBS])
AM_CONDITIONAL(BUILD_LIB, test "x$PKG_CONFIG_LIBS" != x)

AC_CONFIG_FILES([Makefile pthread-stubs.pc])
AC_OUTPUT
