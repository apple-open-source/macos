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
 *  MuscleCardSchema.cpp
 *  TokendMuscle
 */

#include "MuscleCardSchema.h"

#include "MetaAttribute.h"
#include "MetaRecord.h"

#include <PCSC/musclecard.h>
#include <Security/SecCertificate.h>
#include <Security/SecKeychainItem.h>
#include <Security/SecKey.h>

using namespace Tokend;

MuscleCardSchema::MuscleCardSchema() :
	mEncryptCoder(MSC_KEYPOLICY_DIR_ENCRYPT),
	mDecryptCoder(MSC_KEYPOLICY_DIR_DECRYPT),
	mSignCoder(MSC_KEYPOLICY_DIR_SIGN),
	mVerifyCoder(MSC_KEYPOLICY_DIR_VERIFY)
{
}

MuscleCardSchema::~MuscleCardSchema()
{
}

Tokend::Relation *MuscleCardSchema::createKeyRelation(CSSM_DB_RECORDTYPE keyType)
{
	Relation *rn = createStandardRelation(keyType);

	// Set up coders for key records.
	MetaRecord &mr = rn->metaRecord();
	mr.keyHandleFactory(&mMuscleCardKeyHandleFactory);

	// Print name of a key might as well be the key name.
	mr.attributeCoder(kSecKeyPrintName, &mKeyNameCoder);

	// Other key valuess
	mr.attributeCoder(kSecKeyKeyType, &mKeyAlgorithmCoder);
	mr.attributeCoder(kSecKeyKeySizeInBits, &mKeySizeCoder);
	// @@@ Should be different for 3DES keys.
	mr.attributeCoder(kSecKeyEffectiveKeySize, &mKeySizeCoder);

	// Key attributes
	mr.attributeCoder(kSecKeyExtractable, &mKeyExtractableCoder);
	mr.attributeCoder(kSecKeySensitive, &mKeySensitiveCoder);
	mr.attributeCoder(kSecKeyModifiable, &mKeyModifiableCoder);
	mr.attributeCoder(kSecKeyPrivate, &mKeyPrivateCoder);
	// Made up since muscle doesn't tell us these.
	mr.attributeCoder(kSecKeyNeverExtractable, &mFalseCoder);
	mr.attributeCoder(kSecKeyAlwaysSensitive, &mFalseCoder);

	// Key usage
	mr.attributeCoder(kSecKeyEncrypt, &mEncryptCoder);
	mr.attributeCoder(kSecKeyDecrypt, &mDecryptCoder);
	mr.attributeCoder(kSecKeyWrap, &mEncryptCoder);
	mr.attributeCoder(kSecKeyUnwrap, &mDecryptCoder);
	mr.attributeCoder(kSecKeySign, &mSignCoder);
	mr.attributeCoder(kSecKeyVerify, &mVerifyCoder);
	// Made up since muscle doesn't tell us these.
	mr.attributeCoder(kSecKeyDerive, &mFalseCoder);
	mr.attributeCoder(kSecKeySignRecover, &mFalseCoder);
	mr.attributeCoder(kSecKeyVerifyRecover, &mFalseCoder);

	return rn;
}

void MuscleCardSchema::create()
{
	Schema::create();

	/* Relation *rn_priv = */ createKeyRelation(CSSM_DL_DB_RECORD_PRIVATE_KEY);
	Relation *rn_publ = createKeyRelation(CSSM_DL_DB_RECORD_PUBLIC_KEY);
	Relation *rn_symm = createKeyRelation(CSSM_DL_DB_RECORD_SYMMETRIC_KEY);
	Relation *rn_ce = createStandardRelation(CSSM_DL_DB_RECORD_X509_CERTIFICATE);

	// @@@ We need a coder that calculates the public key hash of a public key
	rn_publ->metaRecord().attributeCoder(kSecKeyLabel, &mZeroCoder);

	// For symmetric keys we use the object id as the label.
	rn_symm->metaRecord().attributeCoder(kSecKeyLabel, &mKeyNameCoder);

	// Set coders for certificate attributes.
	MetaRecord &mr_cert = rn_ce->metaRecord();
	mr_cert.attributeCoderForData(&mMscDataAttributeCoder);

	// Create the generic table
	// @@@ HARDWIRED @@@
    Relation *rn_gen = createStandardRelation(CSSM_DL_DB_RECORD_GENERIC);
	MetaRecord &mr_gen = rn_gen->metaRecord();
	mr_gen.attributeCoderForData(&mMscDataAttributeCoder);
	mr_gen.attributeCoder(kSecLabelItemAttr, &mObjectIDCoder);
}

