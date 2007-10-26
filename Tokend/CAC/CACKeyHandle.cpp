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
 *  CACKeyHandle.cpp
 *  TokendMuscle
 */

#include "CACKeyHandle.h"

#include "CACRecord.h"
#include "CACToken.h"

#include <security_utilities/debugging.h>
#include <security_utilities/utilities.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <Security/cssmerr.h>


//
// CACKeyHandle
//
CACKeyHandle::CACKeyHandle(CACToken &cacToken,
	const Tokend::MetaRecord &metaRecord, CACKeyRecord &cacKey) :
	Tokend::KeyHandle(metaRecord, &cacKey),
	mToken(cacToken),
	mKey(cacKey)
{
}

CACKeyHandle::~CACKeyHandle()
{
}

void CACKeyHandle::getKeySize(CSSM_KEY_SIZE &keySize)
{
	secdebug("crypto", "getKeySize");
	keySize.LogicalKeySizeInBits = mKey.sizeInBits();		// Logical key size in bits
	keySize.EffectiveKeySizeInBits = mKey.sizeInBits();		// Effective key size in bits
}

uint32 CACKeyHandle::getOutputSize(const Context &context, uint32 inputSize,
	bool encrypting)
{
	secdebug("crypto", "getOutputSize");
	if (encrypting)
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
	return inputSize;       //accurate for crypto used on CAC cards
}

static const unsigned char sha1sigheader[] =
{
	0x30, // SEQUENCE
	0x21, // LENGTH
	  0x30, // SEQUENCE
	  0x09, // LENGTH
		0x06, 0x05, 0x2B, 0x0E, 0x03, 0x02, 0x1a, // SHA1 OID (1 4 14 3 2 26)
	    0x05, 0x00, // OPTIONAL ANY algorithm params (NULL)
	  0x04, 0x14 // OCTECT STRING (20 bytes)
};

static const unsigned char md5sigheader[] =
{
	0x30, // SEQUENCE
	0x20, // LENGTH
	  0x30, // SEQUENCE
	  0x0C, // LENGTH
		// MD5 OID (1 2 840 113549 2 5)
	    0x06, 0x08, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x02, 0x05,
	    0x05, 0x00, // OPTIONAL ANY algorithm params (NULL)
	  0x04, 0x10 // OCTECT STRING (16 bytes)
};

void CACKeyHandle::generateSignature(const Context &context,
	CSSM_ALGORITHMS signOnly, const CssmData &input, CssmData &signature)
{
	secdebug("crypto", "generateSignature alg: %u signOnly: %u",
		context.algorithm(), signOnly);
	IFDUMPING("crypto", context.dump("signature context"));

	if (context.type() != CSSM_ALGCLASS_SIGNATURE)
		CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);

	if (context.algorithm() != CSSM_ALGID_RSA)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);

	// Find out if we are doing a SHA1 or MD5 signature and setup header to
	// point to the right asn1 blob.
	const unsigned char *header;
	size_t headerLength;
	if (signOnly == CSSM_ALGID_SHA1)
	{
		if (input.Length != 20)
			CssmError::throwMe(CSSMERR_CSP_BLOCK_SIZE_MISMATCH);

		header = sha1sigheader;
		headerLength = sizeof(sha1sigheader);
	}
	else if (signOnly == CSSM_ALGID_MD5)
	{
		if (input.Length != 16)
			CssmError::throwMe(CSSMERR_CSP_BLOCK_SIZE_MISMATCH);

		header = md5sigheader;
		headerLength = sizeof(md5sigheader);
	}
	else if (signOnly == CSSM_ALGID_NONE)
	{
		// Special case used by SSL it's an RSA signature, without the ASN1
		// stuff
		header = NULL;
		headerLength = 0;
	}
	else
		CssmError::throwMe(CSSMERR_CSP_INVALID_DIGEST_ALGORITHM);

	// Create an input buffer in which we construct the data we will send to
	// the token.
	size_t inputDataSize = headerLength + input.Length;
	size_t keyLength = mKey.sizeInBits() / 8;
	auto_array<unsigned char> inputData(keyLength);
	unsigned char *to = inputData.get();

	// Get padding, but default to pkcs1 style padding
	uint32 padding = CSSM_PADDING_PKCS1;
	context.getInt(CSSM_ATTRIBUTE_PADDING, padding);

	if (padding == CSSM_PADDING_PKCS1)
	{
		// Add PKCS1 style padding
		*(to++) = 0;
		*(to++) = 1; /* Private Key Block Type. */
		size_t padLength = keyLength - 3 - inputDataSize;
		memset(to, 0xff, padLength);
		to += padLength;
		*(to++) = 0;
		inputDataSize = keyLength;
	}
	else if (padding == CSSM_PADDING_NONE)
	{
		// Token will fail if the input data isn't exactly keysize / 8 octects
		// long
	}
	else
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PADDING);

	// Now copy the ASN1 header into the input buffer.
	// This header is the DER encoding of
	// DigestInfo ::= SEQUENCE { digestAlgorithm AlgorithmIdentifier,
	// digest OCTET STRING }
	// Where AlgorithmIdentifier ::= SEQUENCE { algorithm OBJECT IDENTIFIER,
	// parameters OPTIONAL ANY }
	if (headerLength)
	{
		memcpy(to, header, headerLength);
		to += headerLength;
	}

	// Finally copy the passed in data to the input buffer.
	memcpy(to, input.Data, input.Length);

	// @@@ Switch to using tokend allocators
	unsigned char *outputData =
		reinterpret_cast<unsigned char *>(malloc(keyLength));
	size_t outputLength = keyLength;
	try
	{
		// Sign the inputData using the token
		mKey.computeCrypt(mToken, true, inputData.get(), inputDataSize,
			outputData, outputLength);
	}
	catch (...)
	{
		// @@@ Switch to using tokend allocators
		free(outputData);
		throw;
	}

	signature.Data = outputData;
	signature.Length = outputLength;
}

