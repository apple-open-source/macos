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
// BinaryKey.h - CSP-wide BinaryKey base class
//

#ifndef	_H_BINARY_KEY_
#define _H_BINARY_KEY_

#include <Security/cssmtype.h>
#include <security_cdsa_utilities/cssmkey.h>

// opaque key reference type 
typedef CSSM_INTPTR	KeyRef;

class AppleCSPSession;

/* 
 * unique blob type passed to generateKeyBlob() for key digest calculation 
 */
#define CSSM_KEYBLOB_RAW_FORMAT_DIGEST	\
	(CSSM_KEYBLOB_RAW_FORMAT_VENDOR_DEFINED + 0x12345)


// frame for Binary key; all modules (BSAFE, CryptKit) must subclass
// this and add a member whose type is the native raw key object.
// Subclasses must implement constructor, destructor, and generateKeyBlob().
class BinaryKey
{
public:
						BinaryKey() : mKeyRef(0), mDescData(Allocator::standard()) { }
	virtual 			~BinaryKey() { mKeyRef = 0; }

	/* 
	 * Generate raw key blob.
	 * The format argument is an in/out parameter and is optionally used
	 * to request a specific keyblob format for providers which can generate
	 * multiple formats. This value comes from an optional
	 * CSSM_ATTRIBUTE_{PUBLIC,PRIVATE,SYMMETRIC}_KEY_FORMAT attribute in the current
	 * context. If so such attribute is present, the default value 
	 * CSSM_KEYBLOB_RAW_FORMAT_NONE is specified as the default input param.
	 *
	 * All BinaryKeys must handle the special case format 
	 * CSSM_KEYBLOB_RAW_FORMAT_DIGEST, which creates a blob suitable for use
	 * in calcuating the digest of the key blob. 
	 *
	 * The session and paramKey arguments facilitate the conversion of a partial
	 * BinaryKey to a fully formed raw key, i.e., a null wrap to get a fully formed
	 * raw key. The attrFlags aregument is used to indicate that this operation
	 * did in fact convert a partial binary key to a fully formed raw key
	 * (in which case the subclass clears the CSSM_KEYATTR_PARTIAL bit
	 * in attrFlags before returning). 
	 */
	virtual void		generateKeyBlob(
		Allocator 		&allocator,
		CssmData			&blob,
		CSSM_KEYBLOB_FORMAT	&format,	// in/out, CSSM_KEYBLOB_RAW_FORMAT_PKCS1, 
										//   etc.
		AppleCSPSession		&session,
		const CssmKey		*paramKey,	// optional
		CSSM_KEYATTR_FLAGS	&attrFlags)	// IN/OUT

		{
			CssmError::throwMe(CSSMERR_CSP_FUNCTION_NOT_IMPLEMENTED); 
		}
		
	CssmKey::Header		mKeyHeader;
	KeyRef				mKeyRef;
	const CssmData		&descData()		{ return mDescData; }
	void				descData(const CssmData &inDescData) 
										{ mDescData.copy(inDescData); }
	
private:
	/* optional DescriptiveData specified by app during WrapKey */
	CssmAutoData		mDescData;
};

// Binary key representing a symmetric key.
class SymmetricBinaryKey : public BinaryKey
{
public:
	SymmetricBinaryKey(
		unsigned keySizeInBits);
	~SymmetricBinaryKey();
	void generateKeyBlob(
		Allocator 		&allocator,
		CssmData			&blob,
		CSSM_KEYBLOB_FORMAT	&format,		/* CSSM_KEYBLOB_RAW_FORMAT_PKCS1, etc. */
		AppleCSPSession		&session,
		const CssmKey		*paramKey,		/* optional, unused here */
		CSSM_KEYATTR_FLAGS 	&attrFlags);	/* IN/OUT */

	CssmData			mKeyData;
	Allocator 		&mAllocator;
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

