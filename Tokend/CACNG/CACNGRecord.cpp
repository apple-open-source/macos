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
 *  CACNGRecord.cpp
 *  TokendMuscle
 */

#include "CACNGRecord.h"

#include "CACNGError.h"
#include "CACNGToken.h"
#include "Attribute.h"
#include "MetaAttribute.h"
#include "MetaRecord.h"
#include <security_cdsa_client/aclclient.h>
#include <Security/SecKey.h>

//
// CACNGRecord
//
CACNGRecord::~CACNGRecord()
{
}


//
// CACNGCertificateRecord
//
CACNGCertificateRecord::~CACNGCertificateRecord()
{
}

Tokend::Attribute *CACNGCertificateRecord::getDataAttribute(Tokend::TokenContext *tokenContext)
{
	byte_string result = identity->read();
	
	CssmData data(malloc_copy(result), result.size());
	return new Tokend::Attribute(data.Data, data.Length);
}

//
// CACNGKeyRecord
//
CACNGKeyRecord::CACNGKeyRecord(shared_ptr<CACNGIDObject> identity, const char *description, const Tokend::MetaRecord &metaRecord, bool signOnly, bool requireNewPin /* = false */)
: CACNGRecord(description), identity(identity), mSignOnly(signOnly), requireNewPin(requireNewPin)
{
    attributeAtIndex(metaRecord.metaAttribute(kSecKeyDecrypt).attributeIndex(),
                     //new Tokend::Attribute(!signOnly));
 					 new Tokend::Attribute(true));
    attributeAtIndex(metaRecord.metaAttribute(kSecKeyUnwrap).attributeIndex(),
                     //new Tokend::Attribute(!signOnly));
					 new Tokend::Attribute(true));
    attributeAtIndex(metaRecord.metaAttribute(kSecKeySign).attributeIndex(),
                     //new Tokend::Attribute(signOnly));
 					 new Tokend::Attribute(true));
}

CACNGKeyRecord::~CACNGKeyRecord()
{
}


void CACNGKeyRecord::computeCrypt(CACNGToken &token, bool sign,
	const unsigned char *data, size_t dataLength, unsigned char *output,
	size_t &outputLength)
{
	if (requireNewPin) {
		token.verifyCachedPin(2);
	}
	byte_string result = identity->crypt(byte_string(data, data + dataLength));

	if (outputLength < result.size())
		CssmError::throwMe(CSSMERR_CSP_BLOCK_SIZE_MISMATCH);

	outputLength = result.size();
	memcpy(output, &result[0], outputLength);
}

void CACNGKeyRecord::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	// 2010.03.01 -SG- added tmptag adjusting to API change in 10.6.0	
	char tmptag[20];
	const uint32 slot = 1;	// hardwired for now, but...
	snprintf(tmptag, sizeof(tmptag), "PIN%d", slot);

	if (!mAclEntries) {
		mAclEntries.allocator(Allocator::standard());
        // Anyone can read the DB record for this key (which is a reference
		// CSSM_KEY)
		mAclEntries.add(CssmClient::AclFactory::AnySubject(
			mAclEntries.allocator()),
			AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_DB_READ, 0));
		if (requireNewPin) {
			mAclEntries.add(CssmClient::AclFactory::PinSubject(
				mAclEntries.allocator(), 2),
				AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_SIGN, CSSM_ACL_AUTHORIZATION_DECRYPT, 0), tmptag);
			if (0x9000 != token->pinStatus(2)) {
				CssmData prompt;
				mAclEntries.add(CssmClient::AclFactory::PromptPWSubject(mAclEntries.allocator(), prompt),
					AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_SIGN, CSSM_ACL_AUTHORIZATION_DECRYPT, 0), tmptag);
			}
		} else {
		// Using this key to sign or decrypt will require PIN1
			mAclEntries.add(CssmClient::AclFactory::PinSubject(
				mAclEntries.allocator(), 1),
				AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_SIGN, CSSM_ACL_AUTHORIZATION_DECRYPT, 0), tmptag);
		}
	}
	count = mAclEntries.size();
	acls = mAclEntries.entries();
}

void CACNGKeyRecord::getOwner(AclOwnerPrototype &owner)
{
	if (!mAclOwner) {
		mAclOwner.allocator(Allocator::standard());
		mAclOwner = CssmClient::AclFactory::PinSubject(Allocator::standard(), requireNewPin ? 2 : 1);
	}
	owner = mAclOwner;
}
//
// CACNGDataRecord
//
CACNGDataRecord::~CACNGDataRecord()
{
}

Tokend::Attribute *CACNGDataRecord::getDataAttribute(Tokend::TokenContext *tokenContext)
{
	byte_string data = buffer->read();
	return new Tokend::Attribute(&data[0], data.size());
}

void CACNGDataRecord::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{

	if (!mAclEntries) {
		mAclEntries.allocator(Allocator::standard());
        // Reading this objects data requires PIN1
		mAclEntries.add(CssmClient::AclFactory::PinSubject(
			mAclEntries.allocator(), 1),
			AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_DB_READ, 0));
	}
	count = mAclEntries.size();
	acls = mAclEntries.entries();
}

