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
 *  MuscleCardToken.cpp
 *  TokendMuscle
 */

#include "MuscleCardToken.h"
#include "Adornment.h"

#include "Adornment.h"
#include "AttributeCoder.h"
#include "KeyRecord.h"
#include "TokenRecord.h"
#include "Msc/MscToken.h"
#include "Msc/MscTokenConnection.h"
#include "Msc/MscWrappers.h"
#include "MuscleCardSchema.h"
#include <security_cdsa_client/aclclient.h>
#include <map>
#include <vector>

using CssmClient::AclFactory;


MuscleCardToken::MuscleCardToken() : mConnection(NULL)
{
}

MuscleCardToken::~MuscleCardToken()
{
	delete mTokenContext;
	delete mSchema;
	delete mConnection;
}

uint32 MuscleCardToken::probe(SecTokendProbeFlags flags, char tokenUid[TOKEND_MAX_UID])
{
	MscTokenInfo tinfo(*(*startupReaderInfo)());
	MscTokenConnection tc(tinfo);
	tc.connect();
	tc.release();
	if (flags!=kSecTokendProbeDefault)
		;
	return 50;
}

void MuscleCardToken::establish(const CSSM_GUID *guid, uint32 subserviceId,
	SecTokendEstablishFlags flags, const char *cacheDirectory, const char *workDirectory,
	char mdsDirectory[PATH_MAX], char printName[PATH_MAX])
{
	MscTokenInfo tinfo(*(*startupReaderInfo)());
	mConnection = new MscTokenConnection(tinfo);
	mConnection->connect();
	::strncpy(printName, mConnection->tokenInfo.tokenName, PATH_MAX);
	mTokenContext = new MscToken(mConnection);
	static_cast<MscToken *>(mTokenContext)->loadobjects();
    mSchema = new MuscleCardSchema();
	mSchema->create();

	populate();
}

//
// Authenticate to the token
//
void MuscleCardToken::authenticate(CSSM_DB_ACCESS_TYPE mode, const AccessCredentials *cred)
{
	if (cred) {
		if (cred->tag() && !strncmp(cred->tag(), "PIN", 3)) {	// tag="PINk"; unlock a PIN
			if (cred->size() != 1)
				CssmError::throwMe(CSSM_ERRCODE_INVALID_SAMPLE_VALUE);	// just one, please
			const TypedList &sample = (*cred)[0];
			switch (sample.type()) {
			case CSSM_SAMPLE_TYPE_PASSWORD:
			case CSSM_SAMPLE_TYPE_PROMPTED_PASSWORD:
				{
					unsigned int slot;
					sscanf(cred->tag()+3, "%d", &slot);	// "PINn"
					secdebug("muscleacl", "verifying PIN%d", slot);
					mConnection->verifyPIN(slot, sample[1].toString());
					secdebug("muscleacl", "verify successful");
				}
				break;
			default:
				secdebug("muscleacl", "sample type %d not supported", sample.type());
				CssmError::throwMe(CSSM_ERRCODE_ACL_SUBJECT_TYPE_NOT_SUPPORTED);
			}
		} else
			secdebug("muscleacl", "authenticate without PIN tag ignored");
	} else
		secdebug("muscleacl", "authenticate(NULL) ignored");
}


//
// Database-level ACLs
//
void MuscleCardToken::getOwner(AclOwnerPrototype &owner)
{
	// MUSCLE defines ACLs on card initialization, but doesn't seem to allow
	// them to be read out after the card has been personalized.
	// In absence of any meaningful information, blame PIN #0.
	if (!mAclOwner) {
		mAclOwner.allocator(Allocator::standard());
		mAclOwner = AclFactory::PinSubject(Allocator::standard(), 0);
	}
	owner = mAclOwner;
}


void MuscleCardToken::getAcl(const char *tag, uint32 &count, AclEntryInfo *&acls)
{
	// we don't (yet) support queries by tag
	if (tag)
		CssmError::throwMe(CSSM_ERRCODE_INVALID_ACL_ENTRY_TAG);

	Allocator &alloc = Allocator::standard();
	// get pin list, then for each pin
	if (!mAclEntries) {
		mAclEntries.allocator(alloc);
        // Anyone can read any record from this db.
        // We don't support insertion modification or deletion yet.
        mAclEntries.add(CssmClient::AclFactory::AnySubject(mAclEntries.allocator()),
                        AclAuthorizationSet(CSSM_ACL_AUTHORIZATION_DB_READ, 0));
		// for each PIN on the card...
		unsigned int pins = mConnection->listPins();
		for (unsigned n = 0; n < 16; n++)
			if (pins & (1 << n)) {
				// add a PIN slot for PASSWORD and PROTECTED_PASSWORD credentials
				mAclEntries.addPin(AclFactory::PWSubject(alloc), n);
				mAclEntries.addPin(AclFactory::PromptPWSubject(alloc, CssmData()), n);
			}
	}

	// return the ACL vector
	count = mAclEntries.size();
	acls = mAclEntries.entries();
}


#pragma mark ---------------- CAC Specific --------------

