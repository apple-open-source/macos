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


//
// pkcs8.cpp - PKCS8 key wrap/unwrap support.
//


#include "pkcs8.h"
#include "AppleCSPUtils.h"
#include "AppleCSPKeys.h"
#include <SecurityNssAsn1/keyTemplates.h>
#include <SecurityNssAsn1/SecNssCoder.h>
#include <SecurityNssAsn1/nssUtils.h>
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
	if(coder.decodeItem(keyData, NSS_PrivateKeyInfoTemplate,
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
			hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
			break;
		case CSSM_ALGID_DSA:
			hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
			break;
		default:
			/* punt */
			hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_NONE;
			break;
	}
	
	/* 
	 * Find someone whoe knows about this key and ask them the
	 * key size
	 */
	CSPKeyInfoProvider *provider = infoProvider(key);
	if(provider == NULL) {
		errorLog0("pkcs8InferKeyHeader no info provider\n");
		/* but we got this far, so don't abort */
		return;
	}
	CSSM_KEY_SIZE keySize;
	provider->QueryKeySizeInBits(keySize);
	hdr.LogicalKeySizeInBits = keySize.LogicalKeySizeInBits;
	delete provider;
}

/*
 * When doing a PKCS8 wrap operation on a reference key, this
 * is used to infer the blob type to obtain before the encryption.
 */
CSSM_KEYBLOB_FORMAT pkcs8RawKeyFormat(
	CSSM_ALGORITHMS	keyAlg)
{
	switch(keyAlg) {
		case CSSM_ALGID_RSA:
			return CSSM_KEYBLOB_RAW_FORMAT_PKCS8;
		case CSSM_ALGID_DSA:
			return CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
		default:
			/* punt */
			return CSSM_KEYBLOB_RAW_FORMAT_NONE;
	}
}
