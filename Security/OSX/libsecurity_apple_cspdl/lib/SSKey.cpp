/*
 * Copyright (c) 2000-2001,2008,2011-2012 Apple Inc. All Rights Reserved.
 * 
 * The contents of this file constitute Original Code as defined in and are
 * subject to the Apple Public Source License Version 1.2 (the 'License').
 * You may not use this file except in compliance with the License. Please obtain
 * a copy of the License at http://www.apple.com/publicsource and read it before
 * using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESS
 * OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES, INCLUDING WITHOUT
 * LIMITATION, ANY WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT. Please see the License for the
 * specific language governing rights and limitations under the License.
 */


//
// SSKey - reference keys for the security server
//
#include "SSKey.h"

#include "SSCSPSession.h"
#include "SSCSPDLSession.h"
#include "SSDatabase.h"
#include "SSDLSession.h"
#include <security_cdsa_utilities/KeySchema.h>
#include <security_cdsa_plugin/cssmplugin.h>
#include <security_utilities/threading.h>

using namespace CssmClient;
using namespace SecurityServer;

// Constructor for a Security Server generated key.
SSKey::SSKey(SSCSPSession &session, KeyHandle keyHandle, CssmKey &ioKey,
			 SSDatabase &inSSDatabase, uint32 inKeyAttr,
			 const CssmData *inKeyLabel)
: ReferencedKey(session.mSSCSPDLSession),
mAllocator(session), mKeyHandle(keyHandle),
mClientSession(session.clientSession())
{
    StLock<Mutex> _ (mMutex); // In the constructor??? Yes. Our handlers aren't thread safe in the slightest...
	CssmKey::Header &header = ioKey.header();
	if (inKeyAttr & CSSM_KEYATTR_PERMANENT)
	{
		if (!inSSDatabase)
			CssmError::throwMe(CSSMERR_CSP_MISSING_ATTR_DL_DB_HANDLE);

		// EncodeKey and store it in the db.
		CssmDataContainer blob(mAllocator);
		clientSession().encodeKey(keyHandle, blob);

		assert(header.HeaderVersion == CSSM_KEYHEADER_VERSION);
		switch (header.KeyClass)
		{
		case CSSM_KEYCLASS_PUBLIC_KEY:
			mRecordType = CSSM_DL_DB_RECORD_PUBLIC_KEY;
			break;
		case CSSM_KEYCLASS_PRIVATE_KEY:
			mRecordType = CSSM_DL_DB_RECORD_PRIVATE_KEY;
			break;
		case CSSM_KEYCLASS_SESSION_KEY:
			mRecordType = CSSM_DL_DB_RECORD_SYMMETRIC_KEY;
			break;
		default:
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
		}

		CssmData label;
		if (inKeyLabel)
			label = *inKeyLabel;

		CssmData none;
		// We store the keys real CSP guid on disk
		CssmGuidData creatorGuid(header.CspId);
		CssmDateData startDate(header.StartDate);
		CssmDateData endDate(header.EndDate);

		DbAttributes attributes(inSSDatabase);
		attributes.recordType(mRecordType);
		attributes.add(KeySchema::KeyClass, mRecordType);
		attributes.add(KeySchema::PrintName, label);
		attributes.add(KeySchema::Alias, none);
		attributes.add(KeySchema::Permanent,
					   header.attribute(CSSM_KEYATTR_PERMANENT));
		attributes.add(KeySchema::Private,
					   header.attribute(CSSM_KEYATTR_PRIVATE));
		attributes.add(KeySchema::Modifiable,
					   header.attribute(CSSM_KEYATTR_MODIFIABLE));
		attributes.add(KeySchema::Label, label);
		attributes.add(KeySchema::ApplicationTag, none);
		attributes.add(KeySchema::KeyCreator, creatorGuid);
		attributes.add(KeySchema::KeyType, header.AlgorithmId);
		attributes.add(KeySchema::KeySizeInBits, header.LogicalKeySizeInBits);
		// @@@ Get the real effective key size.
		attributes.add(KeySchema::EffectiveKeySize, header.LogicalKeySizeInBits);
		attributes.add(KeySchema::StartDate, startDate);
		attributes.add(KeySchema::EndDate, endDate);
		attributes.add(KeySchema::Sensitive,
					   header.attribute(CSSM_KEYATTR_SENSITIVE));
		attributes.add(KeySchema::AlwaysSensitive,
					   header.attribute(CSSM_KEYATTR_ALWAYS_SENSITIVE));
		attributes.add(KeySchema::Extractable,
					   header.attribute(CSSM_KEYATTR_EXTRACTABLE));
		attributes.add(KeySchema::NeverExtractable,
					   header.attribute(CSSM_KEYATTR_NEVER_EXTRACTABLE));
		attributes.add(KeySchema::Encrypt,
					   header.useFor(CSSM_KEYUSE_ANY | CSSM_KEYUSE_ENCRYPT));
		attributes.add(KeySchema::Decrypt,
					   header.useFor(CSSM_KEYUSE_ANY | CSSM_KEYUSE_DECRYPT));
		attributes.add(KeySchema::Derive,
					   header.useFor(CSSM_KEYUSE_ANY | CSSM_KEYUSE_DERIVE));
		attributes.add(KeySchema::Sign,
					   header.useFor(CSSM_KEYUSE_ANY | CSSM_KEYUSE_SIGN));
		attributes.add(KeySchema::Verify,
					   header.useFor(CSSM_KEYUSE_ANY | CSSM_KEYUSE_VERIFY));
		attributes.add(KeySchema::SignRecover,
					   header.useFor(CSSM_KEYUSE_ANY
									 | CSSM_KEYUSE_SIGN_RECOVER));
		attributes.add(KeySchema::VerifyRecover,
					   header.useFor(CSSM_KEYUSE_ANY
									 | CSSM_KEYUSE_VERIFY_RECOVER));
		attributes.add(KeySchema::Wrap,
					   header.useFor(CSSM_KEYUSE_ANY | CSSM_KEYUSE_WRAP));
		attributes.add(KeySchema::Unwrap,
					   header.useFor(CSSM_KEYUSE_ANY | CSSM_KEYUSE_UNWRAP));

		mUniqueId = inSSDatabase->ssInsert(mRecordType, &attributes, &blob);
	}

	header.cspGuid(session.plugin.myGuid()); // Set the csp guid to me.
	makeReferenceKey(mAllocator, keyReference(), ioKey);
}

