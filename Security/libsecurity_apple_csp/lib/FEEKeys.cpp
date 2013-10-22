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
#include <security_cryptkit/feePublicKey.h>
#include <security_cryptkit/falloc.h>
#include <security_cdsa_utilities/cssmdata.h>
#include "AppleCSPSession.h"
#include "AppleCSPUtils.h"
#include <assert.h>
#include <security_utilities/debugging.h>

#define feeKeyDebug(args...)	secdebug("feeKey", ## args)

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
		Allocator			&allocator,
		CssmData			&blob,
		CSSM_KEYBLOB_FORMAT	&format,
		AppleCSPSession		&session,
		const CssmKey		*paramKey,	/* optional, unused here */
		CSSM_KEYATTR_FLAGS 	&attrFlags)	/* IN/OUT */
{
	unsigned char 	*keyBlob;
	unsigned 		len;
	feeReturn		frtn = FR_Internal;
	bool			freeTheKey = false;
	feePubKey		keyToEncode = mFeeKey;
	
	assert(mFeeKey != NULL);
	if((format == CSSM_KEYBLOB_RAW_FORMAT_DIGEST) && 
	   (mKeyHeader.KeyClass == CSSM_KEYCLASS_PRIVATE_KEY)) {
		/* key digest calculation; special case for private keys: cook
		 * up the associated public key and encode that */
		keyToEncode = feePubKeyAlloc();
		if(keyToEncode == NULL) {
			CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
		}
		frtn = feePubKeyInitPubKeyFromPriv(mFeeKey, keyToEncode);
		if(frtn) {
			feePubKeyFree(keyToEncode);
			throwCryptKit(frtn, "feePubKeyInitPubKeyFromPriv");
		}
		freeTheKey = true;
	}

	bool badFormat = false;
	int isPrivate = feePubKeyIsPrivate(keyToEncode);
	
	switch(mKeyHeader.AlgorithmId) {
		case CSSM_ALGID_FEE:
			if(isPrivate) {
				/* FEE private key */
				switch(format) {
					case CSSM_KEYBLOB_RAW_FORMAT_DIGEST:
						format = CSSM_KEYBLOB_RAW_FORMAT_NONE;
						/* and drop thru */
					case CSSM_KEYBLOB_RAW_FORMAT_NONE:
						frtn = feePubKeyCreateDERPrivBlob(keyToEncode, &keyBlob, &len);
						break;
					case CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING:
						frtn = feePubKeyCreatePrivBlob(keyToEncode, &keyBlob, &len);
						break;
					default:
						badFormat = true;
						break;
				}
			}
			else {
				/* FEE Public key */
				switch(format) {
					case CSSM_KEYBLOB_RAW_FORMAT_DIGEST:
						format = CSSM_KEYBLOB_RAW_FORMAT_NONE;
						/* and drop thru */
					case CSSM_KEYBLOB_RAW_FORMAT_NONE:
						frtn = feePubKeyCreateDERPubBlob(keyToEncode, &keyBlob, &len);
						break;
					case CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING:
						frtn = feePubKeyCreatePubBlob(keyToEncode, &keyBlob, &len);
						break;
					default:
						badFormat = true;
						break;
				}
			}
			/* end of base ALGID_FEE */
			break;
			
		case CSSM_ALGID_ECDSA:
			if(isPrivate) {
				/* ECDSA/ECDH private key */
				switch(format) {
					case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:
						/* ECDSA private key: PKCS8 */
						frtn = feePubKeyCreatePKCS8Blob(keyToEncode, &keyBlob, &len);
						break;
					case CSSM_KEYBLOB_RAW_FORMAT_NONE:
						/* set to default format, drop thru */
						format = CSSM_KEYBLOB_RAW_FORMAT_OPENSSL;
					case CSSM_KEYBLOB_RAW_FORMAT_OPENSSL:
						/* ECDSA private key, SEC1/OpenSSL format */
						frtn = feePubKeyCreateOpenSSLBlob(keyToEncode, &keyBlob, &len);
						break;
					case CSSM_KEYBLOB_RAW_FORMAT_DIGEST:
						format = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
						/* and drop thru */
					case CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING:
						/* raw private key bytes */
						frtn = feeCreateECDSAPrivBlob(keyToEncode, &keyBlob, &len);
						break;
					default:
						badFormat = true;
						break;
				}
			}
			else {
				/* 
				 * ECDSA public key.
				 * Note there is no OpenSSL case here, that format is only generated for 
				 * private keys.  
				 */
				switch(format) {
					case CSSM_KEYBLOB_RAW_FORMAT_NONE:
						/* set to default format, drop thru */
						format = CSSM_KEYBLOB_RAW_FORMAT_X509;
					case CSSM_KEYBLOB_RAW_FORMAT_X509:
						/* ECDSA, public key, default: X509 */
						frtn = feePubKeyCreateX509Blob(keyToEncode, &keyBlob, &len);
						break;
					case CSSM_KEYBLOB_RAW_FORMAT_DIGEST:
						format = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
						/* and drop thru */
					case CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING:
						/* raw x|y string */
						frtn = feeCreateECDSAPubBlob(keyToEncode, &keyBlob, &len);
						break;
					default:
						badFormat = true;
						break;
				}
			}
			/* end of case CSSM_ALGID_ECDSA */
			break;
		default:
			/* not reached */
			break;
	}
	
	if(badFormat) {
		CssmError::throwMe(isPrivate ?
			CSSMERR_CSP_INVALID_ATTR_PRIVATE_KEY_FORMAT :
			CSSMERR_CSP_INVALID_ATTR_PUBLIC_KEY_FORMAT);
	}
	if(frtn) {
		throwCryptKit(frtn, "feePubKeyCreate*Blob");
	}
	setUpCssmData(blob, len, allocator);
	memmove(blob.data(), keyBlob, len);
	blob.length(len);
	ffree(keyBlob);
	if(freeTheKey) {
		/* free the temp pub key we created here */
		feePubKeyFree(keyToEncode);
	}
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
	
	CSSM_ALGORITHMS algId = context.algorithm();
	
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
	switch(algId) {
		case CSSM_ALGID_ECDSA:
			/* no options */
			curveType = FCT_ANSI;
			break;
		default:
		{
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
				case CSSM_FEE_CURVE_TYPE_ANSI_X9_62:
					curveType = FCT_ANSI;
					break;
				default:
					/* FIXME - maybe we should be more specific */
					CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_ALG_PARAMS);
			}
			break;
		}
	}
	feeReturn frtn = feePubKeyInitFromPrivDataKeyBits( 
		fPrivBinKey.feeKey(),
		(unsigned char *)seed->data(),
		(unsigned int)seed->length(),
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
	const CssmKey 	&cssmKey,
	AppleCSPSession	&session) :
		CSPKeyInfoProvider(cssmKey, session)
{
}
CSPKeyInfoProvider *FEEKeyInfoProvider::provider(
	const CssmKey 	&cssmKey,
	AppleCSPSession	&session)
{
	switch(cssmKey.algorithm()) {
		case CSSM_ALGID_FEE:
		case CSSM_ALGID_ECDSA:
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
	return new FEEKeyInfoProvider(cssmKey, session);
}

/* Given a raw key, cook up a Binary key */
void CryptKit::FEEKeyInfoProvider::CssmKeyToBinary(
	CssmKey				*paramKey,		// optional, ignored
	CSSM_KEYATTR_FLAGS	&attrFlags,		// IN/OUT
	BinaryKey 			**binKey)
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

/* 
 * Obtain blob suitable for hashing in CSSM_APPLECSP_KEYDIGEST 
 * passthrough.
 */
bool CryptKit::FEEKeyInfoProvider::getHashableBlob(
	Allocator 	&allocator,
	CssmData		&blob)			// blob to hash goes here
{
	/*
	 * The optimized case, a raw key in the "proper" format already.
	 * Currently this is:
	 * FEE public key in default/NONE form (which happens to be DER)
	 */
	assert(mKey.blobType() == CSSM_KEYBLOB_RAW);
	if((mKey.algorithm() == CSSM_ALGID_FEE) && 
	   (mKey.blobFormat() == CSSM_KEYBLOB_RAW_FORMAT_NONE) &&
	   (mKey.keyClass() == CSSM_KEYCLASS_PUBLIC_KEY)) {
		const CssmData &keyBlob = CssmData::overlay(mKey.KeyData);
		copyCssmData(keyBlob, blob, allocator);
		return true;
	}
	
	/* caller converts to binary and proceeds */
	return false;
}

#endif	/* CRYPTKIT_CSP_ENABLE */
