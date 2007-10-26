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
 *  MuscleCardAttributeCoder.cpp
 *  TokendMuscle
 */

#include "MuscleCardAttributeCoder.h"

#include "MetaAttribute.h"
#include "MetaRecord.h"
#include "TokenRecord.h"
#include "KeyRecord.h"
#include "Msc/MscToken.h"
#include "Msc/MscObject.h"

#include <Security/SecKeychainItem.h>
#include <security_cdsa_utilities/cssmkey.h>

using namespace Tokend;

#pragma mark ---------------- Muscle/P11 specific Coder methods --------------

//
// KeyExtractableAttributeCoder
//
KeyExtractableAttributeCoder::~KeyExtractableAttributeCoder() {}

void KeyExtractableAttributeCoder::decode(Tokend::TokenContext *tokenContext,
	const Tokend::MetaAttribute &metaAttribute, Tokend::Record &record)
{
	KeyRecord &keyRecord = dynamic_cast<KeyRecord &>(record);
	bool value = keyRecord.key().acl().read() != MSC_AUT_NONE;
	record.attributeAtIndex(metaAttribute.attributeIndex(), new Attribute(value));
}


//
// KeySensitiveAttributeCoder
//
KeySensitiveAttributeCoder::~KeySensitiveAttributeCoder() {}

void KeySensitiveAttributeCoder::decode(Tokend::TokenContext *tokenContext, const Tokend::MetaAttribute &metaAttribute,
		Tokend::Record &record)
{
	KeyRecord &keyRecord = dynamic_cast<KeyRecord &>(record);
	bool value = keyRecord.key().acl().read() == MSC_AUT_NONE;
	record.attributeAtIndex(metaAttribute.attributeIndex(), new Attribute(value));
}


//
// KeyModifiableAttributeCoder
//
KeyModifiableAttributeCoder::~KeyModifiableAttributeCoder() {}

void KeyModifiableAttributeCoder::decode(Tokend::TokenContext *tokenContext,
	const Tokend::MetaAttribute &metaAttribute, Tokend::Record &record)
{
	KeyRecord &keyRecord = dynamic_cast<KeyRecord &>(record);
	bool value = keyRecord.key().acl().write() != MSC_AUT_NONE;
	record.attributeAtIndex(metaAttribute.attributeIndex(), new Attribute(value));
}


//
// KeyPrivateAttributeCoder
//
KeyPrivateAttributeCoder::~KeyPrivateAttributeCoder() {}

void KeyPrivateAttributeCoder::decode(Tokend::TokenContext *tokenContext,
	const Tokend::MetaAttribute &metaAttribute, Tokend::Record &record)
{
	KeyRecord &keyRecord = dynamic_cast<KeyRecord &>(record);
	bool value = keyRecord.key().acl().use() != MSC_AUT_ALL;
	record.attributeAtIndex(metaAttribute.attributeIndex(), new Attribute(value));
}


//
// KeyDirectionAttributeCoder
//
KeyDirectionAttributeCoder::~KeyDirectionAttributeCoder() {}

void KeyDirectionAttributeCoder::decode(Tokend::TokenContext *tokenContext,
	const Tokend::MetaAttribute &metaAttribute, Tokend::Record &record)
{
	KeyRecord &keyRecord = dynamic_cast<KeyRecord &>(record);
	bool value = (keyRecord.key().policy().direction() & mMask);
	record.attributeAtIndex(metaAttribute.attributeIndex(), new Attribute(value));
}


//
// KeySizeAttributeCoder
//
KeySizeAttributeCoder::~KeySizeAttributeCoder() {}

void KeySizeAttributeCoder::decode(Tokend::TokenContext *tokenContext,
	const Tokend::MetaAttribute &metaAttribute, Tokend::Record &record)
{
	uint32 keySize = dynamic_cast<KeyRecord &>(record).key().size();
	record.attributeAtIndex(metaAttribute.attributeIndex(), new Attribute(keySize));
}


