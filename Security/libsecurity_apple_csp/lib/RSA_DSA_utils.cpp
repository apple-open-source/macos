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
 * RSA_DSA_utils.cpp
 */

#include "RSA_DSA_utils.h"
#include "RSA_DSA_keys.h"
#include <opensslUtils/opensslAsn1.h>
#include <opensslUtils/opensslUtils.h>
#include <security_utilities/logging.h>
#include <security_utilities/debugging.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/err.h>
#include <security_utilities/simpleprefs.h>
#include <security_utilities/threading.h>
#include <security_utilities/globalizer.h>
#include <CoreFoundation/CFNumber.h>

#define rsaMiscDebug(args...)	secdebug("rsaMisc", ## args)

/*
 * Obtain and cache max key sizes. System preferences only consulted 
 * at most once per process.
 */

/* 
 * Do dictionary lookup, convert possible CFNumber to uint32.
 * Does not alter val if valid number is not found.
 */
static void rsaLookupVal(
	Dictionary &prefs,
	CFStringRef key,
	uint32 &val)
{
	CFNumberRef cfVal = (CFNumberRef)prefs.getValue(key);
	if(cfVal == NULL) {
		return;
	}
	if(CFGetTypeID(cfVal) != CFNumberGetTypeID()) {
		return;
	}
	
	/* ensure the number is positive, not relying on gcc 64-bit arithmetic */
	SInt32 s32 = 0;
	CFNumberRef cfLimit = CFNumberCreate(NULL, kCFNumberSInt32Type, &s32); 
	CFComparisonResult result = CFNumberCompare(cfVal, cfLimit, NULL);
	CFRelease(cfLimit);
	if(result == kCFCompareLessThan) {
		/* negative value in preference */
		return;
	}
	
	/* ensure the number fits in 31 bits (the useful size of a SInt32 for us) */
	s32 = 0x7fffffff;
	cfLimit = CFNumberCreate(NULL, kCFNumberSInt32Type, &s32); 
	result = CFNumberCompare(cfVal, cfLimit, NULL);
	CFRelease(cfLimit);
	if(result == kCFCompareGreaterThan) {
		/* too large; discard it */
		return;
	}
	SInt64 s64;
	if(!CFNumberGetValue(cfVal, kCFNumberSInt64Type, &s64)) {
		/* impossible, right? We already range checked */
		return;
	}
	val = (uint32)s64;
}

struct RSAKeySizes {
	uint32 maxKeySize;
	uint32 maxPubExponentSize;
	RSAKeySizes();
};

/* one-time only prefs lookup */
RSAKeySizes::RSAKeySizes()
{
	/* set defaults, these might get overridden */
	maxKeySize = RSA_MAX_KEY_SIZE;
	maxPubExponentSize = RSA_MAX_PUB_EXPONENT_SIZE;
	
	/* now see if there are prefs set for either of these */
	Dictionary* d = Dictionary::CreateDictionary(kRSAKeySizePrefsDomain, Dictionary::US_System, true);
	if (!d)
	{
		return;
	}
	
	if (d->dict())
	{
		auto_ptr<Dictionary>apd(d);
		rsaLookupVal(*apd, kRSAMaxKeySizePref, maxKeySize);
		rsaLookupVal(*apd, kRSAMaxPublicExponentPref, maxPubExponentSize);
	}
	else
	{
		delete d;
	}
}

static ModuleNexus<RSAKeySizes> rsaKeySizes;

/* 
 * Public functions to obtain the currently configured max sizes of 
 * RSA key and public exponent.
 */
uint32 rsaMaxKeySize()
{
	return rsaKeySizes().maxKeySize;
}

uint32 rsaMaxPubExponentSize()
{
	return rsaKeySizes().maxPubExponentSize;
}

/* 
 * Given a Context:
 * -- obtain CSSM key (there must only be one)
 * -- validate keyClass
 * -- validate keyUsage
 * -- convert to RSA *, allocating the RSA key if necessary
 */
