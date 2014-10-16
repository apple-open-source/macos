/*
 * Copyright (c) 2000-2001,2011,2014 Apple Inc. All Rights Reserved.
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


//
// pkcs8.cpp - PKCS8 key wrap/unwrap support.
//


#include "pkcs8.h"
#include "AppleCSPUtils.h"
#include "AppleCSPKeys.h"
#include <Security/keyTemplates.h>
#include <security_asn1/SecNssCoder.h>
#include <security_asn1/nssUtils.h>
#include "AppleCSPSession.h"
#include <Security/cssmapple.h>

/*
 * Given a key in PKCS8 format, fill in the following
 * header fields:
 *
 *	CSSM_KEYBLOB_FORMAT Format
 *  CSSM_ALGORITHMS AlgorithmId
 *  uint32 LogicalKeySizeInBits
 */
void AppleCSPSession::pkcs8InferKeyHeader( 
	CssmKey			&key)
{
	/* 
	 * Incoming key blob is a PrivateKeyInfo. Take it apart
	 * to get its algorithm info, from which we infer other
	 * fields.
	 */
	NSS_PrivateKeyInfo privKeyInfo;
	SecNssCoder coder;
	CSSM_DATA &keyData = key.KeyData;
	
	memset(&privKeyInfo, 0, sizeof(privKeyInfo));
	if(coder.decodeItem(keyData, kSecAsn1PrivateKeyInfoTemplate,
			&privKeyInfo)) {
		errorLog0("pkcs8InferKeyHeader decode error\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}
	
	CSSM_KEYHEADER &hdr = key.KeyHeader;
	if(!cssmOidToAlg(&privKeyInfo.algorithm.algorithm, 
			&hdr.AlgorithmId)) {
		errorLog0("pkcs8InferKeyHeader unknown algorithm\n");
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	
	switch(hdr.AlgorithmId) {
		case CSSM_ALGID_RSA:
		case CSSM_ALGID_ECDSA:
			hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_PKCS8; 
			break;
		case CSSM_ALGID_DSA:
			/* 
			 * Try openssl style first, though our default when 
			 * wrapping is FIPS186
			 */
			hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
			break;
		default:
			/* punt */
			hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_NONE;
			break;
	}
	
	/* 
	 * Find someone who knows about this key and ask them the
	 * key size. infoProvider() throws if no provider found.
	 */
	CSSM_KEY_SIZE keySize;
	try {
		auto_ptr<CSPKeyInfoProvider> provider(infoProvider(key));
		provider->QueryKeySizeInBits(keySize);
	}
	catch(const CssmError &cerror) {
		/*
		 * Special case: DSA private keys keys can be in two forms - FIPS186
		 * (for legacy implementations) and PKCS8 (for openssl). We're wired to
		 * *generate* FIPS186 blobs by default in pkcs8RawKeyFormat(), but to 
		 * decode openssl-generated DSA private keys in wrapped FIPS186 format
		 * we have to try both. 
		 */
		if((cerror.error == CSSMERR_CSP_INVALID_KEY) &&
		   (hdr.AlgorithmId == CSSM_ALGID_DSA)) {
			hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
			try {
				auto_ptr<CSPKeyInfoProvider> provider(infoProvider(key));
				provider->QueryKeySizeInBits(keySize);
			}
			catch(...) {
				/* out of luck */
				throw;
			}
		}
		else {
			/* other error, give up */
			throw;
		}
	}
	catch(...) {
		/* other (non-CSSM) error, give up */
		throw;
	}
	hdr.LogicalKeySizeInBits = keySize.LogicalKeySizeInBits;
}

/*
 * When doing a PKCS8 wrap operation on a reference key, this
 * is used to infer the blob type to obtain before the encryption.
 * App can override this with a 
 * CSSM_ATTRIBUTE_{PRIVATE,PUBLIC,SESSION}_KEY_FORMAT 
 * context attribute. 
 */
CSSM_KEYBLOB_FORMAT pkcs8RawKeyFormat(
	CSSM_ALGORITHMS	keyAlg)
{
	switch(keyAlg) {
		case CSSM_ALGID_RSA:
		case CSSM_ALGID_ECDSA:
			return CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
		case CSSM_ALGID_DSA:
			return CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
		default:
			/* punt */
			return CSSM_KEYBLOB_RAW_FORMAT_NONE;
	}
}

/*
 * When doing a OPENSSL style wrap operation on a reference key, this
 * is used to infer the blob type to obtain before the encryption.
 * App can override this with a 
 * CSSM_ATTRIBUTE_{PRIVATE,PUBLIC,SESSION}_KEY_FORMAT 
 * context attribute. 
 */
CSSM_KEYBLOB_FORMAT opensslRawKeyFormat(
	CSSM_ALGORITHMS	keyAlg)
{
	switch(keyAlg) {
		case CSSM_ALGID_RSA:
			return CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
		case CSSM_ALGID_DSA:
			return CSSM_KEYBLOB_RAW_FORMAT_OPENSSL;
		case CSSM_ALGID_ECDSA:
			return CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
		default:
			/* punt */
			return CSSM_KEYBLOB_RAW_FORMAT_NONE;
	}
}

/*
 * Given a key in some kind of openssl format (just subsequent to decryption
 * during an unwrap), fill in the following header fields:
 *
 *	CSSM_KEYBLOB_FORMAT Format
 *  uint32 LogicalKeySizeInBits
 */
void AppleCSPSession::opensslInferKeyHeader( 
	CssmKey			&key)
{
	CSSM_KEYHEADER &hdr = key.KeyHeader;
	switch(hdr.AlgorithmId) {
		case CSSM_ALGID_RSA:
			hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
			break;
		case CSSM_ALGID_DSA:
			hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_OPENSSL;
			break;
		case CSSM_ALGID_ECDSA:
			hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
			break;
		default:
			/* punt */
			hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_NONE;
			return;
	}
	
	/* now figure out the key size by finding a provider for this key */
	CSSM_KEY_SIZE keySize;
	try {
		auto_ptr<CSPKeyInfoProvider> provider(infoProvider(key));
		provider->QueryKeySizeInBits(keySize);
	}
	catch(...) {
		/* no recovery possible */
		throw;
	}
	hdr.LogicalKeySizeInBits = keySize.LogicalKeySizeInBits;
	return;
}