//
// KeyAlgorithmAttributeCoder
//
KeyAlgorithmAttributeCoder::~KeyAlgorithmAttributeCoder() {}

void KeyAlgorithmAttributeCoder::decode(Tokend::TokenContext *tokenContext,
	const Tokend::MetaAttribute &metaAttribute, Tokend::Record &record)
{
	uint32_t keyType = dynamic_cast<KeyRecord &>(record).key().type();
	uint32 algID;

    switch (keyType)
	{
	case MSC_KEY_RSA_PRIVATE:
	case MSC_KEY_RSA_PRIVATE_CRT:
	case MSC_KEY_RSA_PUBLIC:
		algID = CSSM_ALGID_RSA;
		break;

	case MSC_KEY_DSA_PRIVATE:
	case MSC_KEY_DSA_PUBLIC:
		algID = CSSM_ALGID_DSA;
		break;

	case MSC_KEY_DES:
		algID = CSSM_ALGID_DES;
		break;
	case MSC_KEY_3DES:
		// @@@ Which algid is this?
		algID = CSSM_ALGID_3DES;
		//algID = CSSM_ALGID_3DES_3KEY_EDE;
		//algID = CSSM_ALGID_3DES_2KEY_EDE;
		//algID = CSSM_ALGID_3DES_1KEY_EEE;
		//algID = CSSM_ALGID_3DES_3KEY_EEE;
		//algID = CSSM_ALGID_3DES_2KEY_EEE;
		break;
	case MSC_KEY_3DES3:
		// @@@ Which algid is this?
		algID = CSSM_ALGID_3DES_3KEY_EDE;
		//algID = CSSM_ALGID_3DES_3KEY_EEE;
		break;
	default:
		secdebug("coder", "unknown MSC_KEY_TYPE: %02X r: %p rid: %08X aid: %u", keyType,
			&record, metaAttribute.metaRecord().relationId(), metaAttribute.attributeId());
		algID = CSSM_ALGID_CUSTOM;
		break;
	}

	record.attributeAtIndex(metaAttribute.attributeIndex(), new Attribute(algID));
}


//
// KeyNameAttributeCoder
//
KeyNameAttributeCoder::~KeyNameAttributeCoder() {}

void KeyNameAttributeCoder::decode(Tokend::TokenContext *tokenContext,
	const Tokend::MetaAttribute &metaAttribute, Tokend::Record &record)
{
	MSCUChar8 keyNumber = dynamic_cast<KeyRecord &>(record).key().number();
	char buf[5];
	int used = snprintf(buf, 5, "K%u", keyNumber);
	record.attributeAtIndex(metaAttribute.attributeIndex(), new Attribute(buf, used));
}

//
// ObjectIDAttributeCoder
//
ObjectIDAttributeCoder::~ObjectIDAttributeCoder()
{
}

void ObjectIDAttributeCoder::decode(TokenContext *tokenContext, const MetaAttribute &metaAttribute, Record &record)
{	
	// fill in data with object name from MscObjectInfo
	TokenRecord &tokenRecord = dynamic_cast<TokenRecord &>(record);			
	record.attributeAtIndex(metaAttribute.attributeIndex(), new Attribute(tokenRecord.objid()));
}


//
// MscDataAttributeCoder
//
MscDataAttributeCoder::~MscDataAttributeCoder()
{
}

void MscDataAttributeCoder::decode(TokenContext *tokenContext, const MetaAttribute &metaAttribute, Record &record)
{
	TokenRecord &trec = dynamic_cast<TokenRecord &>(record);			
	MscToken &tok = dynamic_cast<MscToken &>(*tokenContext);
	MscObject &obj = tok.getObject(trec.objid());
	secdebug("dcoder", "getting object %s of size %d", trec.objid().c_str(), obj.size());
	record.attributeAtIndex(metaAttribute.attributeIndex(), new Attribute(obj.data(), obj.size()));
}