// Constructor for a key retrived from a Db.
SSKey::SSKey(SSDLSession &session, CssmKey &ioKey, SSDatabase &inSSDatabase,
			 const SSUniqueRecord &uniqueId, CSSM_DB_RECORDTYPE recordType,
			 CssmData &keyBlob)
: ReferencedKey(session.mSSCSPDLSession),
mAllocator(session.allocator()), mKeyHandle(noKey), mUniqueId(uniqueId),
mRecordType(recordType),
mClientSession(session.clientSession())
{
    StLock<Mutex> _ (mMutex);
	CssmKey::Header &header = ioKey.header();
	memset(&header, 0, sizeof(header)); // Clear key header

	if (!mUniqueId || !mUniqueId->database())
		CssmError::throwMe(CSSM_ERRCODE_INTERNAL_ERROR);

	header.HeaderVersion = CSSM_KEYHEADER_VERSION;
	switch (mRecordType)
	{
	case CSSM_DL_DB_RECORD_PUBLIC_KEY:
		header.KeyClass = CSSM_KEYCLASS_PUBLIC_KEY;
		break;
	case CSSM_DL_DB_RECORD_PRIVATE_KEY:
		header.KeyClass = CSSM_KEYCLASS_PRIVATE_KEY;
		break;
	case CSSM_DL_DB_RECORD_SYMMETRIC_KEY:
		header.KeyClass = CSSM_KEYCLASS_SESSION_KEY;
		break;
	default:
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	}

	DbAttributes attributes(mUniqueId->database());
	attributes.recordType(mRecordType);
	attributes.add(KeySchema::KeyClass);			// 0
	attributes.add(KeySchema::Permanent);			// 1
	attributes.add(KeySchema::Private);			// 2
	attributes.add(KeySchema::Modifiable);			// 3
	attributes.add(KeySchema::KeyCreator);			// 4
	attributes.add(KeySchema::KeyType);			// 5
	attributes.add(KeySchema::KeySizeInBits);		// 6
	attributes.add(KeySchema::StartDate);			// 7
	attributes.add(KeySchema::EndDate);			// 8
	attributes.add(KeySchema::Sensitive);			// 9
	attributes.add(KeySchema::AlwaysSensitive);	// 10
	attributes.add(KeySchema::Extractable);		// 11
	attributes.add(KeySchema::NeverExtractable);	// 12
	attributes.add(KeySchema::Encrypt);			// 13
	attributes.add(KeySchema::Decrypt);			// 14
	attributes.add(KeySchema::Derive);				// 15
	attributes.add(KeySchema::Sign);				// 16
	attributes.add(KeySchema::Verify);				// 17
	attributes.add(KeySchema::SignRecover);		// 18
	attributes.add(KeySchema::VerifyRecover);		// 19
	attributes.add(KeySchema::Wrap);				// 20
	attributes.add(KeySchema::Unwrap);				// 21

	mUniqueId->get(&attributes, NULL);

	// Assert that the mRecordType matches the KeyClass attribute.
	if (mRecordType != uint32(attributes[0]))
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);

	header.AlgorithmId = attributes[5]; // KeyType
    header.LogicalKeySizeInBits = attributes[6]; // KeySizeInBits

	if (attributes[1]) header.setAttribute(CSSM_KEYATTR_PERMANENT);
	if (attributes[2]) header.setAttribute(CSSM_KEYATTR_PRIVATE);
	if (attributes[3]) header.setAttribute(CSSM_KEYATTR_MODIFIABLE);
	if (attributes[9]) header.setAttribute(CSSM_KEYATTR_SENSITIVE);
	if (attributes[11]) header.setAttribute(CSSM_KEYATTR_EXTRACTABLE);
	if (attributes[10]) header.setAttribute(CSSM_KEYATTR_ALWAYS_SENSITIVE);
	if (attributes[12]) header.setAttribute(CSSM_KEYATTR_NEVER_EXTRACTABLE);

	if (attributes[13]) header.usage(CSSM_KEYUSE_ENCRYPT);
	if (attributes[14]) header.usage(CSSM_KEYUSE_DECRYPT);
	if (attributes[15]) header.usage(CSSM_KEYUSE_DERIVE);
	if (attributes[16]) header.usage(CSSM_KEYUSE_SIGN);
	if (attributes[17]) header.usage(CSSM_KEYUSE_VERIFY);
	if (attributes[18]) header.usage(CSSM_KEYUSE_SIGN_RECOVER);
	if (attributes[19]) header.usage(CSSM_KEYUSE_VERIFY_RECOVER);
	if (attributes[20]) header.usage(CSSM_KEYUSE_WRAP);
	if (attributes[21]) header.usage(CSSM_KEYUSE_UNWRAP);

	// If all usages are allowed set usage to CSSM_KEYUSE_ANY
	if (header.usage() == (CSSM_KEYUSE_ENCRYPT | CSSM_KEYUSE_DECRYPT
						   | CSSM_KEYUSE_DERIVE | CSSM_KEYUSE_SIGN
						   | CSSM_KEYUSE_VERIFY | CSSM_KEYUSE_SIGN_RECOVER
						   | CSSM_KEYUSE_VERIFY_RECOVER | CSSM_KEYUSE_WRAP
						   | CSSM_KEYUSE_UNWRAP))
		header.usage(CSSM_KEYUSE_ANY); 

	if (!attributes[7].size() || !attributes[8].size())
		CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);

    header.StartDate = attributes[7].at<CSSM_DATE>(0);
    header.EndDate = attributes[8].at<CSSM_DATE>(0);

	makeReferenceKey(mAllocator, keyReference(), ioKey);
	header.cspGuid(session.plugin.myGuid()); // Set the csp guid to me.
}

