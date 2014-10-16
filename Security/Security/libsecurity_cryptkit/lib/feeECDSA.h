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
 * feeECDSA.h - Elliptic Curve Digital Signature Algorithm (per IEEE 1363)
 *
 * Revision History
 * ----------------
 * 16 Jul 97 at Apple
 *	Created.
 */

#ifndef	_CK_FEEECDSA_H_
#define _CK_FEEECDSA_H_

#if	!defined(__MACH__)
#include <ckconfig.h>
#include <feeTypes.h>
#include <feePublicKey.h>
#else
#include <security_cryptkit/ckconfig.h>
#include <security_cryptkit/feeTypes.h>
#include <security_cryptkit/feePublicKey.h>
#endif

/* 
 * Keep this one defined and visible even if we can't actually do ECDSA - feeSigParse()
 * uses it to detect "wriong signature type".
 */
#define FEE_ECDSA_MAGIC		0xfee00517

#if	CRYPTKIT_ECDSA_ENABLE

#ifdef __cplusplus
extern "C" {
#endif


/*
 * Sign specified block of data (most likely a hash result) using
 * specified private key. Result, an enc64-encoded signature block,
 * is returned in *sigData.
 */
feeReturn feeECDSASign(feePubKey pubKey,
	const unsigned char *data,   	// data to be signed
	unsigned dataLen,				// in bytes
	feeRandFcn randFcn,				// optional
	void *randRef,					// optional 
	unsigned char **sigData,		// malloc'd and RETURNED
	unsigned *sigDataLen);			// RETURNED

/*
 * Verify signature, obtained via feeECDSASign, for specified
 * data (most likely a hash result) and feePubKey. Returns FR_Success or
 * FR_InvalidSignature.
 */
feeReturn feeECDSAVerify(const unsigned char *sigData,
	size_t sigDataLen,
	const unsigned char *data,
	unsigned dataLen,
	feePubKey pubKey);

/*
 * For given key, calculate maximum signature size. 
 */
feeReturn feeECDSASigSize(
	feePubKey pubKey,
	unsigned *maxSigLen);

#ifdef __cplusplus
}
#endif

#endif	/* CRYPTKIT_ECDSA_ENABLE */

#endif	/*_CK_FEEECDSA_H_*/
