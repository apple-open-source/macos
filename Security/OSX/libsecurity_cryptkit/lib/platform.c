/* Copyright (c) 1998,2011,2014 Apple Inc.  All Rights Reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 *
 * platform.c - platform-dependent C functions
 *
 * Revision History
 * ----------------
 *  6 Sep 96 at NeXT
 *	Created.
 */

#include "platform.h"
#include <stdio.h>
#include "feeDebug.h"
#ifdef	NeXT

/*
 * OpenStep....
 */
void CKRaise(const char *reason) {
	#if	FEE_DEBUG
	printf("CryptKit fatal error: %s\n", reason);
	#endif
	exit(1);
}

#import "feeDebug.h"

#if !defined(NeXT_PDO) && FEE_DEBUG

/*
 * Mach, private build. use quick microsecond-accurate system clock.
 */

#include <kern/time_stamp.h>

unsigned createRandomSeed()
{
	struct tsval tsp;

	(void)kern_timestamp(&tsp);
	return tsp.low_val;
}

#else

/*
 * OpenStep, normal case.
 */
#include <sys/types.h>
#include <time.h>

extern int getpid();

unsigned createRandomSeed(void)
{
	time_t curTime;
	unsigned thisPid;

  	time(&curTime);
	thisPid = (unsigned)getpid();

	return (unsigned)curTime ^ (unsigned)thisPid;
}

#endif	/* FEE_DEBUG */

#elif	WIN32

/*
 * OpenStep on Windows.
 */
#include <process.h>	/* for _getpid() */

void CKRaise(const char *reason) {
	#if	FEE_DEBUG
	printf("CryptKit fatal error: %s\n", reason);
	#endif
	exit(1);
}

extern void time(unsigned *tp);

unsigned createRandomSeed()
{
	unsigned curTime;
	unsigned thisPid;

  	time(&curTime);
	thisPid = _getpid();
	return (unsigned)curTime ^ (unsigned)thisPid;
}


#elif	__MAC_BUILD__

/*
 * Macintosh, all flavors.
 */
#include <stdlib.h>
#include <time.h>

void CKRaise(const char *reason) {
	#if	FEE_DEBUG
	printf("CryptKit fatal error: %s\n", reason);
	#endif
	exit(1);
}

/* for X, this isn't used except for testing when SecurityServer when
 * Yarrow is not running. So let's strip it down so we don't have
 * to link against CarbonCore. 
 */
#define BARE_BONES_SEED		1
#if 	BARE_BONES_SEED

#include <sys/types.h>

extern int getpid();

unsigned createRandomSeed()
{
	time_t curTime;
	unsigned thisPid;

  	time(&curTime);
	thisPid = (unsigned)getpid();

	return (unsigned)curTime ^ (unsigned)thisPid;
}

#else	/* BARE_BONES_SEED */

#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/Timer.h>
#include <CoreServices/../Frameworks/CarbonCore.framework/Headers/LowMem.h>

// this is mighty pitiful anyway...
unsigned createRandomSeed()
{
	UnsignedWide curTime;		
	//unsigned ticks;				/* use 16 bits */
	unsigned rtnHi;
	unsigned rtnLo;
	
	/* FIXME - need a way to distinguish OS9x from Carbon. Carbon
	 * doesn't have LMGetTicks(). */
	 
	Microseconds(&curTime);		/* low 16 bits are pretty good */

	// Carbon hack
	// rtnHi = LMGetTicks();
	rtnHi = 0x5a5aa5a5;
	rtnLo = curTime.lo & 0xffff;
	return (rtnHi ^ rtnLo);
}
#endif	/* BARE_BONES_SEED */

#elif unix

/* try for generic UNIX */

void CKRaise(const char *reason) {
	#if	FEE_DEBUG
	printf("CryptKit fatal error: %s\n", reason);
	#endif
	exit(1);
}

#include <sys/types.h>
#include <time.h>

extern int getpid();

unsigned createRandomSeed()
{
	time_t curTime;
	unsigned thisPid;

  	time(&curTime);
	thisPid = (unsigned)getpid();

	return (unsigned)curTime ^ (unsigned)thisPid;
}


#else

#error platform-specific work needed in security_cryptkit/platform.c

#endif
