/*
 * Copyright (c) 2003,2011-2012,2014 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please 
 * obtain a copy of the License at http://www.apple.com/publicsource and 
 * read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER 
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, 
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, 
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. 
 * Please see the License for the specific language governing rights and 
 * limitations under the License.
 */
/*
 * AppleCSPKeys.cpp - Key support
 */
 
#include "AppleCSPKeys.h"
#include "AppleCSPUtils.h"
/*
 * CSPKeyInfoProvider for symmetric keys. 
 */
CSPKeyInfoProvider *SymmetricKeyInfoProvider::provider(
		const CssmKey 	&cssmKey,
		AppleCSPSession	&session)
{
	if(cssmKey.blobType() != CSSM_KEYBLOB_RAW) {
		errorLog0("KeyInfoProvider deals only with RAW keys!\n");
		CssmError::throwMe(CSSMERR_CSP_INTERNAL_ERROR);
	}
	if(cssmKey.keyClass() != CSSM_KEYCLASS_SESSION_KEY) {
		/* that's all we need to know */
		return NULL;
	}
	return new SymmetricKeyInfoProvider(cssmKey, session);
}
 
SymmetricKeyInfoProvider::SymmetricKeyInfoProvider(
	const CssmKey 	&cssmKey,
	AppleCSPSession	&session) :
		CSPKeyInfoProvider(cssmKey, session)
{
}

/* cook up a Binary key */
void SymmetricKeyInfoProvider::CssmKeyToBinary(
	CssmKey				*paramKey,	// ignored
	CSSM_KEYATTR_FLAGS	&attrFlags,	// IN/OUT
	BinaryKey 			**binKey)
{
	CASSERT(mKey.keyClass() == CSSM_KEYCLASS_SESSION_KEY);
	SymmetricBinaryKey *symBinKey = new SymmetricBinaryKey(
		mKey.KeyHeader.LogicalKeySizeInBits);
	copyCssmData(mKey, 
		symBinKey->mKeyData, 
		symBinKey->mAllocator);
	*binKey = symBinKey;
}

/* obtain key size in bits */
void SymmetricKeyInfoProvider::QueryKeySizeInBits(
	CSSM_KEY_SIZE &keySize)
{
	/* FIXME - do we ever need to calculate RC2 effective size here? */
	keySize.LogicalKeySizeInBits = keySize.EffectiveKeySizeInBits =
		(uint32)(mKey.length() * 8);
}

/* 
 * Obtain blob suitable for hashing in CSSM_APPLECSP_KEYDIGEST 
 * passthrough.
 */
bool SymmetricKeyInfoProvider::getHashableBlob(
	Allocator 	&allocator,
	CssmData		&blob)			// blob to hash goes here
{
	/*
	 * This is trivial: the raw key is already in the "proper" format.
	 */
	assert(mKey.blobType() == CSSM_KEYBLOB_RAW);
	const CssmData &keyBlob = CssmData::overlay(mKey.KeyData);
	copyCssmData(keyBlob, blob, allocator);
	return true;
}

