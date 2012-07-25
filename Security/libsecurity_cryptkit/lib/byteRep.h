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
 * byteRep.h - FEE portable byte representation support
 *
 * Revision History
 * ----------------
 * 17 Jul 97	Doug Mitchell at Apple
 *	Added signature routines.
 *  9 Jan 97	Doug Mitchell at NeXT
 *	Split off from ckutilities.h
 */

#ifndef	_CK_BYTEREP_H_
#define _CK_BYTEREP_H_

#include "feeTypes.h"
#include "giantIntegers.h"
#include "elliptic.h"
#include "curveParams.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Support for bytestream key and signature representation.
 */
int intToByteRep(int i, unsigned char *buf);
int shortToByteRep(short s, unsigned char *buf);
int giantToByteRep(giant g, unsigned char *buf);
int keyToByteRep(key k, unsigned char *buf);
int curveParamsToByteRep(curveParams *cp, unsigned char *buf);
int sigToByteRep(int magic,
	int version,
	int minVersion,
	giant g0,
	giant g1,
	unsigned char *buf);

int lengthOfByteRepGiant(giant g);
int lengthOfByteRepKey(key k);
int lengthOfByteRepCurveParams(curveParams *cp);
int lengthOfByteRepSig(giant g0,
	giant g1);

int byteRepToInt(const unsigned char *buf);
unsigned short byteRepToShort(const unsigned char *buf);
giant byteRepToGiant(const unsigned char *buf,
	unsigned bufLen,
	unsigned *giantLen);
key byteRepToKey(const unsigned char *buf,
	unsigned bufLen,
	int twist,
	curveParams *cp,
	unsigned *keyLen);	// returned
curveParams *byteRepToCurveParams(const unsigned char *buf,
	unsigned bufLen,
	unsigned *cpLen);
int byteRepToSig(const unsigned char *buf,
	unsigned bufLen,
	int codeVersion,
	int *sigMagic,				// RETURNED
	int *sigVersion,			// RETURNED
	int *sigMinVersion,			// RETURNED
	giant *g0,					// alloc'd  & RETURNED
	giant *g1);					// alloc'd  & RETURNED

#ifdef __cplusplus
}
#endif

#endif	/*_CK_BYTEREP_H_*/
