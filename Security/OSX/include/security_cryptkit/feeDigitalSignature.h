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
 * feeDigitalSignature.h - generic, portable FEE Digital Signature object
 *
 * Revision History
 * ----------------
 * 22 Aug 96 at NeXT
 *	Created.
 */

#ifndef	_CK_FEEDIGITALSIG_H_
#define _CK_FEEDIGITALSIG_H_

#if	!defined(__MACH__)
#include <feeTypes.h>
#include <feePublicKey.h>
#else
#include <security_cryptkit/feeTypes.h>
#include <security_cryptkit/feePublicKey.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define FEE_SIG_MAGIC		0xfee00516

/*
 * Opaque signature handle.
 */
typedef void *feeSig;

/*
 * Create new feeSig object, including a random large integer 'Pm' for
 * possible use in salting a feeHash object.
 */
feeSig feeSigNewWithKey(
	feePubKey 		pubKey,
	feeRandFcn		randFcn,		/* optional */
	void			*randRef);		/* optional */

void feeSigFree(
	feeSig 			sig);

/*
 * Obtain a malloc'd Pm after or feeSigNewWithKey() feeSigParse()
 */
unsigned char *feeSigPm(
	feeSig 			sig,
	unsigned 		*PmLen);		/* RETURNED */

/*
 * Sign specified block of data (most likely a hash result) using
 * specified feePubKey.
 */
feeReturn feeSigSign(
	feeSig 			sig,
	const unsigned char	*data,   	// data to be signed
	unsigned 		dataLen,	// in bytes
	feePubKey 		pubKey);

/*
 * Given a feeSig processed by feeSigSign, obtain a malloc'd byte
 * array representing the signature.
 */
feeReturn feeSigData(
	feeSig 			sig,
	unsigned char 		**sigData,	// malloc'd and RETURNED
	unsigned 		*sigDataLen);	// RETURNED

/*
 * Obtain a feeSig object by parsing an existing signature block.
 * Note that if Pm is used to salt a hash of the signed data, this must
 * be performed prior to hashing.
 */
feeReturn feeSigParse(
	const unsigned char	*sigData,
	size_t			sigDataLen,
	feeSig 			*sig);		// RETURNED

/*
 * Verify signature, obtained via feeSigParse, for specified
 * data (most likely a hash result) and feePubKey. Returns FR_Success or
 * FR_InvalidSignature.
 */
feeReturn feeSigVerify(
	feeSig 			sig,
	const unsigned char	*data,
	unsigned 		dataLen,
	feePubKey 		pubKey);

/*
 * For given key, calculate maximum signature size. 
 */
feeReturn feeSigSize(
	feePubKey		pubKey,
	unsigned 		*maxSigLen);

#ifdef __cplusplus
}
#endif

#endif	/*_CK_FEEDIGITALSIG_H_*/
