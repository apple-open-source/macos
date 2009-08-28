
/*
 *      features.h
 *
 *    Quick hack in order to be able to compile
 *    the md5-crypt library on other platforms.
 *
 *                              Jan Nijtmans
 */

#ifndef _FEATURES_H

#define _FEATURES_H

#ifndef __BEGIN_DECLS
#define __BEGIN_DECLS /**/
#define __END_DECLS   /**/
#define __restrict    /**/
#define __NORETURN    /**/
#endif

#define __USE_GNU

#ifndef __P
#if defined (__STDC__) && __STDC__
# define __P(x) x
#else
# define __P(x) ()
#endif
#endif

#ifndef __ptr_t
#define __ptr_t void *
#endif

#if __GNUC__ >= 2 && defined(__USE_GNU)
typedef long double __long_double_t;
#endif

#include <errno.h>

#ifndef EOPNOTSUPP
#define EOPNOTSUPP 122
#endif

#ifndef __set_errno
# define __set_errno(val) errno = (val)
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#endif /* _FEATURES_H */
