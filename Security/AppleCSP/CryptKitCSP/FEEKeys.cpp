/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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
 * FEEKeys.cpp - FEE-related asymmetric key pair classes. 
 *
 * Created 2/21/2001 by dmitch.
 */

#ifdef	CRYPTKIT_CSP_ENABLE

#include "FEEKeys.h"
#include "FEECSPUtils.h"
#include "CryptKitSpace.h"
#include <CryptKit/feePublicKey.h>
#include <CryptKit/falloc.h>
#include <Security/cssmdata.h>
#include "AppleCSPSession.h"
#include "AppleCSPUtils.h"
#include <assert.h>
#include <Security/debugging.h>

#define feeKeyDebug(args...)	debug("feeKey", ## args)

/***
 *** FEE-style BinaryKey
 ***/
 
/* constructor with optional existing feePubKey */
CryptKit::FEEBinaryKey::FEEBinaryKey(feePubKey feeKey)
	: mFeeKey(feeKey)
{
	if(mFeeKey == NULL) {
		mFeeKey = feePubKeyAlloc();
		if(mFeeKey == NULL) {
			CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
		}
	}
}

CryptKit::FEEBinaryKey::~FEEBinaryKey()
{
	if(mFeeKey) {
		feePubKeyFree(mFeeKey);
		mFeeKey = NULL;
	}
}

void CryptKit::FEEBinaryKey::generateKeyBlob(
		CssmAllocator 		&allocator,
		CssmData			&blob,
		CSSM_KEYBLOB_FORMAT	&format)
{
	unsigned char 	*keyBlob;
	unsigned 		len;
	feeReturn		frtn;
	bool			derBlob;
	
	assert(mFeeKey != NULL);
	switch(format) {
		/* also case FEE_KEYBLOB_DEFAULT_FORMAT: */
		case CSSM_KEYBLOB_RAW_FORMAT_NONE:
			derBlob = true;
			break;
		case CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING:
			/* native non-DER-encoded blob */
			derBlob = false;
			break;
		default:
			feeKeyDebug("FEEBinaryKey::generateKeyBlob: bad format (%ld)\n", format);
			CssmError::throwMe(feePubKeyIsPrivate(mFeeKey) ?
				CSSMERR_CSP_INVALID_ATTR_PRIVATE_KEY_FORMAT :
				CSSMERR_CSP_INVALID_ATTR_PUBLIC_KEY_FORMAT);
	}
	if(feePubKeyIsPrivate(mFeeKey)) {
		if(derBlob) {
			frtn = feePubKeyCreateDERPrivBlob(mFeeKey, &keyBlob, &len);
		}
		else {
			frtn = feePubKeyCreatePrivBlob(mFeeKey, &keyBlob, &len);
		}
	}
	else {
		if(derBlob) {
			frtn = feePubKeyCreateDERPubBlob(mFeeKey, &keyBlob, &len);
		}
		else {
			frtn = feePubKeyCreatePubBlob(mFeeKey, &keyBlob, &len);
		}
	}
	if(frtn) {
		throwCryptKit(frtn, "feePubKeyCreate*Blob");
	}
	setUpCssmData(blob, len, allocator);
	memmove(blob.data(), keyBlob, len);
	blob.length(len);
	ffree(keyBlob);
	format = derBlob ? FEE_KEYBLOB_DEFAULT_FORMAT : 
		CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
}
		
/***
 *** FEE-style AppleKeyPairGenContext
 ***/

/*
 * This one is specified in, and called from, CSPFullPluginSession. Our
 * only job is to prepare two subclass-specific BinaryKeys and call up to
 * AppleKeyPairGenContext.
 */
void CryptKit::FEEKeyPairGenContext::generate(
	const Context 	&context, 
	CssmKey 		&pubKey, 
	CssmKey 		&privKey)
{
	FEEBinaryKey *pubBinKey  = new FEEBinaryKey();
	FEEBinaryKey *privBinKey = new FEEBinaryKey();
	
	try {
		AppleKeyPairGenContext::generate(context, 
			session(),
			pubKey, 
			pubBinKey, 
			privKey, 
			privBinKey);
	}
	catch (...) {
		delete pubBinKey;
		delete privBinKey;
		throw;
	}

}
	
