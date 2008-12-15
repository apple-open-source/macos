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

#include <algorithm> /* min, find_if */

#include "TLV.h"
#include "PIVUtilities.h"

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
	if(mAllowCaching && lastAttribute.get())
		return lastAttribute.get();
	
	byte_string data;
	
	pivToken.getDataCore(mApplication, description(), mIsCertificate, mAllowCaching, data);
	/* Tokend::Attribute creates a copy of data */
	lastAttribute.reset(new Tokend::Attribute(&data[0], data.size()));
	return lastAttribute.get();
}

//
// PIVKeyRecord
//
PIVKeyRecord::PIVKeyRecord(const unsigned char *application, size_t applicationSize,
	const char *description, const Tokend::MetaRecord &metaRecord,
	unsigned char keyRef) :
    PIVRecord(application, applicationSize, description),
	keyRef(keyRef)
{
	/* Allow all keys to decrypt, unwrap, sign */
    attributeAtIndex(metaRecord.metaAttribute(kSecKeyDecrypt).attributeIndex(),
                     new Tokend::Attribute(true));
    attributeAtIndex(metaRecord.metaAttribute(kSecKeyUnwrap).attributeIndex(),
                     new Tokend::Attribute(true));
    attributeAtIndex(metaRecord.metaAttribute(kSecKeySign).attributeIndex(),
                     new Tokend::Attribute(true));
}

PIVKeyRecord::~PIVKeyRecord()
{
}

/*
	MODIFY - This is where most of the crypto functions end up, and 
	this will be the main place to actually talk with the token.
*/

void PIVKeyRecord::computeCrypt(PIVToken &pivToken, bool sign,	// MODIFY
	const AccessCredentials *cred,
	const byte_string &data, byte_string &output)
{
	if (data.size() != sizeInBits() / 8)
		CssmError::throwMe(CSSMERR_CSP_BLOCK_SIZE_MISMATCH);

	/* Allow all key usage, certificates determine validity */
	unsigned char algRef;
	switch (sizeInBits()) {
	case 1024:
		algRef = 0x06;
		break;
	case 2048:
		algRef = 0x07;
		break;
	default:
		/* Cannot use a key ~= 1024 or 2048 bits yet */
		CssmError::throwMe(CSSMERR_CSP_KEY_USAGE_INCORRECT);
		break;
	}

	/* Build the BER-Encoded message */
	/* Template: 0x7C L { 0x82 0x00, 0x81 L data } .. 2 tag+lengths + 1 tag-0 */
	TLVList commandList;
	commandList.push_back(TLV_ref(new TLV(0x81, data)));
	commandList.push_back(TLV_ref(new TLV(0x82)));
	TLV_ref command = TLV_ref(new TLV(0x7C, commandList));

	/* TODO: Evaluate result length handling */
	/* At least enough to contain BER-TLV */
	size_t resultLength = sizeInBits() / 8;
	resultLength += 1 + TLV::encodedLength(resultLength); // RESPONSE
	resultLength += 1 + 1; // Potential empty response-tlv
	resultLength += 1 + TLV::encodedLength(resultLength); // TLV containing response
	/* Round out resultLength to a multiple of 256 */
	resultLength = resultLength + resultLength % 256 + 256;
	// Ensure that there's enough space to prevent unnecessary resizing
	output.reserve(resultLength);

	PCSC::Transaction _(pivToken);
	pivToken.selectDefault();
	/* Support for the signing key w/ user-consent pin */
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
				if (pin.Length > 0)
				{
					pivToken.verifyPIN(1, pin.Data, pin.Length);
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

	byte_string commandString = command->encode();
	PIVError::check(pivToken.exchangeChainedAPDU(0x00, 0x87, algRef, keyRef, commandString, output));

	/* DECODE 0x7C */
	TLV_ref tlv;
	try {
		tlv = TLV::parse(output);
	} catch(...) {
		secure_zero(output);
		PIVError::throwMe(SCARD_RETURNED_DATA_CORRUPTED);
	}
	secure_zero(output);
	if(tlv->getTag() != (unsigned char*)"\x7C") {
		secdebug("piv", " %s: computeCrypt: missing response tag: 0x%.2X",
				 description(), 0x7C);
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
	}
	byte_string tagData;
	try {
		TLVList list = tlv->getInnerValues();
		TLVList::const_iterator iter = find_if(list.begin(), list.end(), TagPredicate(0x82));
		if(iter != list.end())
			tagData = (*iter)->getValue();
	} catch(...) {
	}
	if(tagData.size() == 0) {
		secdebug("piv", " %s: computeCrypt: missing response value tag: 0x%.2X",
				 description(), 0x82);
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
	}

	if(tagData.size() != sizeInBits() / 8) { // Not enough data at all..
		secure_zero(tagData);
		secdebug("piv", " %s: computeCrypt: expected contained response length: %ld, got: %ld",
				 description(), sizeInBits() / 8, tagData.size());
		PCSC::Error::throwMe(SCARD_E_PROTO_MISMATCH);
	}

	output.swap(tagData);
	/* zero-out tagData */
	secure_zero(tagData);
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

		CssmData prompt;

		if(isUserConsent()) {
			mAclEntries.add(CssmClient::AclFactory::PromptPWSubject(
				mAclEntries.allocator(), prompt),
				AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_SIGN, CSSM_ACL_AUTHORIZATION_DECRYPT, 0));
		} else {
		// Using this key to sign or decrypt will require PIN1
		mAclEntries.add(CssmClient::AclFactory::PinSubject(
			mAclEntries.allocator(), 1),
				AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_SIGN, CSSM_ACL_AUTHORIZATION_DECRYPT, 0));
	}
	}
	count = mAclEntries.size();
	acls = mAclEntries.entries();
}

bool PIVKeyRecord::isUserConsent() const {
	return keyRef == PIV_KEYREF_PIV_DIGITAL_SIGNATURE;
}
