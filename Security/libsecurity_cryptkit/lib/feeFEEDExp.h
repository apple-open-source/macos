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
 * feeFEEDExp.h - generic FEED encryption object using 2:1 expansion
 *
 * Revision History
 * ----------------
 * 28 Aug 96	Doug Mitchell at NeXT
 *	Created.
 */

#ifndef	_CK_FEEFEEDEXP_H_
#define _CK_FEEFEEDEXP_H_

#if	!defined(__MACH__)
#include <ckconfig.h>
#include <feeTypes.h>
#include <feePublicKey.h>
#else
#include <security_cryptkit/ckconfig.h>
#include <security_cryptkit/feeTypes.h>
#include <security_cryptkit/feePublicKey.h>
#endif

#if	CRYPTKIT_ASYMMETRIC_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Opaque object handle.
 */
typedef void *feeFEEDExp;

/*
 * Alloc and init a feeFEEDExp object associated with specified feePubKey.
 */
feeFEEDExp feeFEEDExpNewWithPubKey(
	feePubKey pubKey,
	feeRandFcn randFcn,		// optional 
	void *randRef);

void feeFEEDExpFree(feeFEEDExp feed);

/*
 * Plaintext block size.
 */
unsigned feeFEEDExpPlainBlockSize(feeFEEDExp feed);

/*
 * Ciphertext block size used for decryption.
 */
unsigned feeFEEDExpCipherBlockSize(feeFEEDExp feed);

/*
 * Required size of buffer for ciphertext, upon encrypting one
 * block of plaintext.
 */
unsigned feeFEEDExpCipherBufSize(feeFEEDExp feed);

/*
 * Return the size of ciphertext to hold specified size of encrypted plaintext.
 */
unsigned feeFEEDExpCipherTextSize(feeFEEDExp feed, unsigned plainTextSize);

/*
 * Return the size of plaintext to hold specified size of decrypted ciphertext.
 */
unsigned feeFEEDExpPlainTextSize(feeFEEDExp feed, unsigned cipherTextSize);

/*
 * Encrypt a block or less of data. Caller malloc's cipherText. Generates
 * feeFEEDExpCipherBlockSize() bytes of cipherText if finalBlock is false;
 * if finalBlock is true it could produce twice as much ciphertext. 
 * If plainTextLen is less than feeFEEDExpPlainBlockSize(), finalBlock must be true.
 */
feeReturn feeFEEDExpEncryptBlock(feeFEEDExp feed,
	const unsigned char *plainText,
	unsigned plainTextLen,
	unsigned char *cipherText,
	unsigned *cipherTextLen,		// RETURNED
	int finalBlock);

/*
 * Decrypt (exactly) a block of data. Caller malloc's plainText. Always
 * generates feeFEEDExpBlockSize bytes of plainText, unless 'finalBlock' is
 * non-zero (in which case feeFEEDExpBlockSize or less bytes of plainText are
 * generated).
 */
feeReturn feeFEEDExpDecryptBlock(feeFEEDExp feed,
	const unsigned char *cipherText,
	unsigned cipherTextLen,
	unsigned char *plainText,
	unsigned *plainTextLen,			// RETURNED
	int finalBlock);

/*
 * Convenience routines to encrypt & decrypt multi-block data.
 */
feeReturn feeFEEDExpEncrypt(feeFEEDExp feed,
	const unsigned char *plainText,
	unsigned plainTextLen,
	unsigned char **cipherText,		// malloc'd and RETURNED
	unsigned *cipherTextLen);		// RETURNED

feeReturn feeFEEDExpDecrypt(feeFEEDExp feed,
	const unsigned char *cipherText,
	unsigned cipherTextLen,
	unsigned char **plainText,		// malloc'd and RETURNED
	unsigned *plainTextLen);		// RETURNED

#ifdef __cplusplus
}
#endif

#endif	/* CRYPTKIT_ASYMMETRIC_ENABLE */

#endif	/*_CK_FEEFEEDEXP_H_*/
