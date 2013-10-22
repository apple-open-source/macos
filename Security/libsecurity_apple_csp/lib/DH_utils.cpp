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
#include <opensslUtils/opensslAsn1.h>
#include <security_utilities/logging.h>
#include <security_utilities/debugging.h>
#include <opensslUtils/opensslUtils.h>
#include <openssl/bn.h>
#include <openssl/dh.h>
#include <openssl/err.h>

#define dhMiscDebug(args...)	secdebug("dhMisc", ## args)

/* 
 * Given a Context:
 * -- obtain CSSM key with specified attr (there must only be one)
 * -- validate keyClass per caller's specification
 * -- validate keyUsage
 * -- convert to DH *, allocating the DH key if necessary
 */
DH *contextToDhKey(
	const Context		&context,
	AppleCSPSession 	&session,
	CSSM_ATTRIBUTE_TYPE	attr,		  // CSSM_ATTRIBUTE_KEY for private key
									  // CSSM_ATTRIBUTE_PUBLIC_KEY for public key
	CSSM_KEYCLASS		keyClass,	  // CSSM_KEYCLASS_{PUBLIC,PRIVATE}_KEY	
	CSSM_KEYUSE			usage,		  // CSSM_KEYUSE_ENCRYPT, CSSM_KEYUSE_SIGN, etc.
	bool				&mallocdKey)  // RETURNED
{
    CssmKey *cssmKey = context.get<CssmKey>(attr);
	if(cssmKey == NULL) {
		return NULL;
	}
	const CSSM_KEYHEADER &hdr = cssmKey->KeyHeader;
	if(hdr.AlgorithmId != CSSM_ALGID_DH) {
		CssmError::throwMe(CSSMERR_CSP_ALGID_MISMATCH);
	}
	if(hdr.KeyClass != keyClass) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
	cspValidateIntendedKeyUsage(&hdr, usage);
	cspVerifyKeyTimes(hdr);
	return cssmKeyToDh(*cssmKey, session, mallocdKey);
}

/* 
 * Convert a CssmKey to an DH * key. May result in the 
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
	switch(hdr->BlobType) {
		case CSSM_KEYBLOB_RAW:
			dhKey = rawCssmKeyToDh(cssmKey);
			cspDhDebug("cssmKeyToDh, raw, dhKey %p", dhKey);
			allocdKey = true;
			break;
		case CSSM_KEYBLOB_REFERENCE:
		{
			BinaryKey &binKey = session.lookupRefKey(cssmKey);
			DHBinaryKey *dhBinKey = dynamic_cast<DHBinaryKey *>(&binKey);
			/* this cast failing means that this is some other
			 * kind of binary key */
			if(dhBinKey == NULL) {
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
			}
			assert(dhBinKey->mDhKey != NULL);
			dhKey = dhBinKey->mDhKey;
			cspDhDebug("cssmKeyToDh, ref, dhKey %p", dhKey);
			break;
		}
		default:
			CssmError::throwMe(CSSMERR_CSP_KEY_BLOB_TYPE_INCORRECT);
	}
	return dhKey;
}

/* 
 * Convert a raw CssmKey to a newly alloc'd DH key.
 */
DH *rawCssmKeyToDh(
	const CssmKey	&cssmKey)
{
	const CSSM_KEYHEADER *hdr = &cssmKey.KeyHeader;
	bool isPub = false;
	
	if(hdr->AlgorithmId != CSSM_ALGID_DH) {
		// someone else's key (should never happen)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	assert(hdr->BlobType == CSSM_KEYBLOB_RAW); 
	/* validate and figure out what we're dealing with */
	switch(hdr->KeyClass) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			switch(hdr->Format) {
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS3:	
				case CSSM_KEYBLOB_RAW_FORMAT_X509:
					break;
				/* openssh real soon now */
				case CSSM_KEYBLOB_RAW_FORMAT_OPENSSH:
				default:
					CssmError::throwMe(
						CSSMERR_CSP_INVALID_ATTR_PUBLIC_KEY_FORMAT);
			}
			isPub = true;
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			switch(hdr->Format) {
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS3:	// default
				case CSSM_KEYBLOB_RAW_FORMAT_PKCS8:	// SMIME style
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

	DH *dhKey = DH_new();
	if(dhKey == NULL) {
		crtn = CSSMERR_CSP_MEMORY_ERROR;
	}
    else
    {
        if(isPub) {
            crtn = DHPublicKeyDecode(dhKey, hdr->Format,
                cssmKey.KeyData.Data, (unsigned)cssmKey.KeyData.Length);
        }
        else {
            crtn = DHPrivateKeyDecode(dhKey, hdr->Format,
                cssmKey.KeyData.Data, (unsigned)cssmKey.KeyData.Length);
        }
    }
    
	if(crtn) {
        if (dhKey != NULL) {
            DH_free(dhKey);
        }
        
        CssmError::throwMe(crtn);
	}
	cspDhDebug("rawCssmKeyToDh, dhKey %p", dhKey);
	return dhKey;
}

