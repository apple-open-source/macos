dnl Check for variable types.
dnl
AC_DEFUN(XINETD_CHECK_TYPE,
[AC_REQUIRE([AC_HEADER_STDC])dnl
AC_MSG_CHECKING(for $1)
AC_CACHE_VAL(xinetd_cv_type_$1,
[AC_EGREP_CPP($1, [#include <sys/types.h>
#if STDC_HEADERS
#include <stdlib.h>
#endif
#if HAVE_NETDB_H
#include <netdb.h>
#endif
#if HAVE_RLIM_T
#include <sys/resource.h>
#endif
#if HAVE_STDINT_H
#include <stdint.h>
#endif
#if HAVE_UNISTD_H
#include <unistd.h>
#endif], xinetd_cv_type_$1=yes, xinetd_cv_type_$1=no)])dnl
AC_MSG_RESULT($xinetd_cv_type_$1)
if test $xinetd_cv_type_$1 = no; then
   AC_DEFINE($1, $2)
fi
])