void MuscleCardToken::populate()
{
	secdebug("populate", "MuscleCardToken::populate() begin");

	Tokend::Relation &certRelation = mSchema->findRelation(CSSM_DL_DB_RECORD_X509_CERTIFICATE);
	Tokend::Relation &dataRelation = mSchema->findRelation(CSSM_DL_DB_RECORD_GENERIC);
	Tokend::Relation &privateKeyRelation = mSchema->findRelation(CSSM_DL_DB_RECORD_PRIVATE_KEY);
	Tokend::Relation &publicKeyRelation = mSchema->findRelation(CSSM_DL_DB_RECORD_PUBLIC_KEY);
	Tokend::Relation &symmetricKeyRelation = mSchema->findRelation(CSSM_DL_DB_RECORD_SYMMETRIC_KEY);

	// Map from number to certs.
	typedef std::map< UInt32, RefPointer<Tokend::Record> > CertificateMap;
	CertificateMap certificates;

	typedef std::vector<RefPointer<KeyRecord> > KeyVector;
	KeyVector keys;

	// The first time through, we insert cert and data records. We skip attribute records
	// so that we can add them as adornments to records that will exist after this pass
	for (MscToken::ObjIterator it = static_cast<MscToken *>(mTokenContext)->begin();
		 it != static_cast<MscToken *>(mTokenContext)->end();
		 ++it)
	{
		MscObject *obj = it->second;
		std::string objid = obj->objid();

		secdebug("populate", "Found object with id: %s", objid.c_str());

		switch (objid[0])
		{
		case 'C':			// insert in cert relation
			{
				RefPointer<Tokend::Record> record(new TokenRecord(objid));
				certRelation.insertRecord(record);
				UInt32 certNum = atoi(objid.c_str() + 1);
				certificates.insert(std::pair<UInt32, RefPointer<Tokend::Record> >(certNum, record));
			}
			break;
		case 'k':			// this will become an adornment for key record
#if 0
			{
				// @@@ Move this define to a msc header
#define CKO_CAC_PRIVATE_KEY 0x03000000
				RefPointer<KeyRecord> keyRecord(new KeyRecord(*obj));
				uint32_t cka_class = keyRecord->attributeValueAsUint32(CKA_CLASS);
				switch (cka_class)
				{
				case CKO_PRIVATE_KEY:
				case CKO_CAC_PRIVATE_KEY:
					secdebug("populate", "Inserting private key with id: %s CKA_CLASS: %08X", objid.c_str(), cka_class);
					privateKeyRelation.insertRecord(keyRecord);
					keys.push_back(keyRecord);
					break;
				case CKO_PUBLIC_KEY:
				case CKO_SECRET_KEY:
				default:
					secdebug("populate", "Ignoring key with id: %s CKA_CLASS: %08X", objid.c_str(), cka_class);
					break;
				}
			}
			break;
#endif
		case 'c':			// this might become an adornment for cert record
			secdebug("populate", "Ignoring object with id: %s", objid.c_str());
			break;
		default:			// insert as data record
			{
				RefPointer<Tokend::Record> record(new TokenRecord(objid));
				dataRelation.insertRecord(record);
			}
			break;
		}
	}

	// The first time through, we insert cert and data records. We skip attribute records
	// so that we can add them as adornments to records that will exist after this pass
	for (MscToken::ConstKeyIterator it = static_cast<MscToken *>(mTokenContext)->kbegin();
		 it != static_cast<MscToken *>(mTokenContext)->kend();
		 ++it)
	{
		MscKey *key = it->second;
		IFDUMPING("key", key->debugDump());
		{
			RefPointer<KeyRecord> keyRecord(new KeyRecord(*key));
			uint32_t type = key->type();
			switch (type)
			{
			case MSC_KEY_RSA_PRIVATE:
			case MSC_KEY_RSA_PRIVATE_CRT:
			case MSC_KEY_DSA_PRIVATE:
				secdebug("populate", "Inserting private key with type: %02X", type);
				privateKeyRelation.insertRecord(keyRecord);
				keys.push_back(keyRecord);
				break;
			case MSC_KEY_RSA_PUBLIC:
			case MSC_KEY_DSA_PUBLIC:
				secdebug("populate", "Inserting public key with type: %02X", type);
				publicKeyRelation.insertRecord(keyRecord);
				keys.push_back(keyRecord);
				break;
			case MSC_KEY_DES:
			case MSC_KEY_3DES:
			case MSC_KEY_3DES3:
				secdebug("populate", "Inserting symmetric key with type: %02X", type);
				symmetricKeyRelation.insertRecord(keyRecord);
				keys.push_back(keyRecord);
				break;
			default:
				secdebug("populate", "Ignoring key with type: %02X", type);
				break;
			}
		}
	}

	for (KeyVector::const_iterator ks_it = keys.begin(); ks_it != keys.end(); ++ks_it)
	{
		UInt32 keyNum = (*ks_it)->key().number();
		CertificateMap::const_iterator cs_it = certificates.find(keyNum);
		if (cs_it == certificates.end())
		{
			secdebug("populate", "No certificate found for key: %lu", keyNum);
		}
		else
		{
			secdebug("populate", "Linked key: K%lu to certificate C%lu", keyNum, keyNum);
			(*ks_it)->setAdornment(mSchema->publicKeyHashCoder().certificateKey(),
				new Tokend::LinkedRecordAdornment(cs_it->second));
		}
	}

	secdebug("populate", "MuscleCardToken::populate() end");
}

