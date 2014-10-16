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
 * DES.h - raw DES encryption engine interface
 *
 * Revision History
 * ----------------
 * 31 Mar 97 at Apple
 *	Put per-instance data in struct _desInst
 * 21 Aug 96 at NeXT
 *	Broke out from NSDESCryptor.m
 * 22 Feb 96 at NeXT
 *	Created.
 */

#ifndef	_CK_DES_H_
#define _CK_DES_H_

#include "ckconfig.h"

#if CRYPTKIT_SYMMETRIC_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

#define DES_BLOCK_SIZE_BYTES		8	/* in bytes */
#define DES_KEY_SIZE_BITS			56	/* effective key size in bits */
#define DES_KEY_SIZE_BITS_EXTERNAL	64	/* clients actually pass in this much */
#define DES_KEY_SIZE_BYTES_EXTERNAL	(DES_KEY_SIZE_BITS_EXTERNAL / 8)

#define DES_MODE_STD	0	/* standard Data Encryption Algorithm */
#define DES_MODE_FAST	1	/* DEA without initial and final */
				/*    permutations for speed */
#define DES_MODE_128	2	/* DEA without permutations and with */
				/*    128-byte key (completely independent */
				/*    subkeys for each round) */

/*
 * Per-instance data.
 */
struct _desInst {
    	/* 8 16-bit subkeys for each of 16 rounds, initialized by setkey()
    	 */
	unsigned char kn[16][8];
	int desmode;
};

typedef struct _desInst *desInst;

int desinit(desInst dinst, int mode);
void dessetkey(desInst dinst, char *key);
void endes(desInst dinst, char *block);
void dedes(desInst dinst, char *block);
void desdone(desInst dinst);

#ifdef __cplusplus
}
#endif

#endif	/* CRYPTKIT_SYMMETRIC_ENABLE */

#endif	/*_CK_DES_H_*/
