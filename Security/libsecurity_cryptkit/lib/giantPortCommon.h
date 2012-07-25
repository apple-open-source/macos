/* Copyright (c) 1998 Apple Computer, Inc.  All rights reserved.
 *
 * NOTICE: USE OF THE MATERIALS ACCOMPANYING THIS NOTICE IS SUBJECT
 * TO THE TERMS OF THE SIGNED "FAST ELLIPTIC ENCRYPTION (FEE) REFERENCE
 * SOURCE CODE EVALUATION AGREEMENT" BETWEEN APPLE COMPUTER, INC. AND THE
 * ORIGINAL LICENSEE THAT OBTAINED THESE MATERIALS FROM APPLE COMPUTER,
 * INC.  ANY USE OF THESE MATERIALS NOT PERMITTED BY SUCH AGREEMENT WILL
 * EXPOSE YOU TO LIABILITY.
 ***************************************************************************

   giantPortCommon.h - common header used to specify and access
         platform-dependent giant digit routines.

 Revision History
 ----------------
 1 Sep 98	Doug Mitchell at Apple
 	Created.

*******************************/

#ifndef _CRYPTKIT_GIANT_PORT_COMMON_H_
#define _CRYPTKIT_GIANT_PORT_COMMON_H_

#if  	defined(__i386__) && defined(__GNUC__)
/* Mac OS X, Intel, Gnu compiler */
/* This module doesn't compile yet, punt and use the 
 * inline C functions */
#include "giantPort_Generic.h"

#elif	defined(__ppc__) && defined(__MACH__)
/* Mac OS X, PPC, Gnu compiler */
#include "giantPort_PPC_Gnu.h"

#elif defined(__ppc__ ) && defined(macintosh)

/* Mac OS 9, PPC, Metrowerks */
#include "giantPort_PPC.h"

#else

/* Others */
#include "giantPort_Generic.h"

#endif

#endif	/* _CRYPTKIT_GIANT_PORT_COMMON_H_ */
