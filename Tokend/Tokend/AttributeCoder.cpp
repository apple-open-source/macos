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
 *  AttributeCoder.cpp
 *  TokendMuscle
 */

#include "AttributeCoder.h"

#include "Attribute.h"
#include "Adornment.h"
#include "MetaAttribute.h"
#include "MetaRecord.h"
#include "Record.h"

#include <security_cdsa_utilities/cssmerrors.h>
#include <security_cdsa_utilities/cssmkey.h>
#include <Security/cssmerr.h>

#include <Security/SecKey.h>
#include <Security/SecCertificate.h>
#include <Security/SecKeychainItem.h>

namespace Tokend
{


//
// AttributeCoder
//
AttributeCoder::~AttributeCoder() {}


//
// CertificateAttributeCoder
//
CertificateAttributeCoder::~CertificateAttributeCoder() {}

void CertificateAttributeCoder::decode(TokenContext *tokenContext,
                                       const MetaAttribute &metaAttribute,
                                       Record &record)
{
	// Get the SecCertificateAdornment off record using a pointer to ourself as
	// the key
	SecCertificateAdornment &sca =
		record.adornment<SecCertificateAdornment>(this, tokenContext,
			metaAttribute, record);

	// Get the keychain item for the certificate from the record's adornment.
	SecKeychainItemRef certificate = sca.certificateItem();
	// Read the attribute with the requested attributeId from the item.
	SecKeychainAttribute ska = { metaAttribute.attributeId() };
	SecKeychainAttributeList skal = { 1, &ska };
	OSStatus status = SecKeychainItemCopyContent(certificate, NULL, &skal,
		NULL, NULL);
	if (status)
		MacOSError::throwMe(status);
	// Add the retrieved attribute as an attribute to the record.
	record.attributeAtIndex(metaAttribute.attributeIndex(),
		new Attribute(ska.data, ska.length));
	// Free the retrieved attribute.
	status = SecKeychainItemFreeContent(&skal, NULL);
	if (status)
		MacOSError::throwMe(status);

	// @@@ The code above only returns one email address.  Fix this.
}


//
// ConstAttributeCoder
//
ConstAttributeCoder::ConstAttributeCoder(uint32 value) : mValue(value) {}

ConstAttributeCoder::ConstAttributeCoder(bool value) : mValue(value ? 1 : 0) {}

ConstAttributeCoder::~ConstAttributeCoder() {}

void ConstAttributeCoder::decode(TokenContext *tokenContext,
	const MetaAttribute &metaAttribute, Record &record)
{
	record.attributeAtIndex(metaAttribute.attributeIndex(),
		new Attribute(mValue));
}


//
// GuidAttributeCoder
//
GuidAttributeCoder::GuidAttributeCoder(const CSSM_GUID &guid) : mGuid(guid) {}

GuidAttributeCoder::~GuidAttributeCoder() {}

void GuidAttributeCoder::decode(TokenContext *tokenContext,
	const MetaAttribute &metaAttribute, Record &record)
{
	record.attributeAtIndex(metaAttribute.attributeIndex(),
		new Attribute(&mGuid, sizeof(CSSM_GUID)));
}


//
// NullAttributeCoder
//
NullAttributeCoder::~NullAttributeCoder() {}

void NullAttributeCoder::decode(TokenContext *tokenContext,
	const MetaAttribute &metaAttribute, Record &record)
{
	record.attributeAtIndex(metaAttribute.attributeIndex(), new Attribute());
}


//
// ZeroAttributeCoder
//
ZeroAttributeCoder::~ZeroAttributeCoder() {}

void ZeroAttributeCoder::decode(TokenContext *tokenContext,
	const MetaAttribute &metaAttribute, Record &record)
{
	record.attributeAtIndex(metaAttribute.attributeIndex(),
		new Attribute(reinterpret_cast<const void *>(NULL), 0));
}


//
// KeyDataAttributeCoder
//
KeyDataAttributeCoder::~KeyDataAttributeCoder() {}

void KeyDataAttributeCoder::decode(TokenContext *tokenContext,
	const MetaAttribute &metaAttribute, Record &record)
{
	const MetaRecord &mr = metaAttribute.metaRecord();
	CssmKey key;
	key.header().cspGuid(Guid::overlay(gGuidAppleSdCSPDL));
	key.blobType(CSSM_KEYBLOB_REFERENCE);
	key.blobFormat(CSSM_KEYBLOB_REF_FORMAT_INTEGER);
	key.algorithm(mr.metaAttribute(kSecKeyKeyType)
		.attribute(tokenContext, record).uint32Value());
	key.keyClass(mr.metaAttribute(kSecKeyKeyClass)
		.attribute(tokenContext, record).uint32Value());
	key.header().LogicalKeySizeInBits =
		mr.metaAttribute(kSecKeyKeySizeInBits).attribute(tokenContext, record)
			.uint32Value();

	key.header().KeyAttr =
		(mr.metaAttribute(kSecKeyPermanent).attribute(tokenContext, record)
			.boolValue() ? CSSM_KEYATTR_PERMANENT : 0)
		| (mr.metaAttribute(kSecKeyPrivate).attribute(tokenContext, record)
			.boolValue() ? CSSM_KEYATTR_PRIVATE : 0)
		| (mr.metaAttribute(kSecKeyModifiable).attribute(tokenContext, record)
			.boolValue() ? CSSM_KEYATTR_MODIFIABLE : 0)
		| (mr.metaAttribute(kSecKeySensitive).attribute(tokenContext, record)
			.boolValue() ? CSSM_KEYATTR_SENSITIVE : 0)
		| (mr.metaAttribute(kSecKeyAlwaysSensitive)
			.attribute(tokenContext, record)
				.boolValue() ? CSSM_KEYATTR_ALWAYS_SENSITIVE : 0)
		| (mr.metaAttribute(kSecKeyExtractable).attribute(tokenContext, record)
			.boolValue() ? CSSM_KEYATTR_EXTRACTABLE : 0)
		| (mr.metaAttribute(kSecKeyNeverExtractable)
			.attribute(tokenContext, record)
				.boolValue() ? CSSM_KEYATTR_NEVER_EXTRACTABLE : 0);

	CSSM_KEYUSE usage =
		(mr.metaAttribute(kSecKeyEncrypt).attribute(tokenContext, record)
			.boolValue() ? CSSM_KEYUSE_ENCRYPT : 0)
		| (mr.metaAttribute(kSecKeyDecrypt).attribute(tokenContext, record)
			.boolValue() ? CSSM_KEYUSE_DECRYPT : 0)
		| (mr.metaAttribute(kSecKeySign).attribute(tokenContext, record)
			.boolValue() ? CSSM_KEYUSE_SIGN : 0)
		| (mr.metaAttribute(kSecKeyVerify).attribute(tokenContext, record)
			.boolValue() ? CSSM_KEYUSE_VERIFY : 0)
		| (mr.metaAttribute(kSecKeySignRecover).attribute(tokenContext, record)
			.boolValue() ? CSSM_KEYUSE_SIGN_RECOVER : 0)
		| (mr.metaAttribute(kSecKeyVerifyRecover)
			.attribute(tokenContext, record)
				.boolValue() ? CSSM_KEYUSE_VERIFY_RECOVER : 0)
		| (mr.metaAttribute(kSecKeyWrap).attribute(tokenContext, record)
			.boolValue() ? CSSM_KEYUSE_WRAP : 0)
		| (mr.metaAttribute(kSecKeyUnwrap).attribute(tokenContext, record)
			.boolValue() ? CSSM_KEYUSE_UNWRAP : 0)
		| (mr.metaAttribute(kSecKeyDerive).attribute(tokenContext, record)
			.boolValue() ? CSSM_KEYUSE_DERIVE : 0);
	if (usage == (CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT | CSSM_KEYUSE_SIGN
		| CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_SIGN_RECOVER
		| CSSM_KEYUSE_VERIFY_RECOVER | CSSM_KEYUSE_WRAP | CSSM_KEYUSE_UNWRAP
		| CSSM_KEYUSE_DERIVE))
		usage = CSSM_KEYUSE_ANY;

	key.header().KeyUsage = usage;

	// Dates
	mr.metaAttribute(kSecKeyStartDate).attribute(tokenContext, record)
		.getDateValue(key.header().StartDate);
	mr.metaAttribute(kSecKeyEndDate).attribute(tokenContext, record)
		.getDateValue(key.header().EndDate);

	record.attributeAtIndex(metaAttribute.attributeIndex(),
		new Attribute(&key, sizeof(key)));
}


//
// LinkedRecordAttributeCoder
//
LinkedRecordAttributeCoder::~LinkedRecordAttributeCoder() {}

void LinkedRecordAttributeCoder::decode(Tokend::TokenContext *tokenContext,
	const Tokend::MetaAttribute &metaAttribute,
	Tokend::Record &record)
{
    const Tokend::MetaAttribute *lma = NULL;
	LinkedRecordAdornment *lra = NULL;
    if (mCertificateMetaAttribute)
    {
        lma = mCertificateMetaAttribute;
        lra = record.getAdornment<LinkedRecordAdornment>(certificateKey());
    }

	if (!lra && mPublicKeyMetaAttribute)
    {
        lma = mPublicKeyMetaAttribute;
        lra = record.getAdornment<LinkedRecordAdornment>(publicKeyKey());
    }

    if (!lma || !lra)
		CssmError::throwMe(CSSMERR_DL_MISSING_VALUE);

    // Get the linked record's attribute and set it on record.
	const Attribute &attribute = lma->attribute(tokenContext, lra->record());
	record.attributeAtIndex(metaAttribute.attributeIndex(),
		new Attribute(attribute));
}


//
// DecriptionAttributeCoder
//
DescriptionAttributeCoder::~DescriptionAttributeCoder()
{
}

void DescriptionAttributeCoder::decode(TokenContext *tokenContext,
	const MetaAttribute &metaAttribute, Record &record)
{	
	record.attributeAtIndex(metaAttribute.attributeIndex(),
		new Attribute(record.description()));
}


//
// DataAttributeCoder
//
DataAttributeCoder::~DataAttributeCoder()
{
}

void DataAttributeCoder::decode(TokenContext *tokenContext,
	const MetaAttribute &metaAttribute, Record &record)
{
	record.attributeAtIndex(metaAttribute.attributeIndex(),
		record.getDataAttribute(tokenContext));
}


}	// end namespace Tokend

