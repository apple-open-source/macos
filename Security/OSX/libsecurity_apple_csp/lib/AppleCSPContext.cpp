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
// AppleCSPContext.cpp - CSP-wide contexts 
//

#include "AppleCSPContext.h"
#include "AppleCSPSession.h"
#include "AppleCSPUtils.h"

/*
 * Empty destructor (just to avoid out-of-line copies)
 */
AppleCSPContext::~AppleCSPContext()
{ }

/* 
 * get symmetric key bits - context.key can be either ref or raw.
 * A convenience routine typically used by subclass's init().
 */
void AppleCSPContext::symmetricKeyBits(
	const Context 	&context,
	AppleCSPSession &session,
	CSSM_ALGORITHMS	requiredAlg,	// throws if this doesn't match key alg
	CSSM_KEYUSE 	intendedUse,	// throws if key usage doesn't match this
	uint8			*&keyBits,		// RETURNED (not mallocd or copied)
	CSSM_SIZE		&keyLen)		// RETURNED
{
	/* key must be present and it must be a session key matching caller's spec */
    CssmKey &key = 
		context.get<CssmKey>(CSSM_ATTRIBUTE_KEY, CSSMERR_CSP_MISSING_ATTR_KEY);
	if(key.keyClass() != CSSM_KEYCLASS_SESSION_KEY) {
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_CLASS);
	}
	if(key.algorithm() != requiredAlg) {
		CssmError::throwMe(CSSMERR_CSP_ALGID_MISMATCH);
	}
	cspValidateIntendedKeyUsage(&key.KeyHeader, intendedUse);
	cspVerifyKeyTimes(key.KeyHeader);
	
	/* extract raw bits one way or the other */
	switch(key.blobType()) {
		case CSSM_KEYBLOB_RAW:
			/* easy case, the bits are right there in the CssmKey */
			if(key.blobFormat() != CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING) {
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
			}
			keyLen = key.length();
			keyBits = key.KeyData.Data;
			break;
			
		case CSSM_KEYBLOB_REFERENCE:
		{
			/* do a lookup to get a binary key */
			BinaryKey &binKey = session.lookupRefKey(key);
			/* fails if this is not a SymmetricBinaryKey */
			SymmetricBinaryKey *symBinKey =
				dynamic_cast<SymmetricBinaryKey *>(&binKey);
			if(symBinKey == NULL) {
				CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
			}
			keyLen = symBinKey->mKeyData.Length;
			keyBits = symBinKey->mKeyData.Data;
			break;
		}
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY_FORMAT);
	}
	return;
}

AppleKeyPairGenContext::~AppleKeyPairGenContext()
{ /* virtual */ }

// Called from subclass after it allocates its BinaryKeys.
// Caller frees BinaryKeys if we throw any exception. 
void AppleKeyPairGenContext::generate(
	const Context 	&context, 
	AppleCSPSession	&session,
	CssmKey 		&pubKey, 
	BinaryKey 		*pubBinKey,
	CssmKey 		&privKey,
	BinaryKey		*privBinKey)
{
	uint32			keySize;
	cspKeyStorage 	privStorage;
	cspKeyStorage 	pubStorage;
	CssmKey::Header	&pubHdr  = pubKey.header(); 
	CssmKey::Header	&privHdr = privKey.header(); 
	
	// validate context and key header args
	pubStorage  = cspParseKeyAttr(CKT_Public,  pubHdr.KeyAttr);
	privStorage = cspParseKeyAttr(CKT_Private, privHdr.KeyAttr);
	cspValidateKeyUsageBits(CKT_Public,  pubHdr.KeyUsage);
	cspValidateKeyUsageBits(CKT_Private, privHdr.KeyUsage);
	
	// have subclass generate the key pairs in the form of 
	// its native BinaryKeys
	generate(context, *pubBinKey, *privBinKey, keySize);

	// FIXME - Any other header setup?
	pubHdr.LogicalKeySizeInBits = 
		privHdr.LogicalKeySizeInBits = keySize;
	pubHdr.KeyAttr  &= ~KEY_ATTR_RETURN_MASK;
	privHdr.KeyAttr &= ~KEY_ATTR_RETURN_MASK;
		 
	// Handle key formatting. Delete the BinaryKeys if
	// we're not creating ref keys, after safe completion of 
	// generateKeyBlob (which may throw, in which case the binary keys
	// get deleted by our caller). 
	CSSM_KEYATTR_FLAGS attrFlags = 0;
	switch(pubStorage) {
		case CKS_Ref:
			session.addRefKey(*pubBinKey, pubKey);
			break;
		case CKS_Data:
			pubHdr.Format = requestedKeyFormat(context, pubKey);
			pubBinKey->mKeyHeader  = pubHdr;
			pubBinKey->generateKeyBlob(
				session.normAlloc(),		// alloc in user space
				CssmData::overlay(pubKey.KeyData),
				pubHdr.Format,
				session,
				NULL,						// no paramKey here!
				attrFlags);
			break;
		case CKS_None:
			break;
	}
	switch(privStorage) {
		case CKS_Ref:
			session.addRefKey(*privBinKey, privKey);
			break;
		case CKS_Data:
			privHdr.Format = requestedKeyFormat(context, privKey);
			privBinKey->mKeyHeader = privHdr;
			privBinKey->generateKeyBlob(
				session.normAlloc(),		// alloc in user space
				CssmData::overlay(privKey.KeyData),
				privHdr.Format,
				session,
				NULL,
				attrFlags);
			break;
		case CKS_None:
			break;
	}
	if(pubStorage != CKS_Ref) {
		delete pubBinKey;
	}
	if(privStorage != CKS_Ref) {
		delete privBinKey;
	}
}

