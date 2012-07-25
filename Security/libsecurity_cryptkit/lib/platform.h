/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * platform.h - platform-dependent C functions
 *
 * Revision History
 * ----------------
 *  6 Sep 96	Doug Mitchell at NeXT
 *	Created.
 */

#ifndef	_CK_PLATFORM_H_
#define _CK_PLATFORM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdlib.h>

/* many ways to determin macintosh - different for 68k, PPC/OS9, X */
#if	defined(__POWERPC__) || defined(__CFM68K__) || defined(__APPLE__)
	#undef	__MAC_BUILD__
	#define	__MAC_BUILD__	1
#endif

/*
 * Make sure endianness is defined...
 */
#if defined(__BIG_ENDIAN__) && defined(__LITTLE_ENDIAN__)
#error Hey! multiply defined  endianness!
#endif
#if	!defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__)
    #if	__MAC_BUILD__
	#define	__BIG_ENDIAN__		1
    #elif __i386__ || __i486__
    	#define __LITTLE_ENDIAN__	1
    #else
    	#error Platform dependent work needed
    #endif
#endif	/* endian */

#ifndef	NeXT
    #define bcopy(s, d, l)	memmove(d, s, l)
    #define bzero(s, l)		memset(s, 0, l)
    #define bcmp(s, d, l)	memcmp(s, d, l)
#endif

/*
 * Other platform-dependent functions in platform.c.
 */

extern void CKRaise(const char *reason);

/*
 * Come up with some kind of "really" random int with which to seed the
 * random number generator.
 */
extern unsigned createRandomSeed(void);

#ifdef __cplusplus
}
#endif

#endif	/*_CK_PLATFORM_H_*/
