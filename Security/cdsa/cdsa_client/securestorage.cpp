/*
 * Copyright (c) 2000-2001 Apple Computer, Inc. All Rights Reserved.
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


#include "securestorage.h"
#include "genkey.h"
#include "aclsupport.h"
#include <Security/osxsigning.h>
#include <memory>

using namespace CssmClient;

//
// Manage CSPDL attachments
//
CSPDLImpl::CSPDLImpl(const Guid &guid)
: CSPImpl(Cssm::standard()->autoModule(guid)),
DLImpl(CSPImpl::module())
{
}

CSPDLImpl::CSPDLImpl(const Module &module)
: CSPImpl(module),
DLImpl(module)
{
}

CSPDLImpl::~CSPDLImpl()
{
}

CssmAllocator &CSPDLImpl::allocator() const
{
	DLImpl::allocator(); return CSPImpl::allocator();
}

void CSPDLImpl::allocator(CssmAllocator &alloc)
{
	CSPImpl::allocator(alloc); DLImpl::allocator(alloc);
}

bool CSPDLImpl::operator <(const CSPDLImpl &other) const
{
	return (static_cast<const CSPImpl &>(*this) < static_cast<const CSPImpl &>(other) ||
			(!(static_cast<const CSPImpl &>(other) < static_cast<const CSPImpl &>(*this))
			   && static_cast<const DLImpl &>(*this) < static_cast<const DLImpl &>(other)));
}

bool CSPDLImpl::operator ==(const CSPDLImpl &other) const
{
	return (static_cast<const CSPImpl &>(*this) == static_cast<const CSPImpl &>(other)
			&& static_cast<const DLImpl &>(*this) == static_cast<const DLImpl &>(other));
}

CSSM_SERVICE_MASK CSPDLImpl::subserviceMask() const
{
	return CSPImpl::subserviceType() | DLImpl::subserviceType();
}

void CSPDLImpl::subserviceId(uint32 id)
{
	CSPImpl::subserviceId(id); DLImpl::subserviceId(id);
}


//
// Secure storage
//
SSCSPDLImpl::SSCSPDLImpl(const Guid &guid) : CSPDLImpl::CSPDLImpl(guid)
{
}

SSCSPDLImpl::SSCSPDLImpl(const Module &module) : CSPDLImpl::CSPDLImpl(module)
{
}

SSCSPDLImpl::~SSCSPDLImpl()
{
}

DbImpl *
SSCSPDLImpl::newDb(const char *inDbName, const CSSM_NET_ADDRESS *inDbLocation)
{
	return new SSDbImpl(SSCSPDL(this), inDbName, inDbLocation);
}


//
// SSDbImpl -- Secure Storage Database Implementation
//
SSDbImpl::SSDbImpl(const SSCSPDL &cspdl, const char *inDbName,
				   const CSSM_NET_ADDRESS *inDbLocation)
: DbImpl(cspdl, inDbName, inDbLocation)
{
}

SSDbImpl::~SSDbImpl()
{
}

void
SSDbImpl::create()
{
	DbImpl::create();
}

void
SSDbImpl::open()
{
	DbImpl::open();
}

SSDbUniqueRecord
SSDbImpl::insert(CSSM_DB_RECORDTYPE recordType,
				 const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
				 const CSSM_DATA *data,
				 const CSSM_RESOURCE_CONTROL_CONTEXT *rc)
{
	SSGroup group(SSDb(this), rc);
	const CSSM_ACCESS_CREDENTIALS *cred = rc ? rc->AccessCred : NULL;
	try
	{
		return insert(recordType, attributes, data, group, cred);
	}
	catch(...)
	{
		// @@@ Look at rc for credentials
		group->deleteKey(cred);
		throw;
	}
}

SSDbUniqueRecord
SSDbImpl::insert(CSSM_DB_RECORDTYPE recordType,
				 const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
				 const CSSM_DATA *data, const SSGroup &group,
				 const CSSM_ACCESS_CREDENTIALS *cred)
{
	// Create an encoded dataBlob for this item.
	CssmDataContainer dataBlob;
	group->encodeDataBlob(data, cred, dataBlob);

	// Insert the record with the new juicy dataBlob.
	return SSDbUniqueRecord(safe_cast<SSDbUniqueRecordImpl *>
            (&(*DbImpl::insert(recordType, attributes, &dataBlob))));
}


// DbCursorMaker
DbCursorImpl *
SSDbImpl::newDbCursor(const CSSM_QUERY &query, CssmAllocator &allocator)
{
	return new SSDbCursorImpl(Db(this), query, allocator);
}

DbCursorImpl *
SSDbImpl::newDbCursor(uint32 capacity, CssmAllocator &allocator)
{
	return new SSDbCursorImpl(Db(this), capacity, allocator);
}


// SSDbUniqueRecordMaker
DbUniqueRecordImpl *
SSDbImpl::newDbUniqueRecord()
{
	return new SSDbUniqueRecordImpl(Db(this));
}


//
// SSGroup -- Group key with acl, used to protect a group of items.
//
// @@@ Get this from a shared spot.
CSSM_DB_NAME_ATTR(SSGroupImpl::kLabel, 6, "Label", 0, NULL, BLOB);

// Create a new group.
SSGroupImpl::SSGroupImpl(const SSDb &ssDb,
						 const CSSM_RESOURCE_CONTROL_CONTEXT *credAndAclEntry)
: KeyImpl(ssDb->csp()), mLabel(ssDb->allocator())
{
	mLabel.Length = kLabelSize;
	mLabel.Data = reinterpret_cast<uint8 *>
		(mLabel.mAllocator.malloc(mLabel.Length));

	// Get our csp and set up a random number generation context.
	CSP csp(csp());
	Random random(csp, CSSM_ALGID_APPLE_YARROW);

	// Generate a kLabelSize byte random number that will be the label of
	// the key which we store in the dataBlob.
	random.generate(mLabel, mLabel.Length);

	// Overwrite the first 4 bytes with the magic cookie for a group.
	reinterpret_cast<uint32 *>(mLabel.Data)[0] = kGroupMagic;

	// @@@ Ensure that the label is unique (Chance of collision is 2^80 --
	// birthday paradox).

	// Generate a permanent 3DES key that we will use to encrypt the data.
	GenerateKey genKey(csp, CSSM_ALGID_3DES_3KEY, 192);
	genKey.database(ssDb);

	// Set the acl of the key correctly here
	genKey.initialAcl(ResourceControlContext::overlay(credAndAclEntry));

	// Generate the key
	genKey(*this, KeySpec(CSSM_KEYUSE_ENCRYPT|CSSM_KEYUSE_DECRYPT,
						  CSSM_KEYATTR_PERMANENT|CSSM_KEYATTR_SENSITIVE,
						  mLabel));

	// Activate ourself so CSSM_FreeKey will get called when we go out of
	// scope.
	activate();
}

// Lookup an existing group based on a dataBlob.
SSGroupImpl::SSGroupImpl(const SSDb &ssDb, const CSSM_DATA &dataBlob)
: KeyImpl(ssDb->csp()), mLabel(ssDb->allocator())
{
	if (dataBlob.Length < kLabelSize + kIVSize)
		CssmError::throwMe(CSSMERR_DL_RECORD_NOT_FOUND); // @@@ Not a SS record

	mLabel = CssmData(dataBlob.Data, kLabelSize);
	if (*reinterpret_cast<const uint32 *>(mLabel.Data) != kGroupMagic)
		CssmError::throwMe(CSSMERR_DL_RECORD_NOT_FOUND); // @@@ Not a SS record

	// Look up the symmetric key with that label.
	DbCursor cursor(new DbDbCursorImpl(ssDb, 0, CssmAllocator::standard()));
	cursor->recordType(CSSM_DL_DB_RECORD_SYMMETRIC_KEY);
	cursor->add(CSSM_DB_EQUAL, kLabel, mLabel);

	DbUniqueRecord keyId;
	CssmDataContainer keyData(ssDb->allocator());
	if (!cursor->next(NULL, &keyData, keyId))
		CssmError::throwMe(CSSMERR_DL_RECORD_NOT_FOUND); // @@@ The key is gone

	// Set the key part of ourself.
	static_cast<CSSM_KEY &>(*this) =
		*reinterpret_cast<const CSSM_KEY *>(keyData.Data);

	// Activate ourself so CSSM_FreeKey will get called when we go out of
	// scope.
	activate();
}

const CssmData
SSGroupImpl::label() const
{
	return mLabel;
}

void
SSGroupImpl::decodeDataBlob(const CSSM_DATA &dataBlob,
							const CSSM_ACCESS_CREDENTIALS *cred,
							CssmAllocator &allocator, CSSM_DATA &data)
{
	// First get the IV and the cipherText from the blob.
	CssmData iv(&dataBlob.Data[kLabelSize], kIVSize);
	CssmData cipherText(&dataBlob.Data[kLabelSize + kIVSize],
						dataBlob.Length - (kLabelSize + kIVSize));

	CssmDataContainer plainText1(allocator);
	CssmDataContainer plainText2(allocator);
	try
	{
		// Decrypt the data
		// @@@ Don't use staged decrypt once the AppleCSPDL can do combo
		// encryption.
		// Setup decryption context
		Decrypt decrypt(csp(), algorithm());
		decrypt.mode(CSSM_ALGMODE_CBCPadIV8);
		decrypt.padding(CSSM_PADDING_PKCS1);
		decrypt.initVector(iv);
		decrypt.key(Key(this));
		decrypt.cred(AccessCredentials::overlay(cred));
		decrypt.decrypt(&cipherText, 1, &plainText1, 1);
		decrypt.final(plainText2);
	}
	catch (const CssmError &e)
	{
		if (e.cssmError() != CSSMERR_CSP_APPLE_ADD_APPLICATION_ACL_SUBJECT)
			throw;

		// The user checked to don't ask again checkbox in the rogue app alert.  Let's edit the ACL for this key and add the calling application to it.
		KeychainACL acl(Key(this));
		acl.anyAllow(false);
		acl.alwaysAskUser(true);

		auto_ptr<CodeSigning::OSXCode> code(CodeSigning::OSXCode::main());
		const char *path = code->canonicalPath().c_str();
		CssmData comment(const_cast<char *>(path), strlen(path) + 1);
		acl.push_back(TrustedApplication(path, comment));

		// Change the acl.
		acl.commit();

		// Retry the decrypt operation.
		Decrypt decrypt(csp(), algorithm());
		decrypt.mode(CSSM_ALGMODE_CBCPadIV8);
		decrypt.padding(CSSM_PADDING_PKCS1);
		decrypt.initVector(iv);
		decrypt.key(Key(this));
		decrypt.cred(AccessCredentials::overlay(cred));
		decrypt.decrypt(&cipherText, 1, &plainText1, 1);
		decrypt.final(plainText2);
	}

	// Use DL allocator for allocating memory for data.
	uint32 length = plainText1.Length + plainText2.Length;
	data.Data = allocator.alloc<uint8>(length);
	data.Length = length;
	memcpy(data.Data, plainText1.Data, plainText1.Length);
	memcpy(&data.Data[plainText1.Length], plainText2.Data, plainText2.Length);
}

void
SSGroupImpl::encodeDataBlob(const CSSM_DATA *data,
							const CSSM_ACCESS_CREDENTIALS *cred,
							CssmDataContainer &dataBlob)
{
	// Get our csp and set up a random number generation context.
	CSP csp(csp());
	Random random(csp, CSSM_ALGID_APPLE_YARROW);

	// Encrypt data using key and encode it in a dataBlob.

	// First calculate a random IV.
	uint8 ivBuf[kIVSize];
	CssmData iv(ivBuf, kIVSize);
	random.generate(iv, kIVSize);

	// Setup encryption context
	Encrypt encrypt(csp, algorithm());
	encrypt.mode(CSSM_ALGMODE_CBCPadIV8);
	encrypt.padding(CSSM_PADDING_PKCS1);
	encrypt.initVector(iv);
	encrypt.key(Key(this));
	encrypt.cred(AccessCredentials::overlay(cred));

	// Encrypt the data
	const CssmData nothing;
	const CssmData *plainText = data ? CssmData::overlay(data) : &nothing;
	// @@@ Don't use staged encrypt once the AppleCSPDL can do combo
	// encryption.
	CssmDataContainer cipherText1, cipherText2;
	encrypt.encrypt(plainText, 1, &cipherText1, 1);
	encrypt.final(cipherText2);

	// Create a dataBlob containing the label followed by the IV followed
	// by the cipherText.
	uint32 length = (kLabelSize + kIVSize
					 + cipherText1.Length + cipherText2.Length);
	dataBlob.Data = dataBlob.mAllocator.alloc<uint8>(length);
	dataBlob.Length = length;
	memcpy(dataBlob.Data, mLabel.Data, kLabelSize);
	memcpy(&dataBlob.Data[kLabelSize], iv.Data, kIVSize);
	memcpy(&dataBlob.Data[kLabelSize + kIVSize],
		   cipherText1.Data, cipherText1.Length);
	memcpy(&dataBlob.Data[kLabelSize + kIVSize + cipherText1.Length],
		   cipherText2.Data, cipherText2.Length);
}


//
// SSDbCursorImpl -- Secure Storage Database Cursor Implementation.
//
SSDbCursorImpl::SSDbCursorImpl(const Db &db, const CSSM_QUERY &query,
							   CssmAllocator &allocator)
: DbDbCursorImpl(db, query, allocator)
{
}

SSDbCursorImpl::SSDbCursorImpl(const Db &db, uint32 capacity,
							   CssmAllocator &allocator)
: DbDbCursorImpl(db, capacity, allocator)
{
}

bool
SSDbCursorImpl::next(DbAttributes *attributes, ::CssmDataContainer *data,
					 DbUniqueRecord &uniqueId)
{
	return next(attributes, data, uniqueId, NULL);
}

bool
SSDbCursorImpl::next(DbAttributes *attributes, ::CssmDataContainer *data,
					 DbUniqueRecord &uniqueId,
					 const CSSM_ACCESS_CREDENTIALS *cred)
{
	if (!data)
		return DbDbCursorImpl::next(attributes, data, uniqueId);

	DbAttributes noAttrs, *attrs;
	attrs = attributes ? attributes : &noAttrs;

	// Get the datablob for this record
	CssmDataContainer dataBlob;
	for (;;)
	{
		if (!DbDbCursorImpl::next(attrs, &dataBlob, uniqueId))
			return false;

		// Keep going until we find a non key type record.
		CSSM_DB_RECORDTYPE rt = attrs->recordType();
		if (rt != CSSM_DL_DB_RECORD_SYMMETRIC_KEY
			&& rt != CSSM_DL_DB_RECORD_PRIVATE_KEY
			&& rt != CSSM_DL_DB_RECORD_PUBLIC_KEY)
		{
			// @@@ Check the label and if it doesn't start with the magic for a SSKey return the key.
			break;
		}
		else
		{
			// Free the key we just retrieved
			database()->csp()->freeKey(*reinterpret_cast<CssmKey *>(dataBlob.Data));
		}
	}

	// Get the group for dataBlob
	// @@@ This might fail in which case we should probably not decrypt the
	// data.
	SSGroup group(database(), dataBlob);

	// Decode the dataBlob, pass in the DL allocator.
	group->decodeDataBlob(dataBlob, cred, database()->allocator(), *data);
	return true;
}

bool
SSDbCursorImpl::nextKey(DbAttributes *attributes, Key &key,
						DbUniqueRecord &uniqueId)
{
	DbAttributes noAttrs, *attrs;
	attrs = attributes ? attributes : &noAttrs;
	CssmDataContainer keyData(database()->allocator());
	for (;;)
	{
		if (!DbDbCursorImpl::next(attrs, &keyData, uniqueId))
			return false;
		// Keep going until we find a key type record.
		CSSM_DB_RECORDTYPE rt = attrs->recordType();
		if (rt == CSSM_DL_DB_RECORD_SYMMETRIC_KEY
			|| rt == CSSM_DL_DB_RECORD_PRIVATE_KEY
			|| rt == CSSM_DL_DB_RECORD_PUBLIC_KEY)
			break;
	}

	key = Key(database()->csp(), *reinterpret_cast<CSSM_KEY *>(keyData.Data));
	return true;
}

void
SSDbCursorImpl::activate()
{
	return DbDbCursorImpl::activate();
}

void
SSDbCursorImpl::deactivate()
{
	return DbDbCursorImpl::deactivate();
}


//
// SSDbUniqueRecordImpl -- Secure Storage UniqueRecord Implementation.
//
SSDbUniqueRecordImpl::SSDbUniqueRecordImpl(const Db &db)
: DbUniqueRecordImpl(db)
{
}

SSDbUniqueRecordImpl::~SSDbUniqueRecordImpl()
{
}

void
SSDbUniqueRecordImpl::deleteRecord()
{
	deleteRecord(NULL);
}

void
SSDbUniqueRecordImpl::deleteRecord(const CSSM_ACCESS_CREDENTIALS *cred)
{
	// Get the datablob for this record
	// @@@ Fixme so we don't need to call DbUniqueRecordImpl::get
	CssmDataContainer dataBlob;
	DbUniqueRecordImpl::get(NULL, &dataBlob);

	// Get the group for dataBlob
	// @@@ This might fail in which case we should probably not decrypt the
	// data.
	SSGroup group(database(), dataBlob);

	// @@@ Use transactions.
	// Delete the record.
	DbUniqueRecordImpl::deleteRecord();
	// Delete the group
	// @@@ What if the group is shared?
	group->deleteKey(cred);
}

void
SSDbUniqueRecordImpl::modify(CSSM_DB_RECORDTYPE recordType,
							 const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
							 const CSSM_DATA *data,
							 CSSM_DB_MODIFY_MODE modifyMode)
{
	modify(recordType, attributes, data, modifyMode, NULL);
}

void
SSDbUniqueRecordImpl::modify(CSSM_DB_RECORDTYPE recordType,
							 const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
							 const CSSM_DATA *data,
							 CSSM_DB_MODIFY_MODE modifyMode,
							 const CSSM_ACCESS_CREDENTIALS *cred)
{
	if (!data)
	{
		DbUniqueRecordImpl::modify(recordType, attributes, NULL, modifyMode);
		return;
	}

	// Get the datablob for this record @@@ Fixme so we don't need to call
	// DbUniqueRecordImpl::get
	CssmDataContainer oldDataBlob;
	DbUniqueRecordImpl::get(NULL, &oldDataBlob);

	// Get the group for oldDataBlob
	// @@@ This might fail in which case we should probably not decrypt the
	// data.
	SSGroup group(database(), oldDataBlob);

	// Create a new dataBlob.
	CssmDataContainer dataBlob;
	group->encodeDataBlob(data, cred, dataBlob);
	DbUniqueRecordImpl::modify(recordType, attributes, &dataBlob, modifyMode);
}

void
SSDbUniqueRecordImpl::get(DbAttributes *attributes, ::CssmDataContainer *data)
{
	get(attributes, data, NULL);
}

void
SSDbUniqueRecordImpl::get(DbAttributes *attributes, ::CssmDataContainer *data,
						  const CSSM_ACCESS_CREDENTIALS *cred)
{
	if (!data)
	{
		DbUniqueRecordImpl::get(attributes, NULL);
		return;
	}

	// Get the datablob for this record @@@ Fixme so we don't need to call
	// DbUniqueRecordImpl::get
	CssmDataContainer dataBlob;
	DbUniqueRecordImpl::get(attributes, &dataBlob);

	// Get the group for dataBlob
	// @@@ This might fail in which case we should probably not decrypt the
	// data.
	SSGroup group(database(), dataBlob);

	// Decode the dataBlob, pass in the DL allocator.
	group->decodeDataBlob(dataBlob, cred, allocator(), *data);
}

SSGroup
SSDbUniqueRecordImpl::group()
{
	// Get the datablob for this record
	// @@@ Fixme so we don't need to call DbUniqueRecordImpl::get
	CssmDataContainer dataBlob;
	DbUniqueRecordImpl::get(NULL, &dataBlob);
	return SSGroup(database(), dataBlob);
}
