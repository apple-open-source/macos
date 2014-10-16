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
 * ckutilities.h - general private C routine declarations
 *
 * Revision History
 * ----------------
 * 10/06/98		ap
 *	Changed to compile with C++.
 *  2 Aug 96 at NeXT
 *	Broke out from Blaine Garst's original NSCryptors.m
 */

#ifndef	_CK_UTILITIES_H_
#define _CK_UTILITIES_H_

#include "giantIntegers.h"
#include "elliptic.h"
#include "feeTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

unsigned char *mem_from_giant(giant x, unsigned *memLen);
giant giant_with_data(const unsigned char *d, int len);

void serializeGiant(giant g,
	unsigned char *cp,
	unsigned numBytes);

void deserializeGiant(const unsigned char *cp,
	giant g,
	unsigned numBytes);

#ifdef __cplusplus
}
#endif

#endif	/* _CK_UTILITIES_H_ */
