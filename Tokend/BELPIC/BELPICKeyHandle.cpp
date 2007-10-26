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
 *  BELPICKeyHandle.cpp
 *  TokendMuscle
 */

#include "BELPICKeyHandle.h"

#include "BELPICRecord.h"
#include "BELPICToken.h"

#include <security_utilities/debugging.h>
#include <security_utilities/utilities.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <Security/cssmerr.h>


//
// BELPICKeyHandle
//
BELPICKeyHandle::BELPICKeyHandle(BELPICToken &belpicToken,
	const Tokend::MetaRecord &metaRecord, BELPICKeyRecord &cacKey) :
	Tokend::KeyHandle(metaRecord, &cacKey),
	mToken(belpicToken), mKey(cacKey)
{
}

BELPICKeyHandle::~BELPICKeyHandle()
{
}

void BELPICKeyHandle::getKeySize(CSSM_KEY_SIZE &keySize)
{
	secdebug("crypto", "getKeySize");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

uint32 BELPICKeyHandle::getOutputSize(const Context &context,
	uint32 inputSize, bool encrypting)
{
	secdebug("crypto", "getOutputSize");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
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

void BELPICKeyHandle::generateSignature(const Context &context,
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

		// @@@ Fix me
		//CssmError::throwMe(CSSMERR_CSP_BLOCK_SIZE_MISMATCH);
	}
	else
		CssmError::throwMe(CSSMERR_CSP_INVALID_DIGEST_ALGORITHM);

#if 0
	// @@@ Hack for BELPIC card!
	header = NULL;
	headerLength = 0;
#endif

	// Create an input buffer in which we construct the data we will send to
	// the token.
	size_t inputDataSize = headerLength + input.Length;
	size_t keyLength = mKey.sizeInBits() / 8;
	auto_array<unsigned char> inputData(keyLength);
	unsigned char *to = inputData.get();

	// Get padding, but default to pkcs1 style padding
	uint32 padding = CSSM_PADDING_PKCS1;
	context.getInt(CSSM_ATTRIBUTE_PADDING, padding);

#if 1
	if (padding != CSSM_PADDING_PKCS1)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PADDING);
#else
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
#endif

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
		const AccessCredentials *cred = context.get<const AccessCredentials>(
			CSSM_ATTRIBUTE_ACCESS_CREDENTIALS);
		// Sign the inputData using the token
		mKey.computeCrypt(mToken, true, cred, inputData.get(), inputDataSize,
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

void BELPICKeyHandle::verifySignature(const Context &context,
	CSSM_ALGORITHMS signOnly, const CssmData &input, const CssmData &signature)
{
	secdebug("crypto", "verifySignature");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void BELPICKeyHandle::generateMac(const Context &context,
	const CssmData &input, CssmData &output)
{
	secdebug("crypto", "generateMac");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void BELPICKeyHandle::verifyMac(const Context &context,
	const CssmData &input, const CssmData &compare)
{
	secdebug("crypto", "verifyMac");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void BELPICKeyHandle::encrypt(const Context &context,
	const CssmData &clear, CssmData &cipher)
{
	secdebug("crypto", "encrypt");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void BELPICKeyHandle::decrypt(const Context &context,
	const CssmData &cipher, CssmData &clear)
{
	secdebug("crypto", "decrypt alg: %u", context.algorithm());
	CssmError::throwMe(CSSMERR_CSP_KEY_USAGE_INCORRECT);
}

void BELPICKeyHandle::exportKey(const Context &context,
	const AccessCredentials *cred, CssmKey &wrappedKey)
{
	secdebug("crypto", "exportKey");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}


//
// BELPICKeyHandleFactory
//
BELPICKeyHandleFactory::~BELPICKeyHandleFactory()
{
}


Tokend::KeyHandle *BELPICKeyHandleFactory::keyHandle(
	Tokend::TokenContext *tokenContext, const Tokend::MetaRecord &metaRecord,
	Tokend::Record &record) const
{
	BELPICKeyRecord &key = dynamic_cast<BELPICKeyRecord &>(record);
	BELPICToken &belpicToken = static_cast<BELPICToken &>(*tokenContext);
	return new BELPICKeyHandle(belpicToken, metaRecord, key);
}

