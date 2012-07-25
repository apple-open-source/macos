/*
	File:		feeDebug.h

	Contains:	Debug macros.

	Written by:	Doug Mitchell

	Copyright:	Copyright 1998 by Apple Computer, Inc.
                All rights reserved.

	Change History (most recent first):

	<9>	10/06/98	ap		Changed to compile with C++.

	To Do:
*/

/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************
 */

#ifndef	_CK_FEEDEBUG_H_
#define _CK_FEEDEBUG_H_

#include "giantIntegers.h"
#include "elliptic.h"
#include "curveParams.h"
#include <stdio.h>

#ifdef	__cplusplus
extern "C" {
#endif

#ifdef	NDEBUG
#define FEE_DEBUG	0
#else
#define FEE_DEBUG	1
#endif

/*
 * In utilities.c...
 */
extern void printGiant(const giant x);
extern void printGiantHex(const giant x);
extern void printGiantExp(const giant x);
extern void printKey(const key k);
extern void printCurveParams(const curveParams *p);

#if	FEE_DEBUG

#define dbgLog(x)	printf x


#else	/* FEE_DEBUG */

#define dbgLog(x)

#endif	/* FEE_DEBUG */

/*
 * Profiling.
 */
#define FEE_PROFILE	0		/* general purpose profile */
#define ELL_PROFILE	0		/* ell_even/ell_odd only */

#if	(FEE_PROFILE || ELL_PROFILE)
#include <kern/time_stamp.h>
#endif	/* (FEE_PROFILE || ELL_PROFILE) */

/*
 * Place this macro after the last local and before any code in a routine
 * to profile.
 */
#define CPROF_START 				\
	struct tsval _profStartTime;		\
	struct tsval _profEndTime;		\
	kern_timestamp(&_profStartTime);

/*
 * This one goes at the end of the routine, just before the (only) return.
 * There must be a static accumulator (an unsigned int) on a per-routine basis.
 */
#define CPROF_END(accum)						\
	kern_timestamp(&_profEndTime);					\
	accum += (_profEndTime.low_val - _profStartTime.low_val);

/*
 * Increment a profiling counter.
 */
#define CPROF_INCR(ctr)		ctr++

#if	FEE_PROFILE

#define PROF_START	CPROF_START
#define PROF_END(a)	CPROF_END(a)
#define PROF_INCR(ctr)	CPROF_INCR(ctr)

/*
 * As of 14 Apr 1998, we no longer time mulg or gsquare calls with this
 * mechanism; the time overhead is the same magnitude as the mulg. Instead
 * we'll just count the mulgs and gsquares.
 */
#define PROF_TIME_MULGS		0


/*
 * Fundamental ops
 */
extern unsigned ellAddTime;
extern unsigned whichCurveTime;
extern unsigned ellipticTime;
extern unsigned sigCompTime;

/*
 * low-level primitives
 */
extern unsigned numerDoubleTime;
extern unsigned numerPlusTime;
extern unsigned numerTimesTime;
extern unsigned denomDoubleTime;
extern unsigned denomTimesTime;
extern unsigned powerModTime;
extern unsigned modgTime;
extern unsigned binvauxTime;

/*
 * Counters for calculating microseconds per {mulg, feemod, ...}
 */
extern unsigned numMulg;
extern unsigned numFeemod;
extern unsigned numGsquare;
extern unsigned numBorrows;

extern void clearProfile();

#else	/* FEE_PROFILE */
#define PROF_START
#define PROF_END(a)
#define PROF_INCR(ctr)
#endif	/* FEE_PROFILE */

#if	ELL_PROFILE
extern unsigned	ellOddTime;
extern unsigned ellEvenTime;
extern unsigned numEllOdds;
extern unsigned numEllEvens;
extern void clearEllProfile();

#define EPROF_START	CPROF_START
#define EPROF_END(a)	CPROF_END(a)
#define EPROF_INCR(ctr)	CPROF_INCR(ctr)

#else	/* ELL_PROFILE */
#define EPROF_START
#define EPROF_END(a)
#define EPROF_INCR(ctr)
#endif	/* ELL_PROFILE */

/*
 * NULL gets defined externally if FEE_DEBUG is true..
 */
#if	!FEE_DEBUG
#ifndef	NULL
#define NULL ((void *)0)
#endif	/* NULL */
#endif	/* !FEE_DEBUG */

#if	FEE_DEBUG

#include "platform.h"

#define CKASSERT(expression) 				\
  ((expression) ? (void)0 : 				\
   (printf ("Assertion failed: " #expression 		\
      ", file " __FILE__ ", line %d.\n", __LINE__), 	\
    CKRaise("Assertion Failure")))

#else	/* FEE_DEBUG */

#define CKASSERT(expression)

#endif	/* FEE_DEBUG */

#ifdef	__cplusplus
}
#endif

#endif	/* _CK_FEEDEBUG_H_ */
