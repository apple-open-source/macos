/*
 * Platform-dependent stuff for ckutils
 */

#ifndef	_CKU_PLATFORM_H_
#define _CKU_PLATFORM_H_

#include <string.h>
#include <stdlib.h>

/* use this for linux compatibility testing */
//#define linux 1
/* end linux test */

/*
 * Make sure endianness is defined...
 */
#if	!defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
    #if	macintosh
	#define	__BIG_ENDIAN__		1
    #elif defined(i386) || defined(i486) || defined(__i386__) || defined(__i486__)
    	#define __LITTLE_ENDIAN__	1
    #else
    	#error Platform dependent work needed
    #endif
#endif	/* endian */

#ifdef	NeXT
    #import <libc.h>
    #define SRAND(x)		srandom(x)
    #define RAND()		random()
#else	NeXT
    /*
     * Use stdlib only
     */
    #define SRAND(x)		srand(x)
    #define RAND()		rand()
    #define bcopy(s, d, l)	memmove(d, s, l)
    #define bzero(s, l)		memset(s, 0, l)
    #define bcmp(s, d, l)	memcmp(s, d, l)
#endif
	
#ifdef	CK_NT_C_ONLY

/*
 * Standard I/O redirects for WINNT.
 */
#define open(f, b, c)	_open(f, b, c)
#define close(f)	_close(f)
#define read(f, b, c)	_read(f, b, c)
#define write(f, b, c)	_write(f, b, c)
#define	fstat(f, b)	_fstat(f, b)

#define	O_RDONLY	_O_RDONLY
#define	O_WRONLY	_O_WRONLY
#define	O_CREAT		_O_CREAT
#define O_TRUNC		_O_TRUNC

#endif	CK_NT_C_ONLY

/*
 * Platform-dependent timestamp stuff. For now we assume that each platform
 * has some kind of struct which maintains a high-resolution clock and
 * a function which fills that struct with the current time.
 */
#if		macintosh
	
	#include <Timer.h>
	
	#define PLAT_TIME		UnsignedWide
	#define PLAT_GET_TIME(pt)	Microseconds(&pt)
	#define PLAT_GET_US(start,end)	(end.lo - start.lo)

#elif	linux

	#include <sys/time.h>
	
	#define PLAT_TIME		struct timeval
	#define PLAT_GET_TIME(pt)	gettimeofday(&pt, NULL)
	#define PLAT_GET_US(start,end)						\
		( ( ( (end.tv_sec   & 0xff) * 1000000) + end.tv_usec) - 	\
		  ( ( (start.tv_sec & 0xff) * 1000000) + start.tv_usec) )

#elif	NeXT

	#include <kern/time_stamp.h>

	#define PLAT_TIME		struct tsval
	#define PLAT_GET_TIME(pt)	kern_timestamp(&pt)
	#define PLAT_GET_US(start,end)	(end.low_val - start.low_val)
	
	
#elif defined(__MACH__) && defined(__APPLE__)
	#include <CoreFoundation/CoreFoundation.h>
	/* time as a double */
	#define PLAT_TIME				CFAbsoluteTime
	#define PLAT_GET_TIME(pt)		pt = CFAbsoluteTimeGetCurrent()
	#define PLAT_GET_US(start,end)	\
		((end - start) * 1000000.0)
	#define PLAT_GET_NS(start,end)	\
		((end - start) * 1000000000.0)
#else
    #error Platform dependent work needed
#endif

#endif 	/* _CKU_PLATFORM_H_ */
