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
 * DH_utils.cpp
 */

#include "DH_utils.h"
#include "DH_keys.h"
#include <opensslUtils/openRsaSnacc.h>
#include <Security/logging.h>
#include <Security/debugging.h>
#include <open_ssl/opensslUtils/opensslUtils.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/err.h>

#define dhMiscDebug(args...)	debug("dhMisc", ## args)

/* 
 * Given a Context:
 * -- obtain CSSM key (there must only be one)
 * -- validate keyClass - MUST be private! (DH public keys are never found
 *    in contexts.)
 * -- validate keyUsage
 * -- convert to DH *, allocating the DH key if necessary
 */
DH *contextToDhKey(
	const Context 		&context,
	AppleCSPSession	 	&session,
	CSSM_KEYUSE			usage,		  // CSSM_KEYUSE_ENCRYPT, CSSM_KEYUSE_SIGN, etc.
	bool				&mallocdKey)  // RETURNED
{
    CssmKey &cssmKey = 
		context.get<CssmKey>(CSSM_ATTRIBUTE_KEY, CSSMERR_CSP_MISSING_ATTR_KEY);
	const CSSM_KEYHEADER &hdr = cssmKey.KeyHeader;
	if(hdr.AlgorithmId != CSSM_ALGID_DH) {
		CssmError::throwMe(CSSMERR_CSP_ALGID_MISMATCH);
	}
	if(hdr.KeyClass != CSSM_KEYCLASS_PRIVATE_KEY) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
	cspValidateIntendedKeyUsage(&hdr, usage);
	return cssmKeyToDh(cssmKey, session, mallocdKey);
}
/* 
 * Convert a CssmKey (Private only!) to an DH * key. May result in the 
 * creation of a new DH (when cssmKey is a raw key); allocdKey is true 
 * in that case in which case the caller generally has to free the allocd key).
 */
DH *cssmKeyToDh(
	const CssmKey	&cssmKey,
	AppleCSPSession	&session,
	bool			&allocdKey)		// RETURNED
{
	DH *dhKey = NULL;
	allocdKey = false;
	
	const CSSM_KEYHEADER *hdr = &cssmKey.KeyHeader;
	if(hdr->AlgorithmId != CSSM_ALGID_DH) {
		// someone else's key (should never happen)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	assert(hdr->KeyClass == CSSM_KEYCLASS_PRIVATE_KEY);
	switch(hdr->BlobType) {
		case CSSM_KEYBLOB_RAW:
			dhKey = rawCssmKeyToDh(cssmKey);
			allocdKey = true;
			break;
		case CSSM_KEYBLOB_REFERENCE:
		{
			BinaryKey &binKey = session.lookupRefKey(cssmKey);
			DHBinaryKey *dhBinKey = dynamic_cast<DHBinaryKey *>(&binKey);
			/* this cast failing means that this is some other
			 * kind of binary key */
			if(dhBinKey == NULL) {
				dhMiscDebug("cssmKeyToDh: wrong BinaryKey subclass\n");
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
			}
			assert(dhBinKey->mDhKey != NULL);
			dhKey = dhBinKey->mDhKey;
			break;
		}
		default:
			CssmError::throwMe(CSSMERR_CSP_KEY_BLOB_TYPE_INCORRECT);
	}
	return dhKey;
}

/* 
 * Convert a raw CssmKey (Private only!)  to a newly alloc'd DH key.
 */
DH *rawCssmKeyToDh(
	const CssmKey	&cssmKey)
{
	const CSSM_KEYHEADER *hdr = &cssmKey.KeyHeader;
	
	if(hdr->AlgorithmId != CSSM_ALGID_DH) {
		// someone else's key (should never happen)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	assert(hdr->BlobType == CSSM_KEYBLOB_RAW); 
	assert(hdr->KeyClass == CSSM_KEYCLASS_PRIVATE_KEY);
	if(hdr->Format != DH_PRIV_KEY_FORMAT) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PRIVATE_KEY_FORMAT);
	}
	
	DH *dhKey = DH_new();
	if(dhKey == NULL) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
	}
	CSSM_RETURN crtn;
	crtn = DHPrivateKeyDecode(dhKey, 
		cssmKey.KeyData.Data, 
		cssmKey.KeyData.Length);
	if(crtn) {
		CssmError::throwMe(crtn);
	}
	return dhKey;
}

