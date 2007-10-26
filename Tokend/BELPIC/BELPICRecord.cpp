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
 *  BELPICRecord.cpp
 *  TokendMuscle
 */

#include "BELPICRecord.h"

#include "BELPICError.h"
#include "BELPICToken.h"
#include "Attribute.h"
#include "MetaAttribute.h"
#include "MetaRecord.h"
#include <security_cdsa_client/aclclient.h>
#include <Security/SecKey.h>


//
// BELPICRecord
//
BELPICRecord::~BELPICRecord()
{
}

//
// BELPICCertificateRecord
//
BELPICBinaryFileRecord::~BELPICBinaryFileRecord()
{
}

#define BELPIC_MAXSIZE_CERT           4000

Tokend::Attribute *BELPICBinaryFileRecord::getDataAttribute(
	Tokend::TokenContext *tokenContext)
{
	CssmData data;
	BELPICToken &belpicToken = static_cast<BELPICToken &>(*tokenContext);
	if (belpicToken.cachedObject(0, mDescription, data))
	{
		Tokend::Attribute *attribute = new Tokend::Attribute(data.Data,
			data.Length);
		free(data.Data);
		return attribute;
	}

	PCSC::Transaction _(belpicToken);
	belpicToken.select(mDF, mEF);

	uint8 certificate[BELPIC_MAXSIZE_CERT];
	size_t certificateLength = sizeof(certificate);
	belpicToken.readBinary(certificate, certificateLength);
	data.Data = certificate;
	data.Length = certificateLength;
	belpicToken.cacheObject(0, mDescription, data);

	return new Tokend::Attribute(data.Data, data.Length);
}


//
// BELPICKeyRecord
//
BELPICKeyRecord::BELPICKeyRecord(const uint8_t *keyId,
	const char *description, const Tokend::MetaRecord &metaRecord,
	bool signOnly) :
	BELPICRecord(description),
	mKeyId(keyId),
	mSignOnly(signOnly)
{
    attributeAtIndex(metaRecord.metaAttribute(kSecKeyDecrypt).attributeIndex(),
                     new Tokend::Attribute(!signOnly));
    attributeAtIndex(metaRecord.metaAttribute(kSecKeyUnwrap).attributeIndex(),
                     new Tokend::Attribute(!signOnly));
    attributeAtIndex(metaRecord.metaAttribute(kSecKeySign).attributeIndex(),
                     new Tokend::Attribute(signOnly));
}

BELPICKeyRecord::~BELPICKeyRecord()
{
}

void BELPICKeyRecord::computeCrypt(BELPICToken &belpicToken, bool sign,
	const AccessCredentials *cred, const unsigned char *data,
	size_t dataLength, unsigned char *output, size_t &outputLength)
{
	PCSC::Transaction _(belpicToken);
	belpicToken.selectKeyForSign(mKeyId);

	if (cred)
	{
		uint32 size = cred->size();
		for (uint32 ix = 0; ix < size; ++ix)
		{
			const TypedList &sample = (*cred)[ix];
			if (sample.type() == CSSM_SAMPLE_TYPE_PROMPTED_PASSWORD
                && sample.length() == 2)
            {
                CssmData &pin = sample[1].data();
                if (pin.Length >= BELPIC_MIN_PIN_LEN &&
                    pin.Length <= BELPIC_MAX_PIN_LEN)
                {
                    belpicToken._verifyPIN(1, pin.Data, pin.Length);
                    break;
                }
                else if (pin.Length == 0)
                {
                    // %%% <rdar://4334623>
                    // PIN previously verified by securityd;
                    // continue to look at remaining samples
                }
                else
                {
                    CssmError::throwMe(CSSM_ERRCODE_SAMPLE_VALUE_NOT_SUPPORTED);
                }
            }
        }

	}

	if (dataLength > sizeInBits() / 8)
		CssmError::throwMe(CSSMERR_CSP_BLOCK_SIZE_MISMATCH);

	if (sign != mSignOnly)
		CssmError::throwMe(CSSMERR_CSP_KEY_USAGE_INCORRECT);

	size_t apduSize = dataLength + 5;
	unsigned char apdu[apduSize];
	size_t resultLength = sizeInBits() / 8 + 2;
	unsigned char result[resultLength];

	apdu[0] = 0x00;
	apdu[1] = 0x2A;
	apdu[2] = 0x9E;
	apdu[3] = 0x9A;
	apdu[4] = dataLength;
	memcpy(apdu + 5, data, dataLength);
	BELPICError::check(belpicToken.exchangeAPDU(apdu, apduSize, result,
		resultLength));
	if (resultLength != sizeInBits() / 8 + 2)
	{
		secdebug("cac", " %s: computeCrypt: expected size: %ld, got: %ld",
			mDescription, sizeInBits() / 8 + 2, resultLength);
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
	}

	if (outputLength < resultLength - 2)
		CssmError::throwMe(CSSMERR_CSP_BLOCK_SIZE_MISMATCH);

	outputLength = resultLength - 2;
	memcpy(output, result, outputLength);
}

void BELPICKeyRecord::getAcl(const char *tag, uint32 &count,
	AclEntryInfo *&acls)
{
	// @@@ Key 1 has any acl for sign, key 2 has pin1 acl, and key3 has pin1
	// acl with auto-lock which we express as a prompted password subject.
	if (!mAclEntries) {
		mAclEntries.allocator(Allocator::standard());
        // Anyone can read the DB record for this key (which is a reference
		// CSSM_KEY)
        mAclEntries.add(CssmClient::AclFactory::AnySubject(
			mAclEntries.allocator()),
			AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_DB_READ, 0));
        // Setup the remainder of the acl based on the key type.
		if (*mKeyId == 0x82)
		{
			mAclEntries.add(CssmClient::AclFactory::PinSubject(
				mAclEntries.allocator(), 1),
				AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_SIGN, 0));
		}
		else if (*mKeyId == 0x83)
		{
			CssmData prompt;
			mAclEntries.add(CssmClient::AclFactory::PromptPWSubject(
				mAclEntries.allocator(), prompt),
				AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_SIGN, 0));
		}
	}
	count = mAclEntries.size();
	acls = mAclEntries.entries();
}

