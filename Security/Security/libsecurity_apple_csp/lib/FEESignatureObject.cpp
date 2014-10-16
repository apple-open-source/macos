/*
 * Copyright (c) 2000-2001,2011-2012,2014 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


/*
 * FEESignatureObject.cpp - implementations of FEE-style raw sign/verify classes
 *
 */

#ifdef	CRYPTKIT_CSP_ENABLE

#include "FEESignatureObject.h"
#include <security_cryptkit/feePublicKey.h>
#include <security_cryptkit/feeDigitalSignature.h>
#include <security_cryptkit/falloc.h>
#include <stdexcept>
#include <assert.h>
#include <security_utilities/debugging.h>

#define feeSigObjDebug(args...)		secdebug("feeSig", ##args)

CryptKit::FEESigner::~FEESigner()
{
	if(mWeMallocdFeeKey) {
		assert(mFeeKey != NULL);
		feePubKeyFree(mFeeKey);
	}
}

/* 
 * obtain key from context, validate, convert to native FEE key
 */
void CryptKit::FEESigner::keyFromContext(
	const Context 	&context)
{
	if(initFlag() && (mFeeKey != NULL)) {
		/* reusing context, OK */
		return;
	}
	
	CSSM_KEYCLASS 	keyClass;
	CSSM_KEYUSE		keyUse;
	if(isSigning()) {
		/* signing with private key */
		keyClass = CSSM_KEYCLASS_PRIVATE_KEY;
		keyUse   = CSSM_KEYUSE_SIGN;
	}
	else {
		/* verifying with public key */
		keyClass = CSSM_KEYCLASS_PUBLIC_KEY;
		keyUse   = CSSM_KEYUSE_VERIFY;
	}
	if(mFeeKey == NULL) {
		mFeeKey = contextToFeeKey(context,
			mSession,
			CSSM_ATTRIBUTE_KEY,
			keyClass,
			keyUse,
			mWeMallocdFeeKey);
	}
}

/* reusable init */
void CryptKit::FEESigner::signerInit(
	const Context 	&context,
	bool			isSigning)
{
	setIsSigning(isSigning);
	keyFromContext(context);
	setInitFlag(true);
}

/*
 * Note that, unlike the implementation in security_cryptkit/feePublicKey.c, we ignore
 * the Pm which used to be used as salt for the digest. That made staged verification
 * impossible and I do not believe it increased security. 
 */
void CryptKit::FEERawSigner::sign(
	const void	 	*data, 
	size_t 			dataLen,
	void			*sig,	
	size_t			*sigLen)	/* IN/OUT */
{
	feeSig 			fsig;
	feeReturn		frtn;
	unsigned char	*feeSig;
	unsigned		feeSigLen=0;
	
	if(mFeeKey == NULL) {
		throwCryptKit(FR_BadPubKey, "FEERawSigner::sign (no key)");
	}
	fsig = feeSigNewWithKey(mFeeKey, mRandFcn, mRandRef);
	if(fsig == NULL) {
		throwCryptKit(FR_BadPubKey, "FEERawSigner::sign");
	}
	frtn = feeSigSign(fsig,
		(unsigned char *)data,
		(unsigned)dataLen,
		mFeeKey);
	if(frtn == FR_Success) {
		frtn = feeSigData(fsig, &feeSig, &feeSigLen);
	}
	feeSigFree(fsig);
	if(frtn) {
		throwCryptKit(frtn, "FEERawSigner::sign");
	}
	
	/* copy out to caller and ffree */
	if(*sigLen < feeSigLen) {
		feeSigObjDebug("FEERawSigner sign overflow\n");
		ffree(feeSig);
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	memmove(sig, feeSig, feeSigLen);
	*sigLen = feeSigLen;
	ffree(feeSig);
}

void CryptKit::FEERawSigner::verify(
	const void	 	*data, 
	size_t 			dataLen,
	const void		*sig,			
	size_t			sigLen)
{
	feeSig 		fsig;
	feeReturn	frtn;
	
	if(mFeeKey == NULL) {
		throwCryptKit(FR_BadPubKey, "FEERawSigner::verify (no key)");
	}
	frtn = feeSigParse((unsigned char *)sig, sigLen, &fsig);
	if(frtn) {
		throwCryptKit(frtn, "feeSigParse");
	}
	frtn = feeSigVerify(fsig,
		(unsigned char *)data,
		(unsigned int)dataLen,
		mFeeKey);
	feeSigFree(fsig);
	if(frtn) {
		throwCryptKit(frtn, NULL);
	}
}

size_t CryptKit::FEERawSigner::maxSigSize()
{
	unsigned 	rtn;
	feeReturn 	frtn;
	
	frtn = feeSigSize(mFeeKey, &rtn);
	if(frtn) {
		throwCryptKit(frtn, "feeSigSize");
	}
	return rtn;
}

/* ECDSA - this is really easy. */

void CryptKit::FEEECDSASigner::sign(
	const void	 	*data, 
	size_t 			dataLen,
	void			*sig,	
	size_t			*sigLen)	/* IN/OUT */
{
	unsigned char	*feeSig;
	unsigned		feeSigLen;
	feeReturn		frtn;
	
	if(mFeeKey == NULL) {
		throwCryptKit(FR_BadPubKey, "FEERawSigner::sign (no key)");
	}
	frtn = feeECDSASign(mFeeKey,
		(unsigned char *)data,   // data to be signed
		(unsigned int)dataLen,				// in bytes
		mRandFcn, 
		mRandRef,
		&feeSig,
		&feeSigLen);			
	if(frtn) {
		throwCryptKit(frtn, "feeECDSASign");
	}
	/* copy out to caller and ffree */
	if(*sigLen < feeSigLen) {
		feeSigObjDebug("feeECDSASign overflow\n");
		ffree(feeSig);
		CssmError::throwMe(CSSMERR_CSP_OUTPUT_LENGTH_ERROR);
	}
	memmove(sig, feeSig, feeSigLen);
	*sigLen = feeSigLen;
	ffree(feeSig);

}

void CryptKit::FEEECDSASigner::verify(
	const void	*data, 
	size_t 		dataLen,
	const void	*sig,			
	size_t		sigLen)
{
	feeReturn	frtn;

	if(mFeeKey == NULL) {
		throwCryptKit(FR_BadPubKey, "FEERawSigner::verify (no key)");
	}
	frtn = feeECDSAVerify((unsigned char *)sig,
		sigLen,
		(unsigned char *)data,
		(unsigned int)dataLen,
		mFeeKey);
	if(frtn) {
		throwCryptKit(frtn, NULL);
	}
}

size_t CryptKit::FEEECDSASigner::maxSigSize()
{
	unsigned 	rtn;
	feeReturn 	frtn;
	
	frtn = feeECDSASigSize(mFeeKey, &rtn);
	if(frtn) {
		throwCryptKit(frtn, "feeECDSASigSize");
	}
	return rtn;
}

#endif	/* CRYPTKIT_CSP_ENABLE */