// this one is specified in, and called from, AppleKeyPairGenContext
void CryptKit::FEEKeyPairGenContext::generate(
	const Context 	&context,
	BinaryKey		&pubBinKey,	
	BinaryKey		&privBinKey,
	uint32			&keyBits)
{
	/* 
	 * These casts throw exceptions if the keys are of the 
	 * wrong classes, which would be a major bogon, since we created
	 * the keys in the above generate() function.
	 */
	FEEBinaryKey &fPubBinKey = 
		dynamic_cast<FEEBinaryKey &>(pubBinKey);
	FEEBinaryKey &fPrivBinKey = 
		dynamic_cast<FEEBinaryKey &>(privBinKey);

	/*
	 * Two parameters from context. Key size in bits is required;
	 * seed is optional. If not present, we cook up random private data. 
	 */
	keyBits = context.getInt(CSSM_ATTRIBUTE_KEY_LENGTH,
				CSSMERR_CSP_MISSING_ATTR_KEY_LENGTH);
	CssmCryptoData *cseed = context.get<CssmCryptoData>(CSSM_ATTRIBUTE_SEED);
	CssmData *seed;
	bool haveSeed;
	CssmAutoData aSeed(session());		// malloc on demand
	if(cseed) {
		/* caller specified seed */
		haveSeed = true;
		seed = &cseed->param();
	}
	else {
		/* generate random seed */
		haveSeed = false;
		unsigned keyBytes = ((keyBits + 7) / 8) + 1;
		aSeed.malloc(keyBytes);
		session().getRandomBytes(keyBytes, aSeed);
		seed = &aSeed.get();
	}
	
	/* Curve and prime types - optional */
	feePrimeType primeType = FPT_Default;
	uint32 uPrimeType = context.getInt(CSSM_ATTRIBUTE_FEE_PRIME_TYPE);
	switch(uPrimeType) {
		case CSSM_FEE_PRIME_TYPE_DEFAULT:
			break;
		case CSSM_FEE_PRIME_TYPE_MERSENNE:
			primeType = FPT_Mersenne;
			break;
		case CSSM_FEE_PRIME_TYPE_FEE:
			primeType = FPT_FEE;
			break;
		case CSSM_FEE_PRIME_TYPE_GENERAL:
			primeType = FPT_General;
			break;
		default:
			/* FIXME - maybe we should be more specific */
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_ALG_PARAMS);
	}
	feeCurveType curveType = FCT_Default;
	uint32 uCurveType = context.getInt(CSSM_ATTRIBUTE_FEE_CURVE_TYPE);
	switch(uCurveType) {
		case CSSM_FEE_CURVE_TYPE_DEFAULT:
			break;
		case CSSM_FEE_CURVE_TYPE_MONTGOMERY:
			curveType = FCT_Montgomery;
			break;
		case CSSM_FEE_CURVE_TYPE_WEIERSTRASS:
			curveType = FCT_Weierstrass;
			break;
		default:
			/* FIXME - maybe we should be more specific */
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_ALG_PARAMS);
	}
	feeReturn frtn = feePubKeyInitFromPrivDataKeyBits( 
		fPrivBinKey.feeKey(),
		(unsigned char *)seed->data(),
		seed->length(),
		keyBits,
		primeType,
		curveType,
		/* 
		 * our random seed: trust it
		 * caller's seed: hash it
		 */
		haveSeed ? 1 : 0);
	if(frtn) {
		throwCryptKit(frtn, "feePubKeyInitFromPrivDataKeyBits");
	}
	frtn = feePubKeyInitPubKeyFromPriv(fPrivBinKey.feeKey(), 
		fPubBinKey.feeKey());
	if(frtn) {
		throwCryptKit(frtn, "feePubKeyInitPubKeyFromPriv");
	}
}


/***
 *** FEE-style CSPKeyInfoProvider.
 ***/
CryptKit::FEEKeyInfoProvider::FEEKeyInfoProvider(
	const CssmKey &cssmKey) :
		CSPKeyInfoProvider(cssmKey)
{
}
CSPKeyInfoProvider *FEEKeyInfoProvider::provider(
		const CssmKey &cssmKey)
{
	switch(cssmKey.algorithm()) {
		case CSSM_ALGID_FEE:
			break;
		default:
			return NULL;
	}
	switch(cssmKey.keyClass()) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
		case CSSM_KEYCLASS_PRIVATE_KEY:
			/* FIXME - verify proper CSSM_KEYBLOB_RAW_FORMAT_xx */
			break;
		default:
			return NULL;
	}
	/* OK, we'll handle this one */
	return new FEEKeyInfoProvider(cssmKey);
}

/* Given a raw key, cook up a Binary key */
void CryptKit::FEEKeyInfoProvider::CssmKeyToBinary(
	BinaryKey **binKey)
{
	*binKey = NULL;
	feePubKey feeKey = NULL;
	
	/* first cook up a feePubKey, then drop that into a BinaryKey */
	feeKey = rawCssmKeyToFee(mKey);
	FEEBinaryKey *feeBinKey = new FEEBinaryKey(feeKey);
	*binKey = feeBinKey;
}
		
/* 
 * Obtain key size in bits.
 * Currently only raw public keys are dealt with (they're the ones
 * which come from certs, the only current use for this function).
 * Note that if we need to handle ref keys, we'll need a session ref...
 */
void CryptKit::FEEKeyInfoProvider::QueryKeySizeInBits(
	CSSM_KEY_SIZE &keySize)
{
	feePubKey feeKey = NULL;
	
	if(mKey.blobType() != CSSM_KEYBLOB_RAW) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
	}
	feeKey = rawCssmKeyToFee(mKey);
	keySize.LogicalKeySizeInBits = feePubKeyBitsize(feeKey);
	keySize.EffectiveKeySizeInBits = keySize.LogicalKeySizeInBits;
	feePubKeyFree(feeKey);
}

#endif	/* CRYPTKIT_CSP_ENABLE */
