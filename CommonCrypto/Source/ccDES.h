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
 * ccDES.h - raw DES encryption engine interface
 *
 * Revision History
 * ----------------
 * 31 Mar 97	Doug Mitchell at Apple
 *	Put per-instance data in struct _desInst
 * 21 Aug 96	Doug Mitchell at NeXT
 *	Broke out from NSDESCryptor.m
 * 22 Feb 96	Blaine Garst at NeXT
 *	Created.
 */

#ifndef	_CC_DES_H_
#define _CC_DES_H_

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DES_BLOCK_SIZE_BYTES		8	/* in bytes */
#define DES_KEY_SIZE_BITS			56	/* effective key size in bits */
#define DES_KEY_SIZE_BITS_EXTERNAL	64	/* clients actually pass in this much */
#define DES_KEY_SIZE_BYTES_EXTERNAL	(DES_KEY_SIZE_BITS_EXTERNAL / 8)

/*
 * Per-instance data.
 */
struct _desInst {
    	/* 8 16-bit subkeys for each of 16 rounds, initialized by setkey()
    	 */
	unsigned char kn[16][8];
};

typedef struct _desInst *desInst;

int ccDessetkey(desInst dinst, char *key, size_t keyLength);
void ccEndes(desInst dinst, char *blockIn, char *blockOut);
void ccDedes(desInst dinst, char *blockIn, char *blockOut);

/* triple DES */
struct _des3Inst {
	struct _desInst		desInst[3];
};
typedef struct _des3Inst *des3Inst;

int ccDes3setkey(des3Inst dinst, char *key, size_t keyLength);
void ccEndes3(des3Inst dinst, char *blockIn, char *blockOut);
void ccDedes3(des3Inst dinst, char *blockIn, char *blockOut);

#ifdef __cplusplus
}
#endif

#endif	/*_CK_DES_H_*/
