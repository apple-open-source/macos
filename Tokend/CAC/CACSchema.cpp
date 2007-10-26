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
 *  CACSchema.cpp
 *  TokendMuscle
 */

#include "CACSchema.h"

#include "MetaAttribute.h"
#include "MetaRecord.h"

#include <Security/SecCertificate.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKey.h>

using namespace Tokend;

CACSchema::CACSchema() :
	mKeyAlgorithmCoder(uint32(CSSM_ALGID_RSA)),
	mKeySizeCoder(uint32(1024))
{
}

CACSchema::~CACSchema()
{
}

Tokend::Relation *CACSchema::createKeyRelation(CSSM_DB_RECORDTYPE keyType)
{
	Relation *rn = createStandardRelation(keyType);

	// Set up coders for key records.
	MetaRecord &mr = rn->metaRecord();
	mr.keyHandleFactory(&mCACKeyHandleFactory);

	// Print name of a key might as well be the key name.
	mr.attributeCoder(kSecKeyPrintName, &mDescriptionCoder);

	// Other key valuess
	mr.attributeCoder(kSecKeyKeyType, &mKeyAlgorithmCoder);
	mr.attributeCoder(kSecKeyKeySizeInBits, &mKeySizeCoder);
	mr.attributeCoder(kSecKeyEffectiveKeySize, &mKeySizeCoder);

	// Key attributes
	mr.attributeCoder(kSecKeyExtractable, &mFalseCoder);
	mr.attributeCoder(kSecKeySensitive, &mTrueCoder);
	mr.attributeCoder(kSecKeyModifiable, &mFalseCoder);
	mr.attributeCoder(kSecKeyPrivate, &mTrueCoder);
	mr.attributeCoder(kSecKeyNeverExtractable, &mTrueCoder);
	mr.attributeCoder(kSecKeyAlwaysSensitive, &mTrueCoder);

	// Key usage
	mr.attributeCoder(kSecKeyEncrypt, &mFalseCoder);
	mr.attributeCoder(kSecKeyWrap, &mFalseCoder);
	mr.attributeCoder(kSecKeyVerify, &mFalseCoder);
	mr.attributeCoder(kSecKeyDerive, &mFalseCoder);
	mr.attributeCoder(kSecKeySignRecover, &mFalseCoder);
	mr.attributeCoder(kSecKeyVerifyRecover, &mFalseCoder);

	return rn;
}

void CACSchema::create()
{
	Schema::create();

	createStandardRelation(CSSM_DL_DB_RECORD_X509_CERTIFICATE);
	createKeyRelation(CSSM_DL_DB_RECORD_PRIVATE_KEY);
    Relation *rn_gen = createStandardRelation(CSSM_DL_DB_RECORD_GENERIC);

	// Create the generic table
	MetaRecord &mr_gen = rn_gen->metaRecord();
	mr_gen.attributeCoderForData(&mCACDataAttributeCoder);
}

