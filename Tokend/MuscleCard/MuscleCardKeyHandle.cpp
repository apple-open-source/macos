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
 *  MuscleCardKeyHandle.cpp
 *  TokendMuscle
 */

#include "MuscleCardKeyHandle.h"

#include "KeyRecord.h"
#include "Msc/MscError.h"
#include "Msc/MscKey.h"
#include "Msc/MscToken.h"

#include <security_utilities/debugging.h>
#include <security_utilities/utilities.h>
#include <security_cdsa_utilities/cssmerrors.h>
#include <security_cdsa_client/aclclient.h>
#include <Security/cssmerr.h>

using CssmClient::AclFactory;


//
// MuscleCardKeyHandle
//
MuscleCardKeyHandle::MuscleCardKeyHandle(const Tokend::MetaRecord &metaRecord,
	Tokend::Record &record, MscKey &key) :
	Tokend::KeyHandle(metaRecord, &record),
	mKey(key)
{
}

MuscleCardKeyHandle::~MuscleCardKeyHandle()
{
}

void MuscleCardKeyHandle::getKeySize(CSSM_KEY_SIZE &keySize)
{
	secdebug("crypto", "getKeySize");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

uint32 MuscleCardKeyHandle::getOutputSize(const Context &context, uint32 inputSize, bool encrypting)
{
	secdebug("crypto", "getOutputSize");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

static const MSCUChar8 sha1sigheader[] =
{
	0x30, // SEQUENCE
	0x21, // LENGTH
	  0x30, // SEQUENCE
	  0x09, // LENGTH
		0x06, 0x05, 0x2B, 0x0E, 0x03, 0x02, 0x1a, // SHA1 OID (1 4 14 3 2 26)
	    0x05, 0x00, // OPTIONAL ANY algorithm params (NULL)
	  0x04, 0x14 // OCTECT STRING (20 bytes)
};

static const MSCUChar8 md5sigheader[] =
{
	0x30, // SEQUENCE
	0x20, // LENGTH
	  0x30, // SEQUENCE
	  0x0C, // LENGTH
	    0x06, 0x08, 0x2A, 0x86, 0x48, 0x86, 0xF7, 0x0D, 0x02, 0x05, // MD5 OID (1 2 840 113549 2 5)
	    0x05, 0x00, // OPTIONAL ANY algorithm params (NULL)
	  0x04, 0x10 // OCTECT STRING (16 bytes)
};

void MuscleCardKeyHandle::generateSignature(const Context &context,
	CSSM_ALGORITHMS signOnly, const CssmData &input, CssmData &signature)
{
	secdebug("crypto", "generateSignature alg: %u signOnly: %u", context.algorithm(), signOnly);
	IFDUMPING("crypto", context.dump("signature context"));

	if (context.type() != CSSM_ALGCLASS_SIGNATURE)
		CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);

	if (context.algorithm() != CSSM_ALGID_RSA)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);

	// Find out if we are doing a SHA1 or MD5 signature and setup header to point to the right asn1 blob.
	MSCPCUChar8 header;
	MSCULong32 headerLength;
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
		// Special case used by SSL it's an RSA signature, without the ASN1 stuff
		header = NULL;
		headerLength = 0;
	}
	else
		CssmError::throwMe(CSSMERR_CSP_INVALID_DIGEST_ALGORITHM);

	// Create an input buffer in which we construct the data we will send to the token.
	MSCUChar8 cipherMode;
	MSCULong32 inputDataSize = headerLength + input.Length;
	MSCULong32 keyLength = mKey.size() / 8;
	auto_array<MSCUChar8> inputData(keyLength);
	MSCPUChar8 to = inputData.get();

	// Get padding, but default to pkcs1 style padding
	uint32 padding = CSSM_PADDING_PKCS1;
	context.getInt(CSSM_ATTRIBUTE_PADDING, padding);

	// Figure out whether the underlying token supports RSA_NOPAD, if so we generate our own padding if not,
	// we let the card do the PKCS1 padding itself.
	MSCULong32 rsaCapabilities = mKey.connection().getCapabilities(MSC_TAG_CAPABLE_RSA);
	if (rsaCapabilities & MSC_CAPABLE_RSA_NOPAD)
	{
		secdebug("crypto", "generateSignature: card supports RSA_NOPAD");
		cipherMode = MSC_MODE_RSA_NOPAD;

		if (padding == CSSM_PADDING_PKCS1)
		{
			// Add PKCS1 style padding
			*(to++) = 0;
			*(to++) = 1; /* Private Key Block Type. */
			MSCULong32 padLength = keyLength - 3 - inputDataSize;
			memset(to, 0xff, padLength);
			to += padLength;
			*(to++) = 0;
			inputDataSize = keyLength;
		}
		else if (padding == CSSM_PADDING_NONE)
		{
			// Token will fail if the input data isn't exactly keysize / 8 octects long
		}
		else
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PADDING);
	}
	else if (rsaCapabilities & MSC_CAPABLE_RSA_PKCS1)
	{
		if (padding != CSSM_PADDING_PKCS1)
			CssmError::throwMe(CSSMERR_CSP_INVALID_ATTR_PADDING);

		secdebug("crypto", "generateSignature: card only supports RSA_PKCS1");
		cipherMode = MSC_MODE_RSA_PAD_PKCS1;
	}
	else
	{
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED); // @@@ Look for a better error.
	}

	// Now copy the ASN1 header into the input buffer.
	// This header is the DER encoding of
	// DigestInfo ::= SEQUENCE { digestAlgorithm AlgorithmIdentifier, digest OCTET STRING }
	// Where AlgorithmIdentifier ::= SEQUENCE { algorithm OBJECT IDENTIFIER, parameters OPTIONAL ANY }
	if (headerLength)
	{
		memcpy(to, header, headerLength);
		to += headerLength;
	}

	// Finally copy the passed in data to the input buffer.
	memcpy(to, input.Data, input.Length);

	// @@@ Switch to using tokend allocators
	MSCPUChar8 outputData = reinterpret_cast<MSCPUChar8>(malloc(keyLength));
	size_t outputLength = keyLength;
	try
	{
		// Sign the inputData using the token
		mKey.computeCrypt(cipherMode, MSC_DIR_SIGN, inputData.get(), inputDataSize, outputData, outputLength);
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

void MuscleCardKeyHandle::verifySignature(const Context &context,
	CSSM_ALGORITHMS signOnly, const CssmData &input, const CssmData &signature)
{
	secdebug("crypto", "verifySignature");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void MuscleCardKeyHandle::generateMac(const Context &context,
	const CssmData &input, CssmData &output)
{
	secdebug("crypto", "generateMac");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void MuscleCardKeyHandle::verifyMac(const Context &context,
	const CssmData &input, const CssmData &compare)
{
	secdebug("crypto", "verifyMac");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void MuscleCardKeyHandle::encrypt(const Context &context,
	const CssmData &clear, CssmData &cipher)
{
	secdebug("crypto", "encrypt");
	CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED);
}

void MuscleCardKeyHandle::decrypt(const Context &context,
	const CssmData &cipher, CssmData &clear)
{
	secdebug("crypto", "decrypt alg: %u", context.algorithm());
	IFDUMPING("crypto", context.dump("decrypt context"));

	if (context.type() != CSSM_ALGCLASS_ASYMMETRIC)
		CssmError::throwMe(CSSMERR_CSP_INVALID_CONTEXT);

	if (context.algorithm() != CSSM_ALGID_RSA)
		CssmError::throwMe(CSSMERR_CSP_INVALID_ALGORITHM);

	size_t keyLength = mKey.size() / 8;
	if (cipher.length() % keyLength != 0)
		CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);

	// @@@ Add support for multiples of keyLength by doing multiple blocks
	if (cipher.length() != keyLength)
		CssmError::throwMe(CSSMERR_CSP_INPUT_LENGTH_ERROR);

	// @@@ Use a secure allocator for this.
	auto_array<uint8> outputData(keyLength);
	uint8 *output = outputData.get();
	size_t outputLength = keyLength;

	// Figure out whether the underlying token supports RSA_NOPAD, if so we remove the padding
	// ourselves if not, we let the card remove the PKCS1 padding.
	MSCULong32 rsaCapabilities = mKey.connection().getCapabilities(MSC_TAG_CAPABLE_RSA);
	if (rsaCapabilities & MSC_CAPABLE_RSA_NOPAD)
	{
		secdebug("crypto", "decrypt: card supports RSA_NOPAD");
		// Decrypt the inputData using the token
		mKey.computeCrypt(MSC_MODE_RSA_NOPAD, MSC_DIR_DECRYPT, cipher.Data, cipher.Length, output, outputLength);

		// Now check for proper  pkcs1 type 2 padding and remove it.
		if (outputLength != keyLength || *(output++) != 0 || *(output++) != 2)
			CssmError::throwMe(CSSMERR_CSP_INVALID_DATA);

		/* Skip over padding data */
		outputLength -= 2; // We already skiped the 00 02 at the start of the block.
		size_t padSize;
		for (padSize = 0; padSize < outputLength; ++padSize)
			if (*(output++) == 0) break;

		if (padSize == outputLength || padSize < 8)
			CssmError::throwMe(CSSMERR_CSP_INVALID_DATA);

		outputLength -= padSize + 1; /* Don't count the 00 at the end of the padding. */
	}
	else if (rsaCapabilities & MSC_CAPABLE_RSA_PKCS1)
	{
		secdebug("crypto", "generateSignature: card only supports RSA_PKCS1");
		// Decrypt the inputData using the token
		mKey.computeCrypt(MSC_MODE_RSA_PAD_PKCS1, MSC_DIR_DECRYPT, cipher.Data, cipher.Length, output, outputLength);
	}
	else
	{
		CssmError::throwMe(CSSM_ERRCODE_FUNCTION_NOT_IMPLEMENTED); // @@@ Look for a better error.
	}

	// @@@ Switch to using tokend allocators
	clear.Data = reinterpret_cast<uint8 *>(malloc(outputLength));
	// Finally copy the result into the clear buffer and set the length.
	memcpy(clear.Data, output, outputLength);
	clear.Length = outputLength;
}

void MuscleCardKeyHandle::exportKey(const Context &context, const AccessCredentials *cred,
		CssmKey &wrappedKey)
{
	wrappedKey.clearPod();
	wrappedKey.header().HeaderVersion = CSSM_KEYHEADER_VERSION;
	wrappedKey.header().cspGuid(Guid::overlay(gGuidAppleSdCSPDL));
	wrappedKey.blobType(CSSM_KEYBLOB_RAW);

	uint32_t keyType = mKey.type();
	uint32 algID;
	uint32 keyClass;
	CSSM_KEYBLOB_FORMAT format;

    switch (keyType)
	{
	case MSC_KEY_RSA_PRIVATE:
		format = CSSM_KEYBLOB_RAW_FORMAT_NONE;
		keyClass = CSSM_KEYCLASS_PRIVATE_KEY;
		algID = CSSM_ALGID_RSA;
		break;

	case MSC_KEY_RSA_PRIVATE_CRT:
		format = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
		keyClass = CSSM_KEYCLASS_PRIVATE_KEY;
		algID = CSSM_ALGID_RSA;
		break;

	case MSC_KEY_RSA_PUBLIC:
		format = CSSM_KEYBLOB_RAW_FORMAT_PKCS1;
		keyClass = CSSM_KEYCLASS_PUBLIC_KEY;
		algID = CSSM_ALGID_RSA;
		break;

	case MSC_KEY_DSA_PRIVATE:
		format = CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
		keyClass = CSSM_KEYCLASS_PRIVATE_KEY;
		algID = CSSM_ALGID_DSA;
		break;

	case MSC_KEY_DSA_PUBLIC:
		format = CSSM_KEYBLOB_RAW_FORMAT_FIPS186;
		keyClass = CSSM_KEYCLASS_PUBLIC_KEY;
		algID = CSSM_ALGID_DSA;
		break;

	case MSC_KEY_DES:
		format = CSSM_KEYBLOB_RAW_FORMAT_NONE;
		keyClass = CSSM_KEYCLASS_SESSION_KEY;
		algID = CSSM_ALGID_DES;
		break;

	case MSC_KEY_3DES:
		format = CSSM_KEYBLOB_RAW_FORMAT_NONE;
		keyClass = CSSM_KEYCLASS_SESSION_KEY;
		// @@@ Which algid is this?
		algID = CSSM_ALGID_3DES;
		//algID = CSSM_ALGID_3DES_3KEY_EDE;
		//algID = CSSM_ALGID_3DES_2KEY_EDE;
		//algID = CSSM_ALGID_3DES_1KEY_EEE;
		//algID = CSSM_ALGID_3DES_3KEY_EEE;
		//algID = CSSM_ALGID_3DES_2KEY_EEE;
		break;

	case MSC_KEY_3DES3:
		format = CSSM_KEYBLOB_RAW_FORMAT_NONE;
		keyClass = CSSM_KEYCLASS_SESSION_KEY;
		// @@@ Which algid is this?
		algID = CSSM_ALGID_3DES_3KEY_EDE;
		//algID = CSSM_ALGID_3DES_3KEY_EEE;
		break;

	default:
		format = CSSM_KEYBLOB_RAW_FORMAT_OTHER;
		keyClass = CSSM_KEYCLASS_OTHER;
		algID = CSSM_ALGID_CUSTOM;
		break;
	}

	wrappedKey.blobFormat(format);
	wrappedKey.algorithm(algID);
	wrappedKey.keyClass(keyClass);
	wrappedKey.header().LogicalKeySizeInBits = mKey.size() / 8;

	wrappedKey.header().KeyAttr = CSSM_KEYATTR_MODIFIABLE | CSSM_KEYATTR_EXTRACTABLE;

#if 0
	CSSM_KEYUSE usage =
		(mr.metaAttribute(kSecKeyEncrypt).attribute(tokenContext, record).boolValue() ? CSSM_KEYUSE_ENCRYPT : 0)
		| (mr.metaAttribute(kSecKeyDecrypt).attribute(tokenContext, record).boolValue() ? CSSM_KEYUSE_DECRYPT : 0)
		| (mr.metaAttribute(kSecKeySign).attribute(tokenContext, record).boolValue() ? CSSM_KEYUSE_SIGN : 0)
		| (mr.metaAttribute(kSecKeyVerify).attribute(tokenContext, record).boolValue() ? CSSM_KEYUSE_VERIFY : 0)
		| (mr.metaAttribute(kSecKeySignRecover).attribute(tokenContext, record).boolValue() ? CSSM_KEYUSE_SIGN_RECOVER : 0)
		| (mr.metaAttribute(kSecKeyVerifyRecover).attribute(tokenContext, record).boolValue() ? CSSM_KEYUSE_VERIFY_RECOVER : 0)
		| (mr.metaAttribute(kSecKeyWrap).attribute(tokenContext, record).boolValue() ? CSSM_KEYUSE_WRAP : 0)
		| (mr.metaAttribute(kSecKeyUnwrap).attribute(tokenContext, record).boolValue() ? CSSM_KEYUSE_UNWRAP : 0)
		| (mr.metaAttribute(kSecKeyDerive).attribute(tokenContext, record).boolValue() ? CSSM_KEYUSE_DERIVE : 0);
	if (usage == (CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_SIGN | CSSM_KEYUSE_VERIFY
		| CSSM_KEYUSE_SIGN_RECOVER | CSSM_KEYUSE_VERIFY_RECOVER
		| CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP | CSSM_KEYUSE_DERIVE))
		usage = CSSM_KEYUSE_ANY;

	wrappedKey.header().KeyUsage = usage;
#else
	wrappedKey.header().KeyUsage = CSSM_KEYUSE_ANY;
#endif

	wrappedKey.KeyData.Length = mKey.size() / 8;
	void *buffer = malloc(wrappedKey.KeyData.Length);
	wrappedKey.KeyData.Data = reinterpret_cast<uint8 *>(buffer);
	mKey.exportKey(buffer, wrappedKey.KeyData.Length);
}

void MuscleCardKeyHandle::getOwner(AclOwnerPrototype &owner)
{
	// we don't really know (right now), so claim we're owned by PIN #0
	if (!mAclOwner) {
		Allocator &alloc = Allocator::standard();
		mAclOwner.allocator(alloc);

		unsigned int acl = mKey.keyACL.readPermission;
		if (acl == MSC_AUT_NONE)
			acl = mKey.keyACL.writePermission;
		if (acl == MSC_AUT_NONE)
			acl = mKey.keyACL.usePermission;
		if (acl == MSC_AUT_NONE) {
			// nobody can do anything with this key? how useless...
			mAclOwner = AclFactory::NobodySubject(alloc);
		} else if (acl == MSC_AUT_ALL) {
			// no restrictions - an ANY ACL
			mAclOwner = AclFactory::AnySubject(alloc);
		} else {
			// we don't currently support ownership by multiple PINs:
			// pick the first one and ignore the rest
			for (unsigned n = 0; n < 5; n++)
				if (acl & (MSC_AUT_PIN_0 << n)) {
					mAclOwner = AclFactory::PinSubject(alloc, n);
					break;
				}
			// ignoring the KEY and USER bits -- whatever they might be
		}
	}
	owner = mAclOwner;
}

void MuscleCardKeyHandle::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	// we don't (yet) support queries by tag
	if (tag)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_ENTRY_TAG);

	if (!mAclEntries) {
		mAclEntries.allocator(Allocator::standard());
        // Anyone can read the DB record for this key (which is a reference CSSM_KEY)
		mAclEntries.add(CssmClient::AclFactory::AnySubject(mAclEntries.allocator()),
                        AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_DB_READ, 0));
		// READ -> unwrap (extract)
		keyAcl(mKey.keyACL.readPermission, AclAuthorizationSet(
				CSSM_ACL_AUTHORIZATION_EXPORT_CLEAR,
				CSSM_ACL_AUTHORIZATION_EXPORT_WRAPPED,
				0));
		// WRITE is currently ignored
		// USE will have to serve for all crypto operations (pity that)
		keyAcl(mKey.keyACL.usePermission, AclAuthorizationSet(
			CSSM_ACL_AUTHORIZATION_ENCRYPT,
			CSSM_ACL_AUTHORIZATION_DECRYPT,
			CSSM_ACL_AUTHORIZATION_SIGN,
			CSSM_ACL_AUTHORIZATION_MAC,
			CSSM_ACL_AUTHORIZATION_DERIVE,
			0));
	}
	count = mAclEntries.size();
	acls = mAclEntries.entries();
}

