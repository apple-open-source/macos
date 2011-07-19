dnl @synopsis ACX_WHICH_GETHOSTBYNAME_R
dnl
dnl Provides a test to determine the correct way to call gethostbyname_r
dnl
dnl defines HAVE_GETHOSTBYNAME_R to the number of arguments required
dnl
dnl e.g. 6 arguments (linux)
dnl e.g. 5 arguments (solaris)
dnl e.g. 3 arguments (osf/1)
dnl
dnl @version $Id: acinclude.m4,v 1.6 2001/10/17 07:19:14 brian Exp $
dnl @author Brian Stafford <brian@stafford.uklinux.net>
dnl
dnl based on version by Caolan McNamara <caolan@skynet.ie>
dnl based on David Arnold's autoconf suggestion in the threads faq
dnl with fixes and updates by Matthias Andree
dnl
AC_DEFUN([ACX_WHICH_GETHOSTBYNAME_R],
[AC_CACHE_CHECK(number of arguments to gethostbyname_r,
                acx_cv_which_gethostbyname_r, [
	AC_TRY_LINK([
#		include <netdb.h> 
  	], 	[

        char *name;
        struct hostent *he;
        struct hostent_data data;
        (void) gethostbyname_r(name, he, &data);

		],acx_cv_which_gethostbyname_r=3,
			[
dnl			acx_cv_which_gethostbyname_r=0
  AC_TRY_LINK([
#include <stdlib.h>
#   include <netdb.h>
  ], [
	char *name;
	struct hostent *he, *res;
	char *buffer = NULL;
	int buflen = 2048;
	int h_errnop;
	(void) gethostbyname_r(name, he, buffer, buflen, &res, &h_errnop)
  ],acx_cv_which_gethostbyname_r=6,
  
  [
dnl  acx_cv_which_gethostbyname_r=0
  AC_TRY_LINK([
#include <stdlib.h>
#   include <netdb.h>
  ], [
			char *name;
			struct hostent *he;
			char *buffer = NULL;
			int buflen = 2048;
			int h_errnop;
			(void) gethostbyname_r(name, he, buffer, buflen, &h_errnop)
  ],acx_cv_which_gethostbyname_r=5,acx_cv_which_gethostbyname_r=0)

  ]
  
  )
			]
		)
	])

if test $acx_cv_which_gethostbyname_r -gt 0 ; then
    AC_DEFINE_UNQUOTED([HAVE_GETHOSTBYNAME_R], $acx_cv_which_gethostbyname_r,
    		       [Number of parameters to gethostbyname_r or 0 if not available])
fi

])
