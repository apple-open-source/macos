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
#include <opensslUtils/openRsaSnacc.h>
#include <Security/logging.h>
#include <Security/debugging.h>
#include <open_ssl/opensslUtils/opensslUtils.h>
#include <openssl/bn.h>
#include <openssl/rsa.h>
#include <openssl/dsa.h>
#include <openssl/err.h>

#define rsaMiscDebug(args...)	debug("rsaMisc", ## args)

void throwRsaDsa(
	const char *op)
{
	unsigned long e = logSslErrInfo(op);
	CSSM_RETURN cerr = CSSM_OK;
	
	/* try to parse into something meaningful */
	int reason = ERR_GET_REASON(e);
	int lib = ERR_GET_LIB(e);
	
	/* first try the global ones */
	switch(reason) {
		case ERR_R_MALLOC_FAILURE:
			cerr = CSSMERR_CSP_MEMORY_ERROR; break;
		case ERR_R_SHOULD_NOT_HAVE_BEEN_CALLED:
			/* internal */ break;
		case ERR_R_PASSED_NULL_PARAMETER:
			cerr = CSSMERR_CSP_INVALID_POINTER; break;
		case ERR_R_NESTED_ASN1_ERROR:
		case ERR_R_BAD_ASN1_OBJECT_HEADER:
		case ERR_R_BAD_GET_ASN1_OBJECT_CALL:
		case ERR_R_EXPECTING_AN_ASN1_SEQUENCE:
		case ERR_R_ASN1_LENGTH_MISMATCH:
		case ERR_R_MISSING_ASN1_EOS:
			/* ASN - shouldn't happen, right? */
			cerr = CSSMERR_CSP_INTERNAL_ERROR; break;
		default:
			break;
	}
	if(cerr != CSSM_OK) {
		CssmError::throwMe(cerr);
	}
	
	/* now the lib-specific ones */
	switch(lib) {
		case ERR_R_BN_LIB:
			/* all indicate serious internal error...right? */
			cerr = CSSMERR_CSP_INTERNAL_ERROR; break;
		case ERR_R_RSA_LIB:
			switch(reason) {
				case RSA_R_ALGORITHM_MISMATCH:
					cerr = CSSMERR_CSP_ALGID_MISMATCH; break;
				case RSA_R_BAD_SIGNATURE:
					cerr = CSSMERR_CSP_VERIFY_FAILED; break;
				case RSA_R_DATA_TOO_LARGE:
				case RSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE:
				case RSA_R_DATA_TOO_SMALL:
				case RSA_R_DATA_TOO_SMALL_FOR_KEY_SIZE:
				case RSA_R_DIGEST_TOO_BIG_FOR_RSA_KEY:
					cerr = CSSMERR_CSP_INPUT_LENGTH_ERROR; break;
				case RSA_R_KEY_SIZE_TOO_SMALL:
					cerr = CSSMERR_CSP_INVALID_ATTR_KEY_LENGTH; break;
				case RSA_R_PADDING_CHECK_FAILED:
					cerr = CSSMERR_CSP_INVALID_DATA; break;
				case RSA_R_RSA_OPERATIONS_NOT_SUPPORTED:
					cerr = CSSMERR_CSP_FUNCTION_NOT_IMPLEMENTED; break;
				case RSA_R_UNKNOWN_ALGORITHM_TYPE:
					cerr = CSSMERR_CSP_INVALID_ALGORITHM; break;
				case RSA_R_WRONG_SIGNATURE_LENGTH:
					cerr = CSSMERR_CSP_VERIFY_FAILED; break;
				default:
					cerr = CSSMERR_CSP_INTERNAL_ERROR; break;
			}
			break;
		case ERR_R_DSA_LIB:
			switch(reason) {
				case DSA_R_DATA_TOO_LARGE_FOR_KEY_SIZE:
					cerr = CSSMERR_CSP_INPUT_LENGTH_ERROR; break;
				default:
					cerr = CSSMERR_CSP_INTERNAL_ERROR; break;
			}
			break;
		default:
			cerr = CSSMERR_CSP_INTERNAL_ERROR; break;
	}
	CssmError::throwMe(cerr);
}
	
/*
 * given an openssl-style error, throw appropriate CssmError.
 */
void throwOpensslErr(int irtn)
{
	/* FIXME */
	CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
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
	bool				&mallocdKey)  // RETURNED
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
	return cssmKeyToRsa(cssmKey, session, mallocdKey);
}
/* 
 * Convert a CssmKey to an RSA * key. May result in the creation of a new
 * RSA (when cssmKey is a raw key); allocdKey is true in that case
 * in which case the caller generally has to free the allocd key).
 */
RSA *cssmKeyToRsa(
	const CssmKey	&cssmKey,
	AppleCSPSession	&session,
	bool			&allocdKey)		// RETURNED
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
			rsaKey = rawCssmKeyToRsa(cssmKey);
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
	const CssmKey	&cssmKey)
{
	const CSSM_KEYHEADER *hdr = &cssmKey.KeyHeader;
	bool isPub;
	
	assert(hdr->BlobType == CSSM_KEYBLOB_RAW); 
	
	if(hdr->AlgorithmId != CSSM_ALGID_RSA) {
		// someone else's key (should never happen)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	switch(hdr->KeyClass) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			if(hdr->Format != RSA_PUB_KEY_FORMAT) {
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PUBLIC_KEY_FORMAT);
			}
			isPub = true;
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			if(hdr->Format != RSA_PRIV_KEY_FORMAT) {
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
	if(isPub) {
		crtn = RSAPublicKeyDecode(rsaKey,
			cssmKey.KeyData.Data, 
			cssmKey.KeyData.Length);
	}
	else {
		crtn = RSAPrivateKeyDecode(rsaKey, 
			cssmKey.KeyData.Data, 
			cssmKey.KeyData.Length);
	}
	if(crtn) {
		CssmError::throwMe(crtn);
	}
	return rsaKey;
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
	return cssmKeyToDsa(cssmKey, session, mallocdKey);
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
			dsaKey = rawCssmKeyToDsa(cssmKey);
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
	const CssmKey	&cssmKey)
{
	const CSSM_KEYHEADER *hdr = &cssmKey.KeyHeader;
	bool isPub;
	
	assert(hdr->BlobType == CSSM_KEYBLOB_RAW); 
	
	if(hdr->AlgorithmId != CSSM_ALGID_DSA) {
		// someone else's key (should never happen)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);
	}
	switch(hdr->KeyClass) {
		case CSSM_KEYCLASS_PUBLIC_KEY:
			if(hdr->Format != DSA_PUB_KEY_FORMAT) {
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PUBLIC_KEY_FORMAT);
			}
			isPub = true;
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			if(hdr->Format != DSA_PRIV_KEY_FORMAT) {
				CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PRIVATE_KEY_FORMAT);
			}
			isPub = false;
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
	
	DSA *dsaKey = DSA_new();
	if(dsaKey == NULL) {
		CssmError::throwMe(CSSMERR_CSP_MEMORY_ERROR);
	}
	CSSM_RETURN crtn;
	if(isPub) {
		crtn = DSAPublicKeyDecode(dsaKey,
			cssmKey.KeyData.Data, 
			cssmKey.KeyData.Length);
	}
	else {
		crtn = DSAPrivateKeyDecode(dsaKey,
			cssmKey.KeyData.Data, 
			cssmKey.KeyData.Length);
	}
	if(crtn) {
		CssmError::throwMe(crtn);
	}
	return dsaKey;
}
