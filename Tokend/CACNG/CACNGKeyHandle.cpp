/*
 *  Copyright (c) 2004 Apple Computer, Inc. All Rights Reserved.
 * 
 *  @APPLE_LICENSE_HEADER_START@
 *  
 *  This file contains Original Code and/or Modifications of Original Code
 *  as defined in and that are subject to the Apple Public Source License
 *  Version 2.0 (the 'License'). You may not use this file except in
 *  compliance with the License. Please obtain a copy of the License at
 *  http://www.opensource.apple.com/apsl/ and read it before using this
 *  file.
 *  
 *  The Original Code and all software distributed under the License are
 *  distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 *  EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 *  INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 *  Please see the License for the specific language governing rights and
 *  limitations under the License.
 *  
 *  @APPLE_LICENSE_HEADER_END@
 */

/*
 *  CACNGKeyHandle.cpp
 *  TokendMuscle
 */

#include "CACNGKeyHandle.h"

#include "CACNGRecord.h"
#include "CACNGToken.h"

#include "byte_string.h"
#include "Padding.h"

#include <security_utilities/debugging.h>
#include <security_utilities/utilities.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <Security/cssmerr.h>


//
// CACNGKeyHandle
//
CACNGKeyHandle::CACNGKeyHandle(CACNGToken &cacToken,
	const Tokend::MetaRecord &metaRecord, CACNGKeyRecord &cacKey) :
	Tokend::KeyHandle(metaRecord, &cacKey),
	mToken(cacToken),
	mKey(cacKey)
{
}

CACNGKeyHandle::~CACNGKeyHandle()
{
}

void CACNGKeyHandle::getKeySize(CSSM_KEY_SIZE &keySize)
{
	secdebug("crypto", "getKeySize");
	keySize.LogicalKeySizeInBits = mKey.sizeInBits();		// Logical key size in bits
	keySize.EffectiveKeySizeInBits = mKey.sizeInBits();		// Effective key size in bits
}

uint32 CACNGKeyHandle::getOutputSize(const Context &context, uint32 inputSize,
	bool encrypting)
{
	secdebug("crypto", "getOutputSize");
	if (encrypting)
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	return inputSize;       //accurate for crypto used on CACNG cards
}

void CACNGKeyHandle::generateSignature(const Context &context,
	CSSM_ALGORITHMS alg, const CssmData &input, CssmData &signature)
{
	secdebug("crypto", "generateSignature alg: %u signOnly: %u",
		context.algorithm(), alg);
	IFDUMPING("crypto", context.dump("signature context"));

	if (context.type() != CSSM_ALGCLASS_SIGNATURE)
		CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);

	if (context.algorithm() != CSSM_ALGID_RSA)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);

	// Get padding, but default to pkcs1 style padding
	uint32 padding = CSSM_PADDING_PKCS1;
	context.getInt(CSSM_ATTRIBUTE_PADDING, padding);
	
	byte_string inputData(input.Data, input.Data + input.Length);

	Padding::apply(inputData, mKey.sizeInBits() / 8, padding, alg);

	// @@@ Switch to using tokend allocators
	byte_string outputData(mKey.sizeInBits() / 8);
	size_t outputLength = outputData.size();

	// Sign the inputData using the token
	mKey.computeCrypt(mToken, true, &inputData[0], inputData.size(),
		&outputData[0], outputLength);

	signature.Data = malloc_copy(outputData);
	signature.Length = outputLength;
}

void CACNGKeyHandle::verifySignature(const Context &context,
	CSSM_ALGORITHMS signOnly, const CssmData &input, const CssmData &signature)
{
	secdebug("crypto", "verifySignature");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void CACNGKeyHandle::generateMac(const Context &context,
	const CssmData &input, CssmData &output)
{
	secdebug("crypto", "generateMac");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void CACNGKeyHandle::verifyMac(const Context &context,
	const CssmData &input, const CssmData &compare)
{
	secdebug("crypto", "verifyMac");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void CACNGKeyHandle::encrypt(const Context &context,
	const CssmData &clear, CssmData &cipher)
{
	secdebug("crypto", "encrypt");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void CACNGKeyHandle::decrypt(const Context &context,
	const CssmData &cipher, CssmData &clear)
{
	secdebug("crypto", "decrypt alg: %u", context.algorithm());
	IFDUMPING("crypto", context.dump("decrypt context"));

	if (context.type() != CSSM_ALGCLASS_ASYMMETRIC)
		CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);

	if (context.algorithm() != CSSM_ALGID_RSA)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);

	/* Check for supported padding */
	uint32 padding = context.getInt(CSSM_ATTRIBUTE_PADDING);
	if(!Padding::canRemove(padding))
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PADDING);

	size_t keyLength = mKey.sizeInBits() / 8;
	if (cipher.length() % keyLength != 0)
		CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);

	// @@@ Add support for multiples of keyLength by doing multiple blocks
	if (cipher.length() != keyLength)
		CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);

	// @@@ Use a secure allocator for this.
	byte_string outputData(keyLength);
	uint8 *output = &outputData[0];
	size_t outputLength = keyLength;

	secdebug("crypto", "decrypt: card supports RSA_NOPAD");
	// Decrypt the inputData using the token
	mKey.computeCrypt(mToken, false, cipher.Data, cipher.Length, output,
		outputLength);

	if (outputLength != keyLength)
		CssmError::throwMe(CSSMERR_CSP_INVALID_DATA);
	Padding::remove(outputData, padding);

	// @@@ Switch to using tokend allocators
	clear.Data = malloc_copy(outputData);
	clear.Length = outputData.size();
}

void CACNGKeyHandle::exportKey(const Context &context,
	const AccessCredentials *cred, CssmKey &wrappedKey)
{
	secdebug("crypto", "exportKey");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void CACNGKeyHandle::getAcl(const char *tag, uint32 &count, AclEntryInfo *&aclList)
{
	mKey.getAcl(tag, count, aclList);
}

void CACNGKeyHandle::getOwner(AclOwnerPrototype &owner)
{
	mKey.getOwner(owner);
}

//
// CACNGKeyHandleFactory
//
CACNGKeyHandleFactory::~CACNGKeyHandleFactory()
{
}


Tokend::KeyHandle *CACNGKeyHandleFactory::keyHandle(
	Tokend::TokenContext *tokenContext, const Tokend::MetaRecord &metaRecord,
	Tokend::Record &record) const
{
	CACNGKeyRecord &key = dynamic_cast<CACNGKeyRecord &>(record);
	CACNGToken &cacToken = static_cast<CACNGToken &>(*tokenContext);
	return new CACNGKeyHandle(cacToken, metaRecord, key);
}


