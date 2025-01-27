/*
	File:		ellipticMeasure.h

	Contains:	xxx put contents here xxx


	Copyright:	Copyright (c) 1998,2011,2014 Apple Inc.
                All rights reserved.

	Change History (most recent first):

	<7>	10/06/98	ap		Changed to compile with C++.

	To Do:
*/

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
 * Measurement of feemods and mulgs withing an elliptic_simple() call.
 */

#include "feeDebug.h"

#ifdef	FEE_DEBUG
#define ELLIPTIC_MEASURE	0
#else	// FEE_DEBUG
#define ELLIPTIC_MEASURE	0	/* always off */
#endif	// FEE_DEBUG

#if	ELLIPTIC_MEASURE

extern int doEllMeasure;	// gather stats on/off */
extern int bitsInN;
extern int numFeeMods;
extern int numMulgs;

#define START_ELL_MEASURE(n)		\
	doEllMeasure = 1;		\
	bitsInN = bitlen(n);		\
	numFeeMods = 0;			\
	numMulgs = 0;

#define END_ELL_MEASURE		doEllMeasure = 0;

#define INCR_FEEMODS			\
	if(doEllMeasure) {		\
		numFeeMods++;		\
	}

#define INCR_MULGS			\
	if(doEllMeasure) {		\
		numMulgs++;		\
	}

/*
 * These two are used around mulg() calls in feemod() itself; they
 * inhibit the counting of those mulg() calls.
 */
#define PAUSE_ELL_MEASURE				\
	{						\
		int tempEllMeasure = doEllMeasure;	\
		doEllMeasure = 0;

#define RESUME_ELL_MEASURE				\
		doEllMeasure = tempEllMeasure;		\
	}

#else	// ELLIPTIC_MEASURE

#define START_ELL_MEASURE(n)
#define END_ELL_MEASURE
#define INCR_FEEMODS
#define INCR_MULGS
#define PAUSE_ELL_MEASURE
#define RESUME_ELL_MEASURE

#endif	// ELLIPTIC_MEASURE
