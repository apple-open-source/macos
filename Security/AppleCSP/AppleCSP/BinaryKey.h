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
// BinaryKey.h - CSP-wide BinaryKey base class
//

#ifndef	_H_BINARY_KEY_
#define _H_BINARY_KEY_

#include <Security/utilities.h>
#include <Security/cssmtype.h>

// opaque key reference type 
typedef uint32	KeyRef;

// frame for Binary key; all modules (BSAFE, CryptKit) must subclass
// this and add a member whose type is the native raw key object.
// Subclasses must implement constructor, destructor, and generateKeyBlob().
class BinaryKey
{
public:
						BinaryKey() : mKeyRef(0) { }
	virtual 			~BinaryKey() { mKeyRef = 0; }

	/* 
	 * Generate raw key blob.
	 * The format argument is an in/out parameter and is optionally used
	 * to reque4st a specific keyblob format for providers which can generate
	 * multiple formats. This value comes from an optional
	 * CSSM_ATTRIBUTE_{PUBLIC,PRIVATE,SYMMETRIC}_KEY_FORMAT attribute in the current
	 * context. If so such attribute is present, the default value 
	 * CSSM_KEYBLOB_RAW_FORMAT_NONE is specified as the default input param.
	 */
	virtual void		generateKeyBlob(
		CssmAllocator 		&allocator,
		CssmData			&blob,
		CSSM_KEYBLOB_FORMAT	&format)	// in/out, CSSM_KEYBLOB_RAW_FORMAT_PKCS1, etc.
		{
			CssmError::throwMe(CSSMERR_CSP_FUNCTION_NOT_IMPLEMENTED); 
		}
		
	CssmKey::Header		mKeyHeader;
	KeyRef				mKeyRef;
};

// Binary key representing a symmetric key.
class SymmetricBinaryKey : public BinaryKey
{
public:
	SymmetricBinaryKey(
		unsigned keySizeInBits);
	~SymmetricBinaryKey();
	void generateKeyBlob(
		CssmAllocator 		&allocator,
		CssmData			&blob,
		CSSM_KEYBLOB_FORMAT	&format);	// CSSM_KEYBLOB_RAW_FORMAT_PKCS1, etc.

	CssmData			mKeyData;
	CssmAllocator 		&mAllocator;
};

/*
 * Stateless function to cook up a BinaryKey given a 
 * symmetric CssmKey in RAW format. Returns true on 
 * success, false if we can't deal with this type of key, 
 * throws exception on other runtime errors.
 */
bool symmetricCssmKeyToBinary(
	const CssmKey		&cssmKey,
	BinaryKey			**binKey);	// RETURNED

#endif	// _H_BINARY_KEY_

