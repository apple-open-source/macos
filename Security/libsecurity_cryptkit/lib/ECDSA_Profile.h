/*
	File:		ECDSA_Profile.h

	Contains:	ECDSA Profiling support.

	Written by:	Doug Mitchell

	Copyright:	Copyright 1998 by Apple Computer, Inc., all rights reserved.

	Change History (most recent first):

		 <7>	10/06/98	ap		Changed to compile with C++.

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

#ifndef	_CK_ECDSA_PROFILE_H_
#define _CK_ECDSA_PROFILE_H_

#include "ckconfig.h"

#if CRYPTKIT_ECDSA_ENABLE

#include "feeDebug.h"

#ifdef	FEE_DEBUG
#define ECDSA_PROFILE	0
#else	/* FEE_DEBUG */
#define ECDSA_PROFILE	0	/* always off */
#endif	/* FEE_DEBUG */

#if	ECDSA_PROFILE

#include <kern/time_stamp.h>

/*
 * Unlike the profiling macros in feeDebug.h, these are intended to
 * be used for fragments of code, not entire functions.
 */
#define SIGPROF_START 				\
{						\
	struct tsval _profStartTime;		\
	struct tsval _profEndTime;		\
	kern_timestamp(&_profStartTime);

/*
 * This one goes at the end of the routine, just before the (only) return.
 * There must be a static accumulator (an unsigned int) on a per-routine basis.
 */
#define SIGPROF_END(accum)						\
	kern_timestamp(&_profEndTime);					\
	accum += (_profEndTime.low_val - _profStartTime.low_val);	\
}


/*
 * Accumulators.
 */
extern unsigned signStep1;
extern unsigned signStep2;
extern unsigned signStep34;
extern unsigned signStep5;
extern unsigned signStep67;
extern unsigned signStep8;
extern unsigned vfyStep1;
extern unsigned vfyStep3;
extern unsigned vfyStep4;
extern unsigned vfyStep5;
extern unsigned vfyStep6;
extern unsigned vfyStep7;

#else	/* ECDSA_PROFILE */

#define SIGPROF_START
#define SIGPROF_END(accum)

#endif	/* ECDSA_PROFILE */

#endif	/* CRYPTKIT_ECDSA_ENABLE */
#endif	/* _CK_ECDSA_PROFILE_H_ */