RSA *contextToRsaKey(
	const Context 		&context,
	AppleCSPSession	 	&session,
	CSSM_KEYCLASS		keyClass,	  // CSSM_KEYCLASS_{PUBLIC,PRIVATE}_KEY
	CSSM_KEYUSE			usage,		  // CSSM_KEYUSE_ENCRYPT, CSSM_KEYUSE_SIGN, etc.
	bool				&mallocdKey,  // RETURNED
	CSSM_DATA			&label)		  // mallocd and RETURNED for OAEP
{
    CssmKey &cssmKey = 
		context.get<CssmKey>(CSSM_ATTRIBUTE_KEY, CSSMERR_CSP_MISSING_ATTR_KEY);
	const CSSM_KEYHEADER &hdr = cssmKey.KeyHeader;
	if(hdr.AlgorithmId != CSSM_ALGID_RSA) {
		CssmError::throwMe(CSSMERR_CSP_ALGID_MISMATCH);
	}
	if(hdr.KeyClass != keyClass) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
	cspValidateIntendedKeyUsage(&hdr, usage);
	cspVerifyKeyTimes(hdr);
	return cssmKeyToRsa(cssmKey, session, mallocdKey, label);
}
/* 
 * Convert a CssmKey to an RSA * key. May result in the creation of a new
 * RSA (when cssmKey is a raw key); allocdKey is true in that case
 * in which case the caller generally has to free the allocd key).
 */
RSA *cssmKeyToRsa(
	const CssmKey	&cssmKey,
	AppleCSPSession	&session,
	bool			&allocdKey,		// RETURNED
	CSSM_DATA		&label)			// mallocd and RETURNED for OAEP
{
	RSA *rsaKey = NULL;
	allocdKey = false;
	
	const CSSM_KEYHEADER *hdr = &cssmKey.KeyHeader;
	if(hdr->AlgorithmId != CSSM_ALGID_RSA) {
		// someone else's key (should never happen)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	switch(hdr->BlobType) {
		case CSSM_KEYBLOB_RAW:
			rsaKey = rawCssmKeyToRsa(cssmKey, label);
			allocdKey = true;
			break;
		case CSSM_KEYBLOB_REFERENCE:
		{
			BinaryKey &binKey = session.lookupRefKey(cssmKey);
			RSABinaryKey *rsaBinKey = dynamic_cast<RSABinaryKey *>(&binKey);
			/* this cast failing means that this is some other
			 * kind of binary key */
			if(rsaBinKey == NULL) {
				rsaMiscDebug("cssmKeyToRsa: wrong BinaryKey subclass\n");
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
			}
			assert(rsaBinKey->mRsaKey != NULL);
			rsaKey = rsaBinKey->mRsaKey;
			break;
		}
		default:
			CssmError::throwMe(CSSMERR_CSP_KEY_BLOB_TYPE_INCORRECT);
	}
	return rsaKey;
}

/* 
 * Convert a raw CssmKey to a newly alloc'd RSA key.
 */