SSKey::~SSKey()
{
    StLock<Mutex> _(mMutex); // In the destructor too??? Yes. See SSCSPSession.cpp:354 for an explanation of this code's policy on threads.
	if (mKeyHandle != noKey)
		clientSession().releaseKey(mKeyHandle);
}

void
SSKey::free(const AccessCredentials *accessCred, CssmKey &ioKey,
			CSSM_BOOL deleteKey)
{
    StLock<Mutex> _(mMutex);
	freeReferenceKey(mAllocator, ioKey);
	if (deleteKey)
	{
		if (!mUniqueId || !mUniqueId->database())
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);
	
		// @@@ Evaluate accessCred against Db acl.
		// What should we do with accessCred?  Reauthenticate
		// mUniqueId->database()?
		mUniqueId->deleteRecord();
	}

	if (mKeyHandle != noKey)
	{
		clientSession().releaseKey(mKeyHandle);
		mKeyHandle = noKey;
	}
}

SecurityServer::ClientSession &
SSKey::clientSession()
{
    StLock<Mutex> _(mMutex);
	return mClientSession;
}

KeyHandle SSKey::optionalKeyHandle() const
{
    StLock<Mutex> _(mMutex);
	return mKeyHandle;
}

KeyHandle
SSKey::keyHandle()
{
    StLock<Mutex> _(mMutex);
	if (mKeyHandle == noKey)
	{
		// Deal with uninstantiated keys.
		if (!mUniqueId || !mUniqueId->database())
			CssmError::throwMe(CSSMERR_CSP_INVALID_KEY);

		CssmDataContainer blob(mAllocator);
		mUniqueId->get(NULL, &blob);
		CssmKey::Header dummyHeader; // @@@ Unused
		mKeyHandle =
			clientSession().decodeKey(mUniqueId->database().dbHandle(), blob,
									  dummyHeader);

        secinfo("SecAccessReference", "decoded a new key into handle %d [reference %ld]", mKeyHandle, keyReference());

		// @@@ Check decoded header against returned header
	}

	return  mKeyHandle;
}