void MuscleCardKeyHandle::keyAcl(unsigned int acl, const AclAuthorizationSet &auths)
{
	Allocator &alloc = mAclEntries.allocator();
	if (acl == MSC_AUT_NONE) {
		// there's no way to do this... so say nothing
	} else if (acl == MSC_AUT_ALL) {
		// no restrictions - add an ANY ACL
		mAclEntries.add(AclFactory::AnySubject(alloc), auths);
	} else {
		// general case: for each enabling PIN, issue an ACL entry
		// (we could form a 1-of-n ACL, but that would complicate the sample set)
		for (unsigned n = 0; n < 5; n++)
			if (acl & (MSC_AUT_PIN_0 << n))
				mAclEntries.add(AclFactory::PinSubject(alloc, n), auths);
		// ignoring the KEY and USER bits -- whatever they might be
	}
}


//
// MuscleCardKeyHandleFactory
//
MuscleCardKeyHandleFactory::~MuscleCardKeyHandleFactory()
{
}


Tokend::KeyHandle *MuscleCardKeyHandleFactory::keyHandle(Tokend::TokenContext *tokenContext,
	const Tokend::MetaRecord &metaRecord, Tokend::Record &record) const
{
	KeyRecord &keyRecord = dynamic_cast<KeyRecord &>(record);			
	return new MuscleCardKeyHandle(metaRecord, record, keyRecord.key());
}