RSA *rawCssmKeyToRsa(
	const CssmKey	&cssmKey,
	CSSM_DATA		&label)			// mallocd and RETURNED for OAEP keys
{
	const CSSM_KEYHEADER *hdr = &cssmKey.KeyHeader;
	bool isPub;
	bool isOaep = false;
	
	assert(hdr->BlobType == CSSM_KEYBLOB_RAW); 
	
	switch(hdr->AlgorithmId) {
		case CSSM_ALGID_RSA:
			break;
		case CSSM_ALGMODE_PKCS1_EME_OAEP:
			isOaep = true;
			break;
		default:
			// someone else's key (should never happen)
			CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	
	/* validate and figure out what we're dealing with */
	switch(hdr->KeyClass) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			switch(hdr->Format) {
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS1:	
				case CSSM_KEYBLOB_RAW_FORMAT_X509:
				case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH:
				case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2:
					break;
				default:
					CssmError::throwMe(
						CSSMERR_CSP_INVALID_ATTR_PUBLIC_KEY_FORMAT);
			}
			if(isOaep && (hdr->Format != CSSM_KEYBLOB_RAW_FORMAT_X509)) {
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PUBLIC_KEY_FORMAT);
			}
			isPub = true;
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			switch(hdr->Format) {
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:	// default
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS1:	// openssl style
				case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH:
					break;
				default:
					CssmError::throwMe(
						CSSMERR_CSP_INVALID_ATTR_PRIVATE_KEY_FORMAT);
			}
			if(isOaep && (hdr->Format != CSSM_KEYBLOB_RAW_FORMAT_PKCS8)) {
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PRIVATE_KEY_FORMAT);
			}
			isPub = false;
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
	
	RSA *rsaKey = RSA_new();
	if(rsaKey == NULL) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
	}
	CSSM_RETURN crtn;
	if(isOaep) {
		if(isPub) {
			crtn = RSAOAEPPublicKeyDecode(rsaKey, 
				cssmKey.KeyData.Data, cssmKey.KeyData.Length,
				&label);
		}
		else {
			crtn = RSAOAEPPrivateKeyDecode(rsaKey, 
				cssmKey.KeyData.Data, cssmKey.KeyData.Length,
				&label);
		}
	}
	else {
		if(isPub) {
			crtn = RSAPublicKeyDecode(rsaKey, hdr->Format,
				cssmKey.KeyData.Data, cssmKey.KeyData.Length);
		}
		else {
			crtn = RSAPrivateKeyDecode(rsaKey, hdr->Format,
				cssmKey.KeyData.Data, cssmKey.KeyData.Length);
		}
	}
	if(crtn) {
        RSA_free(rsaKey);
		CssmError::throwMe(crtn);
	}
	
	/* enforce max key size and max public exponent size */
	bool badKey = false;
	uint32 keySize = RSA_size(rsaKey) * 8;
	if(keySize > rsaMaxKeySize()) {
		rsaMiscDebug("rawCssmKeyToRsa: key size exceeded");
		badKey = true;
	}
	else {
		keySize = BN_num_bytes(rsaKey->e) * 8;
		if(keySize > rsaMaxPubExponentSize()) {
			badKey = true;
			rsaMiscDebug("rawCssmKeyToRsa: pub exponent size exceeded"); 
		}
	}
	if(badKey) {
		RSA_free(rsaKey);
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY_LENGTH);
	}
	return rsaKey;
}

/*
 * Given a partially formed DSA public key (with no p, q, or g) and a 
 * CssmKey representing a supposedly fully-formed DSA key, populate
 * the public key's p, g, and q with values from the fully formed key.
 */
CSSM_RETURN dsaGetParamsFromKey(
	DSA 			*partialKey,
	const CssmKey	&paramKey,
	AppleCSPSession	&session)
{
	bool allocdKey;
	DSA *dsaParamKey = cssmKeyToDsa(paramKey, session, allocdKey);
	if(dsaParamKey == NULL) {
		errorLog0("dsaGetParamsFromKey: bad paramKey\n");
		return CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE;
	}
	CSSM_RETURN crtn = CSSM_OK;
	
	/* require fully formed other key of course... */
	if((dsaParamKey->p == NULL) ||
	   (dsaParamKey->q == NULL) ||
	   (dsaParamKey->g == NULL)) {
		errorLog0("dsaGetParamsFromKey: incomplete paramKey\n");
		crtn = CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE;
		goto abort;
	}
	rsaMiscDebug("dsaGetParamsFromKey: partialKey %p paramKey %p",
		partialKey, dsaParamKey);
		
	partialKey->q = BN_dup(dsaParamKey->q);
	partialKey->p = BN_dup(dsaParamKey->p);
	partialKey->g = BN_dup(dsaParamKey->g);
	
abort:
	if(allocdKey) {
		DSA_free(dsaParamKey);
	}
	return crtn;
}

/* 
 * Given a Context:
 * -- obtain CSSM key (there must only be one)
 * -- validate keyClass
 * -- validate keyUsage
 * -- convert to DSA *, allocating the DSA key if necessary
 */