//
// ACL retrieval and change operations
//
void
SSKey::getOwner(CSSM_ACL_OWNER_PROTOTYPE &owner, Allocator &allocator)
{
    StLock<Mutex> _ (mMutex);
	clientSession().getKeyOwner(keyHandle(), AclOwnerPrototype::overlay(owner),
								allocator);
}

void
SSKey::changeOwner(const AccessCredentials &accessCred,
				   const AclOwnerPrototype &newOwner)
{
    StLock<Mutex> _ (mMutex);
	clientSession().changeKeyOwner(keyHandle(), accessCred, newOwner);
	didChangeAcl();
}

void
SSKey::getAcl(const char *selectionTag, uint32 &numberOfAclInfos,
			  AclEntryInfo *&aclInfos, Allocator &allocator)
{
    StLock<Mutex> _ (mMutex);
	clientSession().getKeyAcl(keyHandle(), selectionTag, numberOfAclInfos,
							  aclInfos, allocator);
}

void
SSKey::changeAcl(const AccessCredentials &accessCred, const AclEdit &aclEdit)
{
    StLock<Mutex> _ (mMutex);
	clientSession().changeKeyAcl(keyHandle(), accessCred, aclEdit);
	didChangeAcl();
}

void
SSKey::didChangeAcl()
{
	if (mUniqueId == true)
	{
	    secinfo("keyacl", "SSKey::didChangeAcl() keyHandle: %lu updating DL entry", (unsigned long)mKeyHandle);
		// The key is persistent, make the change on disk.
		CssmDataContainer keyBlob(mAllocator);
		clientSession().encodeKey(keyHandle(), keyBlob);
		mUniqueId->modify(mRecordType, NULL, &keyBlob, CSSM_DB_MODIFY_ATTRIBUTE_NONE);
	}
	else
	{
	    secinfo("keyacl", "SSKey::didChangeAcl() keyHandle: %lu transient key no update done", (unsigned long)mKeyHandle);
	}
}
