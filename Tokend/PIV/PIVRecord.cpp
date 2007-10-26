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
 *  PIVRecord.cpp
 *  TokendPIV
 */

#include "PIVRecord.h"
#include "PIVDefines.h"

#include "PIVError.h"
#include "PIVToken.h"
#include "Attribute.h"
#include "MetaAttribute.h"
#include "MetaRecord.h"
#include <security_cdsa_client/aclclient.h>
#include <Security/SecKey.h>

//
// PIVRecord
//
PIVRecord::~PIVRecord()
{
}

//
// PIVDataRecord
//
PIVDataRecord::~PIVDataRecord()
{
}


//
// PIVCertificateRecord
//
PIVCertificateRecord::~PIVCertificateRecord()
{
}

//
// PIVProtectedRecord
//
PIVProtectedRecord::~PIVProtectedRecord()
{
}

void PIVProtectedRecord::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	if (!mAclEntries) {
		mAclEntries.allocator(Allocator::standard());
        // Reading this object's data requires PIN1
		mAclEntries.add(CssmClient::AclFactory::PinSubject(
			mAclEntries.allocator(), 1),
			AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_DB_READ, 0));
	}
	count = mAclEntries.size();
	acls = mAclEntries.entries();
}

Tokend::Attribute *PIVDataRecord::getDataAttribute(Tokend::TokenContext *tokenContext)
{
	PIVToken &pivToken = dynamic_cast<PIVToken &>(*tokenContext);
	CssmData data;
	
	pivToken.getDataCore(mApplication, mApplicationSize, mDescription, mIsCertificate, mAllowCaching, data);
	
	return new Tokend::Attribute(data.Data, data.Length);
}

//
// PIVKeyRecord
//
PIVKeyRecord::PIVKeyRecord(const unsigned char *application, size_t applicationSize,
	const char *description, const Tokend::MetaRecord &metaRecord,
	bool signOnly) :
    PIVRecord(application, applicationSize, description),
	mSignOnly(signOnly)
{
    attributeAtIndex(metaRecord.metaAttribute(kSecKeyDecrypt).attributeIndex(),
                     new Tokend::Attribute(!signOnly));
    attributeAtIndex(metaRecord.metaAttribute(kSecKeyUnwrap).attributeIndex(),
                     new Tokend::Attribute(!signOnly));
    attributeAtIndex(metaRecord.metaAttribute(kSecKeySign).attributeIndex(),
                     new Tokend::Attribute(signOnly));
}

PIVKeyRecord::~PIVKeyRecord()
{
}

/*
	MODIFY - This is where most of the crypto functions end up, and 
	this will be the main place to actually talk with the token.
*/

void PIVKeyRecord::computeCrypt(PIVToken &pivToken, bool sign,	// MODIFY
	const unsigned char *data, size_t dataLength, unsigned char *output,
	size_t &outputLength)
{
	if (dataLength > sizeInBits() / 8)
		CssmError::throwMe(CSSMERR_CSP_BLOCK_SIZE_MISMATCH);

	if (sign != mSignOnly)
		CssmError::throwMe(CSSMERR_CSP_KEY_USAGE_INCORRECT);

	PCSC::Transaction _(pivToken);
	pivToken.select(mApplication, mApplicationSize);
	size_t apduSize = dataLength + 5;
	unsigned char apdu[apduSize];
	size_t resultLength = sizeInBits() / 8 + 2;
	unsigned char result[resultLength];

	apdu[0] = 0x80;
	apdu[1] = 0x42;
	apdu[2] = 0x00;
	apdu[3] = 0x00;
	apdu[4] = dataLength;
	memcpy(apdu + 5, data, dataLength);
	PIVError::check(pivToken.exchangeAPDU(apdu, apduSize, result,
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

void PIVKeyRecord::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	if (!mAclEntries) {
		mAclEntries.allocator(Allocator::standard());
        // Anyone can read the DB record for this key (which is a reference
		// CSSM_KEY)
		mAclEntries.add(CssmClient::AclFactory::AnySubject(
			mAclEntries.allocator()),
			AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_DB_READ, 0));
		// Using this key to sign or decrypt will require PIN1
		mAclEntries.add(CssmClient::AclFactory::PinSubject(
			mAclEntries.allocator(), 1),
			AclAuthorizationSet((mSignOnly
				? CSSM_ACL_AUTHORIZATION_SIGN
				: CSSM_ACL_AUTHORIZATION_DECRYPT), 0));
	}
	count = mAclEntries.size();
	acls = mAclEntries.entries();
}