void CACKeyHandle::verifySignature(const Context &context,
	CSSM_ALGORITHMS signOnly, const CssmData &input, const CssmData &signature)
{
	secdebug("crypto", "verifySignature");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void CACKeyHandle::generateMac(const Context &context,
	const CssmData &input, CssmData &output)
{
	secdebug("crypto", "generateMac");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void CACKeyHandle::verifyMac(const Context &context,
	const CssmData &input, const CssmData &compare)
{
	secdebug("crypto", "verifyMac");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void CACKeyHandle::encrypt(const Context &context,
	const CssmData &clear, CssmData &cipher)
{
	secdebug("crypto", "encrypt");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void CACKeyHandle::decrypt(const Context &context,
	const CssmData &cipher, CssmData &clear)
{
	secdebug("crypto", "decrypt alg: %u", context.algorithm());
	IFDUMPING("crypto", context.dump("decrypt context"));

	if (context.type() != CSSM_ALGCLASS_ASYMMETRIC)
		CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);

	if (context.algorithm() != CSSM_ALGID_RSA)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);

	size_t keyLength = mKey.sizeInBits() / 8;
	if (cipher.length() % keyLength != 0)
		CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);

	// @@@ Add support for multiples of keyLength by doing multiple blocks
	if (cipher.length() != keyLength)
		CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);

	// @@@ Use a secure allocator for this.
	auto_array<uint8> outputData(keyLength);
	uint8 *output = outputData.get();
	size_t outputLength = keyLength;

	secdebug("crypto", "decrypt: card supports RSA_NOPAD");
	// Decrypt the inputData using the token
	mKey.computeCrypt(mToken, false, cipher.Data, cipher.Length, output,
		outputLength);

	// Now check for proper  pkcs1 type 2 padding and remove it.
	if (outputLength != keyLength || *(output++) != 0 || *(output++) != 2)
		CssmError::throwMe(CSSMERR_CSP_INVALID_DATA);

	/* Skip over padding data */
	// We already skiped the 00 02 at the start of the block.
	outputLength -= 2;
	size_t padSize;
	for (padSize = 0; padSize < outputLength; ++padSize)
		if (*(output++) == 0) break;

	if (padSize == outputLength || padSize < 8)
		CssmError::throwMe(CSSMERR_CSP_INVALID_DATA);

	/* Don't count the 00 at the end of the padding. */
	outputLength -= padSize + 1;

	// @@@ Switch to using tokend allocators
	clear.Data = reinterpret_cast<uint8 *>(malloc(outputLength));
	// Finally copy the result into the clear buffer and set the length.
	memcpy(clear.Data, output, outputLength);
	clear.Length = outputLength;
}

void CACKeyHandle::exportKey(const Context &context,
	const AccessCredentials *cred, CssmKey &wrappedKey)
{
	secdebug("crypto", "exportKey");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}


//
// CACKeyHandleFactory
//
CACKeyHandleFactory::~CACKeyHandleFactory()
{
}


Tokend::KeyHandle *CACKeyHandleFactory::keyHandle(
	Tokend::TokenContext *tokenContext, const Tokend::MetaRecord &metaRecord,
	Tokend::Record &record) const
{
	CACKeyRecord &key = dynamic_cast<CACKeyRecord &>(record);
	CACToken &cacToken = static_cast<CACToken &>(*tokenContext);
	return new CACKeyHandle(cacToken, metaRecord, key);
}


