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
 * FeeDES.h - generic, portable DES encryption object
 *
 * Revision History
 * ----------------
 * 26 Aug 96 at NeXT
 *	Created.
 */

#ifndef	_CK_FEEDES_H_
#define _CK_FEEDES_H_

#if	!defined(__MACH__)
#include <ckconfig.h>
#include <feeTypes.h>
#else
#include <security_cryptkit/ckconfig.h>
#include <security_cryptkit/feeTypes.h>
#endif

#if	CRYPTKIT_SYMMETRIC_ENABLE

#ifdef __cplusplus
extern "C" {
#endif

#define FEE_DES_MIN_STATE_SIZE	8

/*
 * Opaque object handle.
 */
typedef void *feeDES;

/*
 * Alloc and init a feeDES object with specified initial state.
 * State must be at least 8 bytes; only 8 bytes are used, ignoring
 * MSB of each bytes.
 */
feeDES feeDESNewWithState(const unsigned char *state,
	unsigned stateLen);

void feeDESFree(feeDES des);

/*
 * Set new initial state.
 */
feeReturn feeDESSetState(feeDES des,
	const unsigned char *state,
	unsigned stateLen);

/*
 * Set block or chain (CBC) mode. CBC is default.
 */
void feeDESSetBlockMode(feeDES des);
void feeDESSetChainMode(feeDES des);

/*
 * Plaintext block size.
 */
unsigned feeDESPlainBlockSize(feeDES des);

/*
 * Ciphertext block size used for decryption.
 */
unsigned feeDESCipherBlockSize(feeDES des);

/*
 * Required size of buffer for ciphertext, upon encrypting one
 * block of plaintext.
 */
unsigned feeDESCipherBufSize(feeDES des);

/*

 * Return the size of ciphertext to hold specified size of plaintext.

 */

unsigned feeDESCipherTextSize(feeDES des, unsigned plainTextSize);


/*
 * Key size in bits.
 */
unsigned feeDESKeySize(feeDES des);

/*
 * Encrypt a block or less of data. Caller malloc's cipherText. Generates
 * up to (2 * feeDESBlockSize) bytes of cipherText. If plainTextLen is
 * less than feeDESBlockSize, finalBlock must be true.
 */
feeReturn feeDESEncryptBlock(feeDES des,
	const unsigned char *plainText,
	unsigned plainTextLen,
	unsigned char *cipherText,
	unsigned *cipherTextLen,		// RETURNED
	int finalBlock);

/*
 * Decrypt (exactly) a block of data. Caller malloc's plainText. Always
 * generates feeDESBlockSize bytes of plainText, unless 'finalBlock' is
 * non-zero (in which case feeDESBlockSize or less bytes of plainText are
 * generated).
 */
feeReturn feeDESDecryptBlock(feeDES des,
	const unsigned char *cipherText,
	unsigned cipherTextLen,
	unsigned char *plainText,
	unsigned *plainTextLen,			// RETURNED
	int finalBlock);

/*
 * Convenience routines to encrypt & decrypt multi-block data.
 */
feeReturn feeDESEncrypt(feeDES des,
	const unsigned char *plainText,
	unsigned plainTextLen,
	unsigned char **cipherText,		// malloc'd and RETURNED
	unsigned *cipherTextLen);		// RETURNED

feeReturn feeDESDecrypt(feeDES des,
	const unsigned char *cipherText,
	unsigned cipherTextLen,
	unsigned char **plainText,		// malloc'd and RETURNED
	unsigned *plainTextLen);		// RETURNED

#ifdef __cplusplus
}
#endif

#endif	/* CRYPTKIT_SYMMETRIC_ENABLE */
#endif	/*_CK_FEEDES_H_*/