DSA *contextToDsaKey(
	const Context 		&context,
	AppleCSPSession	 	&session,
	CSSM_KEYCLASS		keyClass,	  // CSSM_KEYCLASS_{PUBLIC,PRIVATE}_KEY
	CSSM_KEYUSE			usage,		  // CSSM_KEYUSE_ENCRYPT, CSSM_KEYUSE_SIGN, etc.
	bool				&mallocdKey)  // RETURNED
{
    CssmKey &cssmKey = 
		context.get<CssmKey>(CSSM_ATTRIBUTE_KEY, CSSMERR_CSP_MISSING_ATTR_KEY);
	const CSSM_KEYHEADER &hdr = cssmKey.KeyHeader;
	if(hdr.AlgorithmId != CSSM_ALGID_DSA) {
		CssmError::throwMe(CSSMERR_CSP_ALGID_MISMATCH);
	}
	if(hdr.KeyClass != keyClass) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
	cspValidateIntendedKeyUsage(&hdr, usage);
	cspVerifyKeyTimes(hdr);
	DSA *rtnDsa = cssmKeyToDsa(cssmKey, session, mallocdKey);
	if((keyClass == CSSM_KEYCLASS_PUBLIC_KEY) &&
			(rtnDsa->p == NULL)) {
		/*
		 * Special case: this specific key is only partially formed;
		 * it's missing the DSA parameters p, g, and q. To proceed with this
		 * key, the caller must pass in another fully formned DSA public key
		 * in raw form in the context. If it's there we use those parameters.
		 */
		rsaMiscDebug("contextToDsaKey; partial DSA key %p", rtnDsa);
		CssmKey *paramKey = context.get<CssmKey>(CSSM_ATTRIBUTE_PARAM_KEY);
		if(paramKey == NULL) {
			rsaMiscDebug("contextToDsaKey: missing DSA params, no pub key in "
				"context");
			if(mallocdKey) {
				DSA_free(rtnDsa);
				mallocdKey = false;
			}
			CssmError::throwMe(CSSMERR_CSP_APPLE_PUBLIC_KEY_INCOMPLETE);
		}
		
		/* 
		 * If this is a ref key, we have to cook up a new DSA key to 
		 * avoid modifying the existing key. If we started with a raw key,
		 * we can modify it directly since the underlying DSA key has
		 * a lifetime only as long as this context (and since the context
		 * contains the parameter-bearing key, the params are valid 
		 * as long as the DSA key).
		 */
		if(!mallocdKey) {
			DSA *existKey = rtnDsa;
			rtnDsa = DSA_new();
			if(rtnDsa == NULL) {
				CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);	
			}
			rtnDsa->pub_key = BN_dup(existKey->pub_key);
			rsaMiscDebug("contextToDsaKey; temp partial copy %p", rtnDsa);
			mallocdKey = true;
		}
		
		/*
		 * Add params from paramKey into rtnDsa
		 */
		CSSM_RETURN crtn = dsaGetParamsFromKey(rtnDsa, *paramKey, session);
		if(crtn) {
			if(mallocdKey) {
				DSA_free(rtnDsa);
				mallocdKey = false;
			}
			CssmError::throwMe(crtn);
		}
	}
	return rtnDsa;
}

/* 
 * Convert a CssmKey to an DSA * key. May result in the creation of a new
 * DSA (when cssmKey is a raw key); allocdKey is true in that case
 * in which case the caller generally has to free the allocd key).
 */
DSA *cssmKeyToDsa(
	const CssmKey	&cssmKey,
	AppleCSPSession	&session,
	bool			&allocdKey)		// RETURNED
{
	DSA *dsaKey = NULL;
	allocdKey = false;
	
	const CSSM_KEYHEADER *hdr = &cssmKey.KeyHeader;
	if(hdr->AlgorithmId != CSSM_ALGID_DSA) {
		// someone else's key (should never happen)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	switch(hdr->BlobType) {
		case CSSM_KEYBLOB_RAW:
			dsaKey = rawCssmKeyToDsa(cssmKey, session, NULL);
			allocdKey = true;
			break;
		case CSSM_KEYBLOB_REFERENCE:
		{
			BinaryKey &binKey = session.lookupRefKey(cssmKey);
			DSABinaryKey *dsaBinKey = dynamic_cast<DSABinaryKey *>(&binKey);
			/* this cast failing means that this is some other
			 * kind of binary key */
			if(dsaBinKey == NULL) {
				rsaMiscDebug("cssmKeyToDsa: wrong BinaryKey subclass\n");
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
			}
			assert(dsaBinKey->mDsaKey != NULL);
			dsaKey = dsaBinKey->mDsaKey;
			break;
		}
		default:
			CssmError::throwMe(CSSMERR_CSP_KEY_BLOB_TYPE_INCORRECT);
	}
	return dsaKey;
}

