/* $XFree86: xc/lib/expat/expat_config.h,v 1.3 2003/05/19 18:12:11 tsi Exp $ */
/* config.h.  Generated automatically by configure.  */
/* config.h.in.  Generated automatically from configure.in by autoheader.  */

#include <X11/Xos.h>

/* Define to empty if the keyword does not work.  */
/* #undef const */

/* Define if you have a working `mmap' system call.  */
/* #undef  HAVE_MMAP */

/* Define to `long' if <sys/types.h> doesn't define.  */
/* #undef off_t */

/* Define to `unsigned' if <sys/types.h> doesn't define.  */
/* #undef size_t */

#ifndef X_NOT_POSIX

/* Define if you have the ANSI C header files.  */
#define STDC_HEADERS 1

/* Define if you have the <fcntl.h> header file.  */
#define HAVE_FCNTL_H 1

/* Define if you have the <unistd.h> header file.  */
#define HAVE_UNISTD_H 1

#endif

/* Define if you have the bcopy function.  */
/* #undef HAVE_BCOPY */

/* Define if you have the getpagesize function.  */
/* #undef HAVE_GETPAGESIZE */

/* Define if you have the memmove function.  */
#define HAVE_MEMMOVE 1

#if X_BYTE_ORDER==X_BIG_ENDIAN
#define BYTEORDER 4321
#endif

#if X_BYTE_ORDER==X_LITTLE_ENDIAN
#define BYTEORDER 1234
#endif

#ifndef BYTEORDER
#error "Unknown byte order"
#endif

#define XML_NS
#define XML_DTD

#define XML_CONTEXT_BYTES 1024

#ifndef HAVE_MEMMOVE
#ifdef HAVE_BCOPY
#define memmove(d,s,l) bcopy((s),(d),(l))
#else
#define memmove(d,s,l) ;punting on memmove;
#endif

#endif