/*
 * Called from subclass's generate method. Subclass is also a 
 * AppleCSPContext.
 */
void AppleSymmKeyGenContext::generateSymKey(
	const Context 	&context, 
	AppleCSPSession	&session,		// for ref keys
	CssmKey 		&cssmKey)		// RETURNED 
{
	/* there really is no legal way this should throw... */
	uint32 reqKeySize = context.getInt(
		CSSM_ATTRIBUTE_KEY_LENGTH, 
		CSSMERR_CSP_MISSING_ATTR_KEY_LENGTH);
	if((reqKeySize < minSizeInBits) ||
	   (reqKeySize > maxSizeInBits)) {
		CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEY_SIZE);
	}
	if(mustBeByteSized) {
		if((reqKeySize & 0x7) != 0) {
			CssmError::throwMe(CSSMERR_CSP_UNSUPPORTED_KEY_SIZE);
		}
	}
	
	// validate KeyAtrr and KeyUsage already present in header
	cspKeyStorage 	keyStorage;
	CssmKey::Header	&hdr  = cssmKey.header(); 
	
	keyStorage = cspParseKeyAttr(CKT_Session,  hdr.KeyAttr);
	cspValidateKeyUsageBits(CKT_Session, hdr.KeyUsage);
	hdr.KeyAttr  &= ~KEY_ATTR_RETURN_MASK;
	
	hdr.LogicalKeySizeInBits = reqKeySize;
	uint32 keySizeInBytes = (reqKeySize + 7) / 8;
	SymmetricBinaryKey *binKey = NULL;
	CssmData *keyData = NULL;
	
	switch(keyStorage) {
		case CKS_None:
			/* no way */
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEYATTR_MASK);
		case CKS_Ref:
			/* cook up a symmetric binary key */
			binKey = new SymmetricBinaryKey(reqKeySize);
			keyData = &binKey->mKeyData;
			break;
		case CKS_Data:
			/* key bytes --> caller's cssmKey */
			keyData = &(CssmData::overlay(cssmKey.KeyData));
			setUpCssmData(*keyData, keySizeInBytes, 
				session.normAlloc());
			break;
	}
	
	// in any case, fill key bytes with random data
	session.getRandomBytes(keySizeInBytes, keyData->Data);

	if(keyStorage == CKS_Ref) {
		session.addRefKey(*binKey, cssmKey);
	}
	else {
		/* Raw data */
		hdr.BlobType = CSSM_KEYBLOB_RAW;
		hdr.Format = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING; 
	}

	// FIXME - any other header fields?
}

//
// Symmetric Binary Key support
//
SymmetricBinaryKey::SymmetricBinaryKey(
	unsigned keySizeInBits) :
		mAllocator(Allocator::standard(Allocator::sensitive))
{
	setUpCssmData(mKeyData, (keySizeInBits + 7) / 8, mAllocator);
}

SymmetricBinaryKey::~SymmetricBinaryKey()
{
	freeCssmData(mKeyData, mAllocator);
}

void SymmetricBinaryKey::generateKeyBlob(
	Allocator 		&allocator,
	CssmData			&blob,
	CSSM_KEYBLOB_FORMAT	&format,	// CSSM_KEYBLOB_RAW_FORMAT_PKCS1, etc.
	AppleCSPSession		&session,
	const CssmKey		*paramKey,	/* optional, unused here */
	CSSM_KEYATTR_FLAGS 	&attrFlags)	/* IN/OUT */
{
	switch(format) {
		case CSSM_KEYBLOB_RAW_FORMAT_NONE:			// default
		case CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING:	// the one we can do
		case CSSM_KEYBLOB_RAW_FORMAT_DIGEST:		// same thing
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_SYMMETRIC_KEY_FORMAT);
	}
	copyCssmData(mKeyData, blob, allocator);
	format = CSSM_KEYBLOB_RAW_FORMAT_OCTET_STRING;
}