/* 
 * Convert a raw CssmKey to a newly alloc'd DSA key.
 */
DSA *rawCssmKeyToDsa(
	const CssmKey	&cssmKey,
	AppleCSPSession	&session,
	const CssmKey	*paramKey)		// optional
{
	const CSSM_KEYHEADER *hdr = &cssmKey.KeyHeader;
	bool isPub;
	
	assert(hdr->BlobType == CSSM_KEYBLOB_RAW); 
	
	if(hdr->AlgorithmId != CSSM_ALGID_DSA) {
		// someone else's key (should never happen)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	/* validate and figure out what we're dealing with */
	switch(hdr->KeyClass) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			switch(hdr->Format) {
				case CSSM_KEYBLOB_RAW_FORMAT_FIPS186:	
				case CSSM_KEYBLOB_RAW_FORMAT_X509:
				case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH2:
					break;
				default:
					CssmError::throwMe(
						CSSMERR_CSP_INVALID_ATTR_PUBLIC_KEY_FORMAT);
			}
			isPub = true;
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			switch(hdr->Format) {
				case CSSM_KEYBLOB_RAW_FORMAT_FIPS186:	// default
				case CSSM_KEYBLOB_RAW_FORMAT_OPENSSL:	// openssl style
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:		// SMIME style
					break;
				/* openssh real soon now */
				case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH:
				default:
					CssmError::throwMe(
						CSSMERR_CSP_INVALID_ATTR_PRIVATE_KEY_FORMAT);
			}
			isPub = false;
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
	
	CSSM_RETURN crtn;
	DSA *dsaKey = DSA_new();
    
    if (dsaKey == NULL) {
        crtn = CSSMERR_CSP_MEMORY_ERROR;
    }
    else {
        if(dsaKey == NULL) {
            CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
        }
        if(isPub) {
            crtn = DSAPublicKeyDecode(dsaKey, hdr->Format,
                cssmKey.KeyData.Data, 
                cssmKey.KeyData.Length);
        }
        else {
            crtn = DSAPrivateKeyDecode(dsaKey, hdr->Format,
                cssmKey.KeyData.Data, 
                cssmKey.KeyData.Length);
        }
    }
	
    if(crtn) {
        if (dsaKey != NULL) {
            DSA_free(dsaKey);
        }
        
        CssmError::throwMe(crtn);
    }
	/* 
	 * Add in optional external parameters if this is not fully formed.
	 * This path is only taken from DSAKeyInfoProvider::CssmKeyToBinary,
	 * e.g., when doing a NULL unwrap of a partially formed DSA public 
	 * key with the "complete the key with these params" option.
	 */
	if(isPub && (dsaKey->p == NULL) && (paramKey != NULL)) {
		rsaMiscDebug("rawCssmKeyToDsa; updating dsaKey %p", dsaKey);
		crtn = dsaGetParamsFromKey(dsaKey, *paramKey, session);
		if(crtn) {
			DSA_free(dsaKey);
			CssmError::throwMe(crtn);
		}
	}
	
	if(dsaKey->p != NULL) {
		/* avoid use of provided DSA key which exceeds the max size */
		uint32 keySize = BN_num_bits(dsaKey->p);
		if(keySize > DSA_MAX_KEY_SIZE) {
			DSA_free(dsaKey);
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_KEY_LENGTH);
		}
	}
	return dsaKey;
}

/*
 * Given a DSA private key, calculate its public component if it 
 * doesn't already exist. Used for calculating the key digest of 
 * an incoming raw private key.
 */
void dsaKeyPrivToPub(
	DSA *dsaKey)
{
	assert(dsaKey != NULL);
	assert(dsaKey->priv_key != NULL);
	
	if(dsaKey->pub_key != NULL) {
		return;
	}

	/* logic copied from DSA_generate_key() */
	dsaKey->pub_key = BN_new();
	if(dsaKey->pub_key == NULL) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
	}
	BN_CTX *ctx = BN_CTX_new();
	if (ctx == NULL) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
	}
	int rtn = BN_mod_exp(dsaKey->pub_key,
		dsaKey->g,
		dsaKey->priv_key,
		dsaKey->p,
		ctx);
	BN_CTX_free(ctx);
	if(rtn == 0) {
		CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
	}
}
