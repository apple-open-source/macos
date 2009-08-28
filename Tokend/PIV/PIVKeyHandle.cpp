/*
 *  Copyright (c) 2004-2007 Apple Inc. All Rights Reserved.
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
 *  PIVKeyHandle.cpp
 *  TokendPIV
 */

#include "PIVKeyHandle.h"

#include "PIVRecord.h"
#include "PIVToken.h"

#include <security_utilities/debugging.h>
#include <security_utilities/utilities.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <Security/cssmerr.h>

#include "byte_string.h"

#include "PIVUtilities.h"
#include "Padding.h"

//
// PIVKeyHandle
//
PIVKeyHandle::PIVKeyHandle(PIVToken &pivToken,
	const Tokend::MetaRecord &metaRecord, PIVKeyRecord &pivKey) :
	Tokend::KeyHandle(metaRecord, &pivKey),
	mToken(pivToken),
	mKey(pivKey)
{
}

PIVKeyHandle::~PIVKeyHandle()
{
}

void PIVKeyHandle::getKeySize(CSSM_KEY_SIZE &keySize)
{
	secdebug("crypto", "getKeySize");
	keySize.LogicalKeySizeInBits = mKey.sizeInBits();
	keySize.EffectiveKeySizeInBits = mKey.sizeInBits();
}

uint32 PIVKeyHandle::getOutputSize(const Context &context, uint32 inputSize,
	bool encrypting)
{
	secdebug("crypto", "getOutputSize");
	if (encrypting)
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	return inputSize;       //accurate for crypto used on PIV cards
}


void PIVKeyHandle::generateSignature(const Context &context,
	CSSM_ALGORITHMS alg, const CssmData &input, CssmData &signature)
{
	// MODIFY: This routine may have to be modified
	// See comment at top of file
	secdebug("crypto", "generateSignature alg: %u sigAlg: %u",
		context.algorithm(), alg);
	IFDUMPING("crypto", context.dump("signature context"));

	if (context.type() != CSSM_ALGCLASS_SIGNATURE)
		CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);

	if (context.algorithm() != CSSM_ALGID_RSA)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);

	// Create an input buffer in which we construct the data we will send to the token.
	byte_string inputData(input.Data, input.Data + input.Length);

	// Get padding, but default to pkcs1 style padding
	uint32 padding = CSSM_PADDING_PKCS1;
	context.getInt(CSSM_ATTRIBUTE_PADDING, padding);

	Padding::apply(inputData, mKey.sizeInBits() / 8, padding, alg);

	// @@@ Switch to using tokend allocators
	/* Use ref to a new buffer item to keep the data around after the function ends */
	size_t keyLength = mKey.sizeInBits() / 8;
	byte_string outputData;
	outputData.reserve(keyLength);

	const AccessCredentials *cred = context.get<const AccessCredentials>(CSSM_ATTRIBUTE_ACCESS_CREDENTIALS);
	// Sign the inputData using the token
	mKey.computeCrypt(mToken, true, cred, inputData, outputData);

	signature.Data = malloc_copy(outputData);
	signature.Length = outputData.size();
}

void PIVKeyHandle::verifySignature(const Context &context,
	CSSM_ALGORITHMS signOnly, const CssmData &input, const CssmData &signature)
{
	secdebug("crypto", "verifySignature");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void PIVKeyHandle::generateMac(const Context &context,
	const CssmData &input, CssmData &output)
{
	secdebug("crypto", "generateMac");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void PIVKeyHandle::verifyMac(const Context &context,
	const CssmData &input, const CssmData &compare)
{
	secdebug("crypto", "verifyMac");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void PIVKeyHandle::encrypt(const Context &context,
	const CssmData &clear, CssmData &cipher)
{
	secdebug("crypto", "encrypt");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void PIVKeyHandle::decrypt(const Context &context,
	const CssmData &cipher, CssmData &clear)
{
	// MODIFY: This routine may have to be modified
	// See comment at top of file
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

	// @@@ Use a secure allocator for this.
	/* Use ref to a new buffer item to keep the data around after the function ends */
	byte_string outputData;
	outputData.reserve(cipher.Length);
	// --- support for multiples of keyLength by doing multiple blocks
	for(size_t i = 0; i < cipher.Length; i += keyLength) {
		byte_string inputData(cipher.Data + i, cipher.Data + i + keyLength);
		byte_string tmpOutput;
		tmpOutput.reserve(keyLength);
		secdebug("crypto", "decrypt: card supports RSA_NOPAD");
		const AccessCredentials *cred = context.get<const AccessCredentials>(CSSM_ATTRIBUTE_ACCESS_CREDENTIALS);
		// Decrypt the inputData using the token
		mKey.computeCrypt(mToken, false, cred, inputData, tmpOutput);
		Padding::remove(tmpOutput, padding);
		outputData += tmpOutput;
		/* Clear out temporary output */
		secure_zero(tmpOutput);
	}

	clear.Data = malloc_copy(outputData);
	clear.Length = outputData.size();
}

void PIVKeyHandle::exportKey(const Context &context,
	const AccessCredentials *cred, CssmKey &wrappedKey)
{
	secdebug("crypto", "exportKey");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

//
// PIVKeyHandleFactory
//
PIVKeyHandleFactory::~PIVKeyHandleFactory()
{
}


Tokend::KeyHandle *PIVKeyHandleFactory::keyHandle(
	Tokend::TokenContext *tokenContext, const Tokend::MetaRecord &metaRecord,
	Tokend::Record &record) const
{
	PIVKeyRecord &key = dynamic_cast<PIVKeyRecord &>(record);
	PIVToken &pivToken = static_cast<PIVToken &>(*tokenContext);
	return new PIVKeyHandle(pivToken, metaRecord, key);
}

