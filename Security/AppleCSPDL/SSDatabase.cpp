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


//
// SSDatabase.cpp - Security Server database object
//
#include "SSDatabase.h"

#include "KeySchema.h"

using namespace CssmClient;
using namespace SecurityServer;

const char *const SSDatabaseImpl::DBBlobRelationName = "DBBlob";


SSDatabaseImpl::SSDatabaseImpl(ClientSession &inClientSession,
							   const CssmClient::DL &dl,
							   const char *inDbName, const CSSM_NET_ADDRESS *inDbLocation)
: Db::Impl(dl, inDbName, inDbLocation), mClientSession(inClientSession), mSSDbHandle(noDb)
{
}

SSDatabaseImpl::~SSDatabaseImpl()
{
	if (mSSDbHandle != noDb)
		mClientSession.releaseDb(mSSDbHandle);
}

SSUniqueRecord
SSDatabaseImpl::insert(CSSM_DB_RECORDTYPE recordType,
					   const CSSM_DB_RECORD_ATTRIBUTE_DATA *attributes,
					   const CSSM_DATA *data, bool)
{
	SSUniqueRecord uniqueId(SSDatabase(this));
	check(CSSM_DL_DataInsert(handle(), recordType,
							 attributes,
							 data, uniqueId));
	// Activate uniqueId so CSSM_DL_FreeUniqueRecord() gets called when it goes out of scope.
	uniqueId->activate();
	return uniqueId;
}

void
SSDatabaseImpl::lock()
{
	mClientSession.lock(dbHandle());

}

void
SSDatabaseImpl::unlock()
{
	mClientSession.unlock(dbHandle());
}

void
SSDatabaseImpl::unlock(const CSSM_DATA &password)
{
	mClientSession.unlock(dbHandle(), CssmData::overlay(password));
}

void
SSDatabaseImpl::getSettings(uint32 &outIdleTimeout, bool &outLockOnSleep)
{
	DBParameters parameters;
	mClientSession.getDbParameters(dbHandle(), parameters);
	outIdleTimeout = parameters.idleTimeout;
	outLockOnSleep = parameters.lockOnSleep;
}

void
SSDatabaseImpl::setSettings(uint32 inIdleTimeout, bool inLockOnSleep)
{
	DBParameters parameters;
	parameters.idleTimeout = inIdleTimeout;
	parameters.lockOnSleep = inLockOnSleep;
	mClientSession.setDbParameters(dbHandle(), parameters);

	// Reencode the db blob.
	CssmDataContainer dbb(allocator());
	mClientSession.encodeDb(mSSDbHandle, dbb, allocator());
	mDbBlobId->modify(DBBlobRelationID, NULL, &dbb, CSSM_DB_MODIFY_ATTRIBUTE_NONE);
}

bool
SSDatabaseImpl::isLocked()
{
	return mClientSession.isLocked(dbHandle());
}

void
SSDatabaseImpl::changePassphrase(const CSSM_ACCESS_CREDENTIALS *cred)
{
	mClientSession.changePassphrase(dbHandle(), AccessCredentials::overlay(cred));

	// Reencode the db blob.
	CssmDataContainer dbb(allocator());
	mClientSession.encodeDb(mSSDbHandle, dbb, allocator());
	mDbBlobId->modify(DBBlobRelationID, NULL, &dbb, CSSM_DB_MODIFY_ATTRIBUTE_NONE);
}

DbHandle
SSDatabaseImpl::dbHandle()
{
	activate();
	return mSSDbHandle;
}

void
SSDatabaseImpl::create(const DLDbIdentifier &dlDbIdentifier)
{
	DbImpl::create();

	try
	{
		// @@@ The CSSM_DB_SCHEMA_ATTRIBUTE_INFO and CSSM_DB_SCHEMA_INDEX_INFO
		// arguments should be optional.
		createRelation(DBBlobRelationID, DBBlobRelationName,
					0, (CSSM_DB_SCHEMA_ATTRIBUTE_INFO *)42,
					0, (CSSM_DB_SCHEMA_INDEX_INFO *)42);

		// @@@ Only iff not already in mDbInfo
		createRelation(CSSM_DL_DB_RECORD_PUBLIC_KEY, "CSSM_DL_DB_RECORD_PUBLIC_KEY",
					KeySchema::KeySchemaAttributeCount, KeySchema::KeySchemaAttributeList,
					KeySchema::KeySchemaIndexCount, KeySchema::KeySchemaIndexList);

		// @@@ Only iff not already in mDbInfo
		createRelation(CSSM_DL_DB_RECORD_PRIVATE_KEY, "CSSM_DL_DB_RECORD_PRIVATE_KEY",
					KeySchema::KeySchemaAttributeCount, KeySchema::KeySchemaAttributeList,
					KeySchema::KeySchemaIndexCount, KeySchema::KeySchemaIndexList);

		// @@@ Only iff not already in mDbInfo
		createRelation(CSSM_DL_DB_RECORD_SYMMETRIC_KEY, "CSSM_DL_DB_RECORD_SYMMETRIC_KEY",
					KeySchema::KeySchemaAttributeCount, KeySchema::KeySchemaAttributeList,
					KeySchema::KeySchemaIndexCount, KeySchema::KeySchemaIndexList);

		DBParameters dbParameters;
		memset(&dbParameters, 0, sizeof(DBParameters));
		dbParameters.idleTimeout = kDefaultIdleTimeout;
		dbParameters.lockOnSleep = kDefaultLockOnSleep;

		const AccessCredentials *cred = NULL;
		const AclEntryInput *owner = NULL;
		if (resourceControlContext())
		{
			cred = AccessCredentials::overlay(resourceControlContext()->AccessCred);
			owner = &AclEntryInput::overlay(resourceControlContext()->InitialAclEntry);
		}
		mSSDbHandle = mClientSession.createDb(dlDbIdentifier, cred, owner, dbParameters);
		CssmDataContainer dbb(allocator());
		mClientSession.encodeDb(mSSDbHandle, dbb, allocator());
		mDbBlobId = Db::Impl::insert(DBBlobRelationID, NULL, &dbb);
	}
	catch(...)
	{
		DbImpl::deleteDb();
		throw;
	}
}

void
SSDatabaseImpl::open(const DLDbIdentifier &dlDbIdentifier)
{
	Db::Impl::open();

	DbCursor cursor(SSDatabase(this));
	cursor->recordType(DBBlobRelationID);
	CssmDataContainer dbb(allocator());
	if (!cursor->next(NULL, &dbb, mDbBlobId))
		CssmError::throwMe(CSSMERR_DL_DATABASE_CORRUPT);

	mSSDbHandle = mClientSession.decodeDb(dlDbIdentifier, AccessCredentials::overlay(accessCredentials()), dbb);
}

DbUniqueRecordImpl *
SSDatabaseImpl::newDbUniqueRecord()
{
	return new SSUniqueRecordImpl(SSDatabase(this));
}


SSUniqueRecordImpl::SSUniqueRecordImpl(const SSDatabase &db)
: DbUniqueRecord::Impl(db)
{
}

SSUniqueRecordImpl::~SSUniqueRecordImpl()
{
}

SSDatabase
SSUniqueRecordImpl::database() const
{
	return parent<SSDatabase>();
}

